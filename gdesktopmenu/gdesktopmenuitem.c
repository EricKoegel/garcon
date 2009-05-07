/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006-2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General 
 * Public License along with this library; if not, write to the 
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gio/gio.h>

#include <gdesktopmenu/gdesktopmenuenvironment.h>
#include <gdesktopmenu/gdesktopmenuelement.h>
#include <gdesktopmenu/gdesktopmenuitem.h>



#define G_DESKTOP_MENU_ITEM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), G_TYPE_DESKTOP_MENU_ITEM, GDesktopMenuItemPrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_DESKTOP_ID,
  PROP_FILENAME,
  PROP_REQUIRES_TERMINAL,
  PROP_NO_DISPLAY,
  PROP_STARTUP_NOTIFICATION,
  PROP_NAME,
  PROP_GENERIC_NAME,
  PROP_COMMENT,
  PROP_ICON_NAME,
  PROP_COMMAND,
  PROP_TRY_EXEC,
  PROP_PATH,
};



static void         g_desktop_menu_item_class_init                      (GDesktopMenuItemClass    *klass);
static void         g_desktop_menu_item_element_init                    (GDesktopMenuElementIface *iface);
static void         g_desktop_menu_item_init                            (GDesktopMenuItem         *item);
static void         g_desktop_menu_item_finalize                        (GObject                  *object);
static void         g_desktop_menu_item_get_property                    (GObject                  *object,
                                                                         guint                     prop_id,
                                                                         GValue                   *value,
                                                                         GParamSpec               *pspec);
static void         g_desktop_menu_item_set_property                    (GObject                  *object,
                                                                         guint                     prop_id,
                                                                         const GValue             *value,
                                                                         GParamSpec               *pspec);
static const gchar *g_desktop_menu_item_get_element_name                (GDesktopMenuElement      *element);
static const gchar *g_desktop_menu_item_get_element_comment             (GDesktopMenuElement      *element);
static const gchar *g_desktop_menu_item_get_element_icon_name           (GDesktopMenuElement      *element);
static gboolean     g_desktop_menu_item_get_element_visible             (GDesktopMenuElement      *element);
static gboolean     g_desktop_menu_item_get_element_show_in_environment (GDesktopMenuElement      *element);
static gboolean     g_desktop_menu_item_get_element_no_display          (GDesktopMenuElement      *element);


struct _GDesktopMenuItemClass
{
  GObjectClass __parent__;
};

struct _GDesktopMenuItemPrivate
{
  /* Desktop file id */
  gchar    *desktop_id;

  /* Absolute filename */
  gchar    *filename;

  /* List of categories */
  GList    *categories;

  /* Whether this application requires a terminal to be started in */
  guint     requires_terminal : 1;

  /* Whether this menu item should be hidden */
  guint     no_display : 1;

  /* Whether this application supports startup notification */
  guint     supports_startup_notification : 1;

  /* Name to be displayed for the menu item */
  gchar    *name;

  /* Generic name of the menu item */
  gchar    *generic_name;

  /* Comment/description of the item */
  gchar    *comment;

  /* Command to be executed when the menu item is clicked */
  gchar    *command;

  /* TryExec value */
  gchar    *try_exec;

  /* Menu item icon name */
  gchar    *icon_name;

  /* Environments in which the menu item should be displayed only */
  gchar   **only_show_in;

  /* Environments in which the menu item should be hidden */
  gchar   **not_show_in;

  /* Working directory */
  gchar    *path;

  /* Counter keeping the number of menus which use this item. This works
   * like a reference counter and should be increased / decreased by GDesktopMenu
   * items whenever the item is added to or removed from the menu. */
  guint     num_allocated;
};

struct _GDesktopMenuItem
{
  GObject                  __parent__;

  /* < private > */
  GDesktopMenuItemPrivate *priv;
};



static GObjectClass *g_desktop_menu_item_parent_class = NULL;



