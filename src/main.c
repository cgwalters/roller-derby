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
#include <libdevmapper.h>
#include <lvm2app.h>
#include <lvm2cmd.h>
#include <string.h>

#include "util.h"
#include "glvm.h"
#include "libgsystem.h"

typedef struct {
  GCancellable *cancellable;

  lvm_t lvmh;
  GHashTable  *mountdata;
} RollerDerbyApp;

static RollerDerbyApp *app;

static gboolean opt_verbose; 
static char **opt_tag_vg;
static char **opt_untag_vg;
static char **opt_tag_lv;
static char **opt_untag_lv;

static GOptionEntry options[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Print more information", NULL },
  { "tag-vg", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_tag_vg, "Enable snapshotting for all LVs in volume group VG", "VG" },
  { "untag-vg", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_untag_vg, "Disable snapshotting for all LVs in volume group VG", "VG" },
  { "tag", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_tag_lv, "Enable snapshotting for given logical volume LV", "LV" },
  { "untag", 0, 0, G_OPTION_ARG_STRING_ARRAY, &opt_untag_lv, "Disable snapshotting for given logical volume LV", "LV" },
};

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

static void
lookup_mount (GHashTable   *mountcache,
              gint          major,
              gint          minor,
              char        **path,
              char        **filesystem)
{
  gs_free char *key = g_strdup_printf ("%d:%d", major, minor);
  char **lines = g_hash_table_lookup (mountcache, key);

  if (!lines)
    *path = *filesystem = NULL;
  else
    {
      *path = lines[4];
      *filesystem = lines[8];
    }
}

static gboolean
tag_list_includes_rollback (struct dm_list    *tags)
{
  struct lvm_str_list *tagl;

  dm_list_iterate_items (tagl, tags)
    {
      const char *tag = tagl->str;
      if (strcmp (tag, "rollback_include") == 0)
        {
          return TRUE;
        }
    }
  return FALSE;
}

static gboolean
list_lvs_to_snapshot (lvm_t              lvmh,
                      GPtrArray        **out_lv_names,
                      GCancellable      *cancellable,
                      GError           **error)
{
  gboolean ret = FALSE;
  gs_unref_ptrarray GPtrArray *ret_lv_names = NULL;
  struct dm_list *vgnames = NULL;
  struct lvm_str_list *strl;

  ret_lv_names = g_ptr_array_new_with_free_func (g_free);
  
  vgnames = lvm_list_vg_names (lvmh);
  dm_list_iterate_items (strl, vgnames)
    {
      struct dm_list *tags;
      struct dm_list *lvs;
      struct lvm_lv_list *lvsl;
      gboolean include_entire_vg = FALSE;
      const char *vgname = strl->str;
      vg_t vg = lvm_vg_open (lvmh, vgname, "r", 0);

      if (vg == NULL)
        {
          glvm_set_error (error, lvmh);
          goto out;
        }

      tags = lvm_vg_get_tags (vg);
      include_entire_vg = tag_list_includes_rollback (tags);

      lvs = lvm_vg_list_lvs (vg);
      dm_list_iterate_items (lvsl, lvs)
        {
          lv_t lv = lvsl->lv;
          gboolean matches;
          const char *lvname = lvm_lv_get_name (lv);
          
          if (include_entire_vg)
            {
              matches = TRUE;
            }
          else
            {
              tags = lvm_lv_get_tags (lv);
              matches = tag_list_includes_rollback (tags);
            }

          if (matches)
            g_ptr_array_add (ret_lv_names, g_strdup_printf ("%s/%s", vgname, lvname));
        }
      
      if (lvm_vg_close (vg) == -1)
        {
          glvm_set_error (error, lvmh);
          goto out;
        }
    }

  ret = TRUE;
 out:
  rd_transfer_out_value (out_lv_names, &ret_lv_names);
  return ret;
}

