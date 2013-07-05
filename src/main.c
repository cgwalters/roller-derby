/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2013 Colin Walters <walters@verbum.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gio/gio.h>
#include <string.h>

#include "rd-main.h"
#include "libgsystem.h"

static gboolean handle_opt_version (const gchar    *option_name,
				    const gchar    *value,
				    gpointer        data,
				    GError        **error);

static GOptionEntry app_options[] = {
  { "version", 0, 0, G_OPTION_ARG_CALLBACK, handle_opt_version, "Show version", NULL },
  { NULL }
};

struct _RdApp {
  GCancellable *cancellable;

  lvm_t lvmh;
  GHashTable  *mountdata;
  GOptionGroup *optgroup;
};

static RdBuiltin builtins[] = {
  { "list", rd_builtin_list, 0 },
  { "add", rd_builtin_add, 0 },
  { "remove", rd_builtin_remove, 0 },
#if 0
  { "add-vg", rd_builtin_add_vg, 0 },
  { "remove-vg", rd_builtin_remove_vg, 0 },
  { "snapshot", rd_builtin_snapshot, 0 },
#endif
  { NULL }
};

static RdApp *app;

static char **
parse_file_lines (GFile           *path,
                  GError         **error)
{
  gs_free char *contents;
  
  contents = gs_file_load_contents_utf8 (path, NULL, error);
  if (!contents)
    return NULL;

  return g_strsplit (contents, "\n", -1);
}

static GHashTable *
parse_mounts_indexed_by_majmin (GError           **error)
{
  gs_unref_object GFile *mountinfo = g_file_new_for_path ("/proc/self/mountinfo");
  GHashTable *ret;
  char **lines;
  char **iter;

  lines = parse_file_lines (mountinfo, error);
  if (!lines)
    return NULL;

  ret = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_strfreev);

  for (iter = lines; *iter; iter++)
    {
      char **components = g_strsplit (*iter, " ", -1);
      if (g_strv_length (components) < 8)
        {
          g_strfreev (components);
          continue;
        }
      g_hash_table_insert (ret, g_strdup (components[2]), components);
    }

  return ret;
}

lvm_t
rd_app_get_lvmh (RdApp *app)
{
  return app->lvmh;
}

GOptionGroup *
rd_app_get_options (RdApp *app)
{
  if (!app->optgroup)
    {
      app->optgroup = g_option_group_new ("roller-derby",
                                          "Options for roller-derby",
                                          "Show all roller-derby options", NULL, NULL);
      g_option_group_add_entries (app->optgroup, app_options);
    }
  return app->optgroup;
}

GHashTable *
rd_app_get_mounts (RdApp   *self)
{
  if (!self->mountdata)
    {
      GError *local_error = NULL;
      self->mountdata = parse_mounts_indexed_by_majmin (&local_error);
      if (local_error)
        {
          g_printerr ("Internal Error: %s\n", local_error->message);
          exit (1);
        }
    }

  return self->mountdata;
}

static void
usage (void) G_GNUC_NORETURN;

static void
usage (void)
{
  RdBuiltin *biter = builtins;
  g_print ("Usage: roller-derby BUILTIN\n");
  g_print ("builtins:\n");
  while (biter->name)
    {
      g_print ("  %s\n", biter->name);
      biter++;
    }
  exit (1);
}

static gboolean
handle_opt_version (const gchar    *option_name,
                    const gchar    *value,
                    gpointer        data,
                    GError        **error)
{
  g_print ("roller-derby %s\n", PACKAGE_VERSION);
  exit (0);
}

int
main (int    argc,
      char **argv)
{
  GCancellable *cancellable = NULL;
  GError *local_error = NULL;
  GError **error = &local_error;
  RdApp appstruct;
  gs_unref_ptrarray GPtrArray *names = NULL;
  GOptionContext *context;
  RdBuiltin *biter = builtins;
  const char *builtin_name;

  /* http://bugzilla.gnome.org/show_bug.cgi?id=526454 */
  g_setenv ("GIO_USE_VFS", "local", TRUE);

  g_type_init ();

  memset (&appstruct, 0, sizeof (appstruct));
  app = &appstruct;

  context = g_option_context_new ("Manage LVM rollback state");
  g_option_context_add_group (context, rd_app_get_options (app));

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (argc < 2)
    usage ();

  builtin_name = argv[1];
  while (biter->name && strcmp (builtin_name, biter->name) != 0)
    biter++;

  if (!biter->name)
    usage ();

  app->lvmh = lvm_init (NULL);
  if (!app->lvmh)
    {
      glvm_set_error (error, app->lvmh);
      goto out;
    }

  if (!biter->func (argc - 2, argv + 2, app, cancellable, error))
    goto out;
  
 out:
  if (app->lvmh)
    lvm_quit (app->lvmh);
  if (app->mountdata)
    g_hash_table_unref (app->mountdata);
  if (local_error != NULL)
    {
      g_printerr ("%s\n", local_error->message);
      g_error_free (local_error);
      return 1;
    }
  return 0;
}