GType
g_desktop_menu_item_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GInterfaceInfo element_info =
      {
        (GInterfaceInitFunc) g_desktop_menu_item_element_init,
        NULL,
        NULL,
      };

      type = g_type_register_static_simple (G_TYPE_OBJECT,
                                            "GDesktopMenuItem",
                                            sizeof (GDesktopMenuItemClass),
                                            (GClassInitFunc) g_desktop_menu_item_class_init,
                                            sizeof (GDesktopMenuItem),
                                            (GInstanceInitFunc) g_desktop_menu_item_init,
                                            0);

      g_type_add_interface_static (type, G_TYPE_DESKTOP_MENU_ELEMENT, &element_info);
    }
  
  return type;
}



static void
g_desktop_menu_item_class_init (GDesktopMenuItemClass *klass)
{
  GObjectClass *gobject_class;

  g_type_class_add_private (klass, sizeof (GDesktopMenuItemPrivate));

  /* Determine the parent type class */
  g_desktop_menu_item_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = g_desktop_menu_item_finalize;
  gobject_class->get_property = g_desktop_menu_item_get_property;
  gobject_class->set_property = g_desktop_menu_item_set_property;

  /**
   * GDesktopMenuItem:desktop-id:
   *
   * The desktop-file id of this application.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DESKTOP_ID,
                                   g_param_spec_string ("desktop-id",
                                                        "Desktop-File Id",
                                                        "Desktop-File Id of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:filename:
   *
   * The (absolute) filename of the %GDesktopMenuItem. Whenever this changes, the
   * complete file is reloaded. 
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILENAME,
                                   g_param_spec_string ("filename",
                                                        "Filename",
                                                        "Absolute filename",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:requires-terminal:
   *
   * Whether this application requires a terinal to be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_REQUIRES_TERMINAL,
                                   g_param_spec_boolean ("requires-terminal",
                                                         "Requires a terminal",
                                                         "Whether this application requires a terminal",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:no-display:
   *
   * Whether this menu item is hidden in menus.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NO_DISPLAY,
                                   g_param_spec_boolean ("no-display",
                                                         "No Display",
                                                         "Visibility state of the menu item",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:startup-notification:
   *
   * Whether this application supports startup notification.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_STARTUP_NOTIFICATION,
                                   g_param_spec_boolean ("supports-startup-notification",
                                                         "Startup notification",
                                                         "Startup notification support",
                                                         FALSE,
                                                         G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:name:
   *
   * Name of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Name",
                                                        "Name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:generic-name:
   *
   * GenericName of the application (will be displayed in menus etc.).
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_GENERIC_NAME,
                                   g_param_spec_string ("generic-name",
                                                        "Generic name",
                                                        "Generic name of the application",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:comment:
   *
   * Comment/description for the application. To be displayed e.g. in tooltips of
   * GtkMenuItems.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMENT,
                                   g_param_spec_string ("comment",
                                                        "Comment",
                                                        "Comment/description for the application",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:command:
   *
   * Command to be executed when the menu item is clicked.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        "Command",
                                                        "Application command",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:try-exec:
   *
   * TryExec value of the item's desktop entry.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_TRY_EXEC,
                                   g_param_spec_string ("try-exec",
                                                        "TryExec",
                                                        "TryExec",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:icon-name:
   *
   * Name of the icon to be displayed for this menu item.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON_NAME,
                                   g_param_spec_string ("icon-name",
                                                        "Icon name",
                                                        "Name of the application icon",
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * GDesktopMenuItem:path:
   *
   * Working directory the application should be started in.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_PATH,
                                   g_param_spec_string ("path",
                                                        "Path",
                                                        "Working directory path",
                                                        NULL,
                                                        G_PARAM_READWRITE));
}



static void
g_desktop_menu_item_element_init (GDesktopMenuElementIface *iface)
{
  iface->get_name = g_desktop_menu_item_get_element_name;
  iface->get_comment = g_desktop_menu_item_get_element_comment;
  iface->get_icon_name = g_desktop_menu_item_get_element_icon_name;
  iface->get_visible = g_desktop_menu_item_get_element_visible;
  iface->get_show_in_environment = g_desktop_menu_item_get_element_show_in_environment;
  iface->get_no_display = g_desktop_menu_item_get_element_no_display;
}



static void
g_desktop_menu_item_init (GDesktopMenuItem *item)
{
  item->priv = G_DESKTOP_MENU_ITEM_GET_PRIVATE (item);
  item->priv->desktop_id = NULL;
  item->priv->name = NULL;
  item->priv->generic_name = NULL;
  item->priv->comment = NULL;
  item->priv->filename = NULL;
  item->priv->command = NULL;
  item->priv->try_exec = NULL;
  item->priv->categories = NULL;
  item->priv->icon_name = NULL;
  item->priv->only_show_in = NULL;
  item->priv->not_show_in = NULL;
  item->priv->path = NULL;
  item->priv->num_allocated = 0;
}



static void
g_desktop_menu_item_finalize (GObject *object)
{
  GDesktopMenuItem *item = G_DESKTOP_MENU_ITEM (object);

  g_free (item->priv->desktop_id);
  g_free (item->priv->name);
  g_free (item->priv->generic_name);
  g_free (item->priv->comment);
  g_free (item->priv->filename);
  g_free (item->priv->command);
  g_free (item->priv->try_exec);
  g_free (item->priv->icon_name);

  g_strfreev (item->priv->only_show_in);
  g_strfreev (item->priv->not_show_in);
  
  g_free (item->priv->path);

  g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
  g_list_free (item->priv->categories);

  (*G_OBJECT_CLASS (g_desktop_menu_item_parent_class)->finalize) (object);
}



static void
g_desktop_menu_item_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GDesktopMenuItem *item = G_DESKTOP_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      g_value_set_string (value, g_desktop_menu_item_get_desktop_id (item));
      break;

    case PROP_FILENAME:
      g_value_set_string (value, g_desktop_menu_item_get_filename (item));
      break;

    case PROP_COMMENT:
      g_value_set_string (value, g_desktop_menu_item_get_comment (item));
      break;

    case PROP_REQUIRES_TERMINAL:
    case PROP_NO_DISPLAY:
    case PROP_STARTUP_NOTIFICATION:
    case PROP_NAME:
    case PROP_GENERIC_NAME:
    case PROP_COMMAND:
    case PROP_ICON_NAME:
    case PROP_TRY_EXEC:
      break;

    case PROP_PATH:
      g_value_set_string (value, g_desktop_menu_item_get_path (item));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
g_desktop_menu_item_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GDesktopMenuItem *item = G_DESKTOP_MENU_ITEM (object);

  switch (prop_id)
    {
    case PROP_DESKTOP_ID:
      g_desktop_menu_item_set_desktop_id (item, g_value_get_string (value));
      break;

    case PROP_FILENAME:
      g_desktop_menu_item_set_filename (item, g_value_get_string (value));
      break;

    case PROP_REQUIRES_TERMINAL:
      g_desktop_menu_item_set_requires_terminal (item, g_value_get_boolean (value));
      break;

    case PROP_NO_DISPLAY:
      g_desktop_menu_item_set_no_display (item, g_value_get_boolean (value));
      break;

    case PROP_STARTUP_NOTIFICATION:
      g_desktop_menu_item_set_supports_startup_notification (item, g_value_get_boolean (value));
      break;

    case PROP_NAME:
      g_desktop_menu_item_set_name (item, g_value_get_string (value));
      break;

    case PROP_GENERIC_NAME:
      g_desktop_menu_item_set_generic_name (item, g_value_get_string (value));
      break;

    case PROP_COMMENT:
      g_desktop_menu_item_set_comment (item, g_value_get_string (value));
      break;

    case PROP_COMMAND:
      g_desktop_menu_item_set_command (item, g_value_get_string (value));
      break;

    case PROP_TRY_EXEC:
      g_desktop_menu_item_set_try_exec (item, g_value_get_string (value));
      break;

    case PROP_ICON_NAME:
      g_desktop_menu_item_set_icon_name (item, g_value_get_string (value));
      break;

    case PROP_PATH:
      g_desktop_menu_item_set_path (item, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



GDesktopMenuItem*
g_desktop_menu_item_new (const gchar *uri)
{
  GDesktopMenuItem *item = NULL;
  GKeyFile         *rc;
  GError           *error = NULL;
  GFile            *file;
  GList            *categories = NULL;
  gboolean          terminal;
  gboolean          no_display;
  gboolean          startup_notify;
  gchar            *path;
  gchar            *name;
  gchar            *generic_name;
  gchar            *comment;
  gchar            *exec;
  gchar            *try_exec;
  gchar            *icon;
  gchar            *filename;
  gchar           **mt;
  gchar           **str_list;

  g_return_val_if_fail (uri != NULL, NULL);

  file = g_file_new_for_uri (uri);
  filename = g_file_get_path (file);
  g_object_unref (file);

  /* Return NULL if the filename is not an absolute path or if the file does not exists */
  if (G_UNLIKELY (!g_path_is_absolute (filename) || !g_file_test (filename, G_FILE_TEST_EXISTS)))
    return NULL;

  /* Try to open the .desktop file */
  rc = g_key_file_new ();
  g_key_file_load_from_file (rc, filename, G_KEY_FILE_NONE, &error);
  if (G_UNLIKELY (error != NULL))
    {
      g_error_free (error);
      return NULL;
    }

  /* Abort if the file has been marked as "deleted"/hidden */
  if (G_UNLIKELY (g_key_file_get_boolean (rc, "Desktop Entry", "Hidden", NULL)))
    {
      g_key_file_free (rc);
      return NULL;
    }

  /* Parse name, exec command and icon name */
  name = g_key_file_get_locale_string (rc, "Desktop Entry", "Name", NULL, NULL);
  generic_name = g_key_file_get_locale_string (rc, "Desktop Entry", "GenericName", NULL, NULL);
  comment = g_key_file_get_locale_string (rc, "Desktop Entry", "Comment", NULL, NULL);
  exec = g_key_file_get_string (rc, "Desktop Entry", "Exec", NULL);
  try_exec = g_key_file_get_string (rc, "Desktop Entry", "TryExec", NULL);
  icon = g_key_file_get_string (rc, "Desktop Entry", "Icon", NULL);
  path = g_key_file_get_string (rc, "Desktop Entry", "Path", NULL);

  /* Validate Name and Exec fields */
  if (G_LIKELY (exec != NULL && name != NULL && g_utf8_validate (name, -1, NULL)))
    {
      /* Determine other application properties */
      terminal = g_key_file_get_boolean (rc, "Desktop Entry", "Terminal", NULL);
      no_display = g_key_file_get_boolean (rc, "Desktop Entry", "NoDisplay", NULL);
      startup_notify = g_key_file_get_boolean (rc, "Desktop Entry", "StartupNotify", NULL) ||
                       g_key_file_get_boolean (rc, "Desktop Entry", "X-KDE-StartupNotify", NULL);

      /* Allocate a new menu item instance */
      item = g_object_new (G_TYPE_DESKTOP_MENU_ITEM, 
                           "filename", filename,
                           "command", exec, 
                           "try-exec", try_exec,
                           "name", name, 
                           "generic-name", generic_name,
                           "comment", comment,
                           "icon-name", icon, 
                           "requires-terminal", terminal, 
                           "no-display", no_display, 
                           "supports-startup-notification", startup_notify, 
                           "path", path,
                           NULL);

      /* Determine the categories this application should be shown in */
      str_list = g_key_file_get_string_list (rc, "Desktop Entry", "Categories", NULL, NULL);
      if (G_LIKELY (str_list != NULL))
        {
          for (mt = str_list; *mt != NULL; ++mt)
            {
              if (**mt != '\0')
                categories = g_list_prepend (categories, g_strdup (*mt));
            }

          /* Free list */          
          g_strfreev (str_list);

          /* Assign categories list to the menu item */
          g_desktop_menu_item_set_categories (item, categories);
        }

      /* Set the rest of the private data directly */
      item->priv->only_show_in = g_key_file_get_string_list (rc, "Desktop Entry", "OnlyShowIn", NULL, NULL);
      item->priv->not_show_in = g_key_file_get_string_list (rc, "Desktop Entry", "NotShowIn", NULL, NULL);
    }

  /* Free strings */
  g_free (name);
  g_free (generic_name);
  g_free (comment);
  g_free (exec);
  g_free (try_exec);
  g_free (icon);
  g_free (path);

  /* Close file handle */
  g_key_file_free (rc);

  return item;
}