static gboolean
tag_one_lv (lvm_t              lvmh,
            const char        *path,
            gboolean           do_tag,
            GCancellable      *cancellable,
            GError           **error)
{
  gboolean ret = FALSE;
  gs_free char *vgname = NULL;
  gs_free char *lvname = NULL;
  glvm_cleanup_vg vg_t vg = NULL;
  lv_t lv;

  if (!glvm_open_vg_lv (lvmh, path, "w", 0, &vg, &lv,
                        cancellable, error))
    goto out;

  if (do_tag)
    {
      if (lvm_lv_add_tag (lv, "rollback_include") == -1)
        {
          glvm_set_error (error, lvmh);
          goto out;
        }
    }
  else
    {
      if (lvm_lv_remove_tag (lv, "rollback_include") == -1)
        {
          glvm_set_error (error, lvmh);
          goto out;
        }
    }

  if (lvm_vg_write (vg) == -1)
    {
      glvm_set_error (error, lvmh);
      goto out;
    }

  ret = TRUE;
 out:
  return ret;
}

static gboolean
print_one_lv_status (lvm_t           lvmh,
                     GHashTable     *mountdata,
                     const char     *path,
                     GCancellable   *cancellable,
                     GError        **error)
{
  gboolean ret = FALSE;
  lv_t lv = NULL;
  int major, minor;
  char *mount_path;
  char *mount_fs;
  glvm_cleanup_vg vg_t vg = NULL;
          
  if (!glvm_open_vg_lv (app->lvmh, path, "r", 0, &vg, &lv,
                        cancellable, error))
    goto out;

  if (!glvm_get_lv_majmin (lv, &major, &minor, error))
    goto out;

  g_print ("%s\n", path);

  lookup_mount (mountdata, major, minor, &mount_path, &mount_fs);
  if (mount_path == NULL)
    g_print ("  (not mounted)\n");
  else
    {
      g_print ("  mounted: %s\n", mount_path);
      g_print ("  fs: %s\n", mount_fs);
    }


  ret = TRUE;
 out:
  return ret;
}

int
main (int    argc,
      char **argv)
{
  GCancellable *cancellable = NULL;
  GError *local_error = NULL;
  GError **error = &local_error;
  RollerDerbyApp appstruct;
  guint i;
  gs_unref_ptrarray GPtrArray *names = NULL;
  GOptionContext *context;
  char **iter;

  g_setenv ("GIO_USE_VFS", "local", TRUE);

  memset (&appstruct, 0, sizeof (appstruct));
  app = &appstruct;

  context = g_option_context_new ("Manage LVM rollback state");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  app->lvmh = lvm_init (NULL);
  if (!app->lvmh)
    {
      glvm_set_error (error, app->lvmh);
      goto out;
    }

  app->mountdata = parse_mounts_indexed_by_majmin (error);
  if (!app->mountdata)
    goto out;

  if (opt_tag_lv)
    {
      for (iter = opt_tag_lv; *iter; iter++)
        {
          const char *lvname = *iter;
          if (!tag_one_lv (app->lvmh, lvname, TRUE,
                           cancellable, error))
            goto out;
        }
    }

  if (opt_untag_lv)
    {
      for (iter = opt_untag_lv; *iter; iter++)
        {
          const char *lvname = *iter;
          if (!tag_one_lv (app->lvmh, lvname, FALSE,
                           cancellable, error))
            goto out;
        }
    }

  if (!list_lvs_to_snapshot (app->lvmh, &names, cancellable, error))
    goto out;

  if (names->len > 0)
    {
      for (i = 0; i < names->len; i++)
        {
          const char *path = names->pdata[i];
          
          if (!print_one_lv_status (app->lvmh, app->mountdata, path,
                                    cancellable, error))
            goto out;
        }
    }
  else
    {
      g_print ("No LVs tagged with 'rollback_include'; use --tag or --tag-vg to add them\n");
    }

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