const gchar*
g_desktop_menu_item_get_desktop_id (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->desktop_id;
}



void
g_desktop_menu_item_set_desktop_id (GDesktopMenuItem *item,
                                    const gchar      *desktop_id)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  g_return_if_fail (desktop_id != NULL);

  /* Free old desktop_id */
  if (G_UNLIKELY (item->priv->desktop_id != NULL))
    {
      /* Abort if old and new desktop_id are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->desktop_id, desktop_id) == 0))
        return;

      /* Otherwise free the old desktop_id */
      g_free (item->priv->desktop_id);
    }

  /* Assign the new desktop_id */
  item->priv->desktop_id = g_strdup (desktop_id);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "desktop_id");
}



const gchar*
g_desktop_menu_item_get_filename (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->filename;
}



void
g_desktop_menu_item_set_filename (GDesktopMenuItem *item,
                                  const gchar      *filename)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  g_return_if_fail (filename != NULL);
  g_return_if_fail (g_path_is_absolute (filename));

  /* Check if there is an old filename */
  if (G_UNLIKELY (item->priv->filename != NULL))
    {
      /* Abort if old and new filename are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->filename, filename) == 0))
        return;

      /* Otherwise free the old filename */
      g_free (item->priv->filename);
    }

  /* Assign the new filename */
  item->priv->filename = g_strdup (filename);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "filename");
}




GList*
g_desktop_menu_item_get_categories (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->categories;
}



void
g_desktop_menu_item_set_categories (GDesktopMenuItem *item,
                                    GList            *categories)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->categories != NULL))
    {
      /* Abort if lists are equal */
      if (G_UNLIKELY (item->priv->categories == categories))
        return;

      /* Free old list */
      g_list_foreach (item->priv->categories, (GFunc) g_free, NULL);
      g_list_free (item->priv->categories);
    }

  /* Assign new list */
  item->priv->categories = categories;
}



const gchar*
g_desktop_menu_item_get_command (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->command;
}



void
g_desktop_menu_item_set_command (GDesktopMenuItem *item,
                                 const gchar      *command)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->command != NULL))
    {
      /* Abort if old and new command are equal */
      if (G_UNLIKELY (command != NULL && g_utf8_collate (item->priv->command, command) == 0))
        return;

      /* Otherwise free old command */
      g_free (item->priv->command);
    }

  /* Assign new command */
  item->priv->command = g_strdup (command);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "command");
}



const gchar*
g_desktop_menu_item_get_try_exec (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->try_exec;
}



void
g_desktop_menu_item_set_try_exec (GDesktopMenuItem *item,
                                  const gchar      *try_exec)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->try_exec != NULL))
    {
      /* Abort if old and new try_exec are equal */
      if (G_UNLIKELY (try_exec != NULL && g_utf8_collate (item->priv->try_exec, try_exec) == 0))
        return;

      /* Otherwise free old try_exec */
      g_free (item->priv->try_exec);
    }

  /* Assign new try_exec */
  item->priv->try_exec = g_strdup (try_exec);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "try-exec");
}



const gchar*
g_desktop_menu_item_get_name (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->name;
}



void
g_desktop_menu_item_set_name (GDesktopMenuItem *item,
                              const gchar      *name)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->name != NULL))
    {
      /* Abort if old and new name are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->name, name) == 0))
        return;

      /* Otherwise free old name */
      g_free (item->priv->name);
    }

  /* Assign new name */
  item->priv->name = g_strdup (name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "name");
}



const gchar*
g_desktop_menu_item_get_generic_name (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->generic_name;
}



void
g_desktop_menu_item_set_generic_name (GDesktopMenuItem *item,
                                      const gchar      *generic_name)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->generic_name != NULL))
    {
      /* Abort if old and new generic name are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->generic_name, generic_name) == 0))
        return;

      /* Otherwise free old generic name */
      g_free (item->priv->generic_name);
    }

  /* Assign new generic_name */
  item->priv->generic_name = g_strdup (generic_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "generic-name");
}



const gchar*
g_desktop_menu_item_get_comment (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->comment;
}



void
g_desktop_menu_item_set_comment (GDesktopMenuItem *item,
                                 const gchar      *comment)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->comment != NULL))
    {
      /* Abort if old and new comment are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->comment, comment) == 0))
        return;

      /* Otherwise free old comment */
      g_free (item->priv->comment);
    }

  /* Assign new comment */
  item->priv->comment = g_strdup (comment);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "comment");
}



const gchar*
g_desktop_menu_item_get_icon_name (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->icon_name;
}



void        
g_desktop_menu_item_set_icon_name (GDesktopMenuItem *item,
                                   const gchar      *icon_name)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->icon_name != NULL))
    {
      /* Abort if old and new icon name are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->icon_name, icon_name) == 0))
        return;

      /* Otherwise free old icon name */
      g_free (item->priv->icon_name);
    }

  /* Assign new icon name */
  item->priv->icon_name = g_strdup (icon_name);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "icon_name");
}



const gchar*
g_desktop_menu_item_get_path (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), NULL);
  return item->priv->path;
}



void        
g_desktop_menu_item_set_path (GDesktopMenuItem *item,
                              const gchar      *path)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (G_UNLIKELY (item->priv->path != NULL))
    {
      /* Abort if old and new path are equal */
      if (G_UNLIKELY (g_utf8_collate (item->priv->path, path) == 0))
        return;

      /* Otherwise free old path */
      g_free (item->priv->path);
    }

  /* Assign new path */
  item->priv->path = g_strdup (path);

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "path");
}



gboolean
g_desktop_menu_item_requires_terminal (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);
  return item->priv->requires_terminal;
}



void        
g_desktop_menu_item_set_requires_terminal (GDesktopMenuItem *item,
                                           gboolean          requires_terminal)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->requires_terminal == requires_terminal)
    return;

  /* Assign new value */
  item->priv->requires_terminal = requires_terminal;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "requires-terminal");
}



gboolean    
g_desktop_menu_item_get_no_display (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);
  return item->priv->no_display;
}



void        
g_desktop_menu_item_set_no_display (GDesktopMenuItem *item,
                                    gboolean          no_display)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->no_display == no_display)
    return;

  /* Assign new value */
  item->priv->no_display = no_display;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "no-display");
}



gboolean    
g_desktop_menu_item_supports_startup_notification (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);
  return item->priv->supports_startup_notification;
}



void        
g_desktop_menu_item_set_supports_startup_notification (GDesktopMenuItem *item,
                                                       gboolean          supports_startup_notification)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  
  /* Abort if old and new value are equal */
  if (item->priv->supports_startup_notification == supports_startup_notification)
    return;

  /* Assign new value */
  item->priv->supports_startup_notification = supports_startup_notification;

  /* Notify listeners */
  g_object_notify (G_OBJECT (item), "supports-startup-notification");
}



gboolean
g_desktop_menu_item_has_category (GDesktopMenuItem *item,
                                  const gchar      *category)
{
  GList   *iter;
  gboolean found = FALSE;

  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);
  g_return_val_if_fail (category != NULL, FALSE);

  for (iter = item->priv->categories; iter != NULL; iter = g_list_next (iter))
    if (G_UNLIKELY (g_utf8_collate (iter->data, category) == 0))
      {
        found = TRUE;
        break;
      }

  return found;
}



gboolean
g_desktop_menu_item_get_show_in_environment (GDesktopMenuItem *item)
{
  const gchar *env;
  gboolean     show = TRUE;
  gboolean     included;
  int          i;

  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = g_desktop_menu_get_environment ();

  /* If no environment has been set, the menu item is displayed no matter what
   * OnlyShowIn or NotShowIn contain */
  if (G_UNLIKELY (env == NULL))
    return TRUE;

  /* Check if we have an OnlyShowIn OR a NotShowIn list (only one of them will be
   * there, according to the desktop entry specification) */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (item->priv->only_show_in); ++i) 
        {
          if (G_UNLIKELY (g_utf8_collate (item->priv->only_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it's not, don't show the menu item */
      if (G_LIKELY (!included))
        show = FALSE;
    }
  else if (G_UNLIKELY (item->priv->not_show_in != NULL))
    {
      /* Determine whether our environment is included in this list */
      included = FALSE;
      for (i = 0; i < g_strv_length (item->priv->not_show_in); ++i)
        {
          if (G_UNLIKELY (g_utf8_collate (item->priv->not_show_in[i], env) == 0))
            included = TRUE;
        }

      /* If it is, hide the menu item */
      if (G_UNLIKELY (included))
        show = FALSE;
    }

  return show;
}



gboolean
g_desktop_menu_item_only_show_in_environment (GDesktopMenuItem *item)
{
  const gchar *env;
  gboolean     show = FALSE;
  int          i;

  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);

  /* Determine current environment */
  env = g_desktop_menu_get_environment ();

  /* If no environment has been set, the contents of OnlyShowIn don't matter */
  if (G_LIKELY (env == NULL))
    return FALSE;

  /* Check if we have an OnlyShowIn list */
  if (G_UNLIKELY (item->priv->only_show_in != NULL))
    {
      for (i = 0; i < g_strv_length (item->priv->only_show_in); ++i)
        if (G_UNLIKELY (g_utf8_collate (item->priv->only_show_in[i], env) == 0))
          {
            show = TRUE;
            break;
          }
    }

  return show;
}



void
g_desktop_menu_item_ref (GDesktopMenuItem *item)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  /* Increment the allocation counter */
  g_desktop_menu_item_increment_allocated (item);

  /* Grab a reference on the object */
  g_object_ref (G_OBJECT (item));
}



void
g_desktop_menu_item_unref (GDesktopMenuItem *item)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  /* Decrement the reference counter */
  g_object_unref (G_OBJECT (item));
}



gint
g_desktop_menu_item_get_allocated (GDesktopMenuItem *item)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (item), FALSE);
  return item->priv->num_allocated;
}



void
g_desktop_menu_item_increment_allocated (GDesktopMenuItem *item)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));
  item->priv->num_allocated++;
}



void
g_desktop_menu_item_decrement_allocated (GDesktopMenuItem *item)
{
  g_return_if_fail (G_IS_DESKTOP_MENU_ITEM (item));

  if (item->priv->num_allocated > 0)
    item->priv->num_allocated--;
}



static const gchar*
g_desktop_menu_item_get_element_name (GDesktopMenuElement *element)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), NULL);
  return G_DESKTOP_MENU_ITEM (element)->priv->name;
}



static const gchar*
g_desktop_menu_item_get_element_comment (GDesktopMenuElement *element)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), NULL);
  return G_DESKTOP_MENU_ITEM (element)->priv->comment;
}



static const gchar*
g_desktop_menu_item_get_element_icon_name (GDesktopMenuElement *element)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), NULL);
  return G_DESKTOP_MENU_ITEM (element)->priv->icon_name;
}



gboolean
g_desktop_menu_item_get_element_visible (GDesktopMenuElement *element)
{
  GDesktopMenuItem *item;

  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), FALSE);

  item = G_DESKTOP_MENU_ITEM (element);

  if (!g_desktop_menu_item_get_show_in_environment (item))
    return FALSE;
  else if (g_desktop_menu_item_get_no_display (item))
    return FALSE;

  /* TODO Check TryExec as well */

  return TRUE;
}



gboolean
g_desktop_menu_item_get_element_show_in_environment (GDesktopMenuElement *element)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), FALSE);
  return g_desktop_menu_item_get_show_in_environment (G_DESKTOP_MENU_ITEM (element));
}



gboolean
g_desktop_menu_item_get_element_no_display (GDesktopMenuElement *element)
{
  g_return_val_if_fail (G_IS_DESKTOP_MENU_ITEM (element), FALSE);
  return g_desktop_menu_item_get_no_display (G_DESKTOP_MENU_ITEM (element));
}
