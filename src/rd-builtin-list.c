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
  gs_transfer_out_value (out_lv_names, &ret_lv_names);
  return ret;
}

static gboolean
print_one_lv_status (RdApp          *app,
                     const char     *path,
                     GCancellable   *cancellable,
                     GError        **error)
{
  gboolean ret = FALSE;
  lvm_t lvmh;
  GHashTable *mountcache;
  lv_t lv = NULL;
  int major, minor;
  char *mount_path;
  char *mount_fs;
  glvm_cleanup_vg vg_t vg = NULL;

  lvmh = rd_app_get_lvmh (app);
  mountcache = rd_app_get_mounts (app);
          
  if (!glvm_open_vg_lv (lvmh, path, "r", 0, &vg, &lv,
                        cancellable, error))
    goto out;

  if (!glvm_get_lv_majmin (lv, &major, &minor, error))
    goto out;

  g_print ("%s\n", path);

  lookup_mount (mountcache, major, minor, &mount_path, &mount_fs);
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

gboolean
rd_builtin_list (int             argc,
                 char          **argv,
                 RdApp          *app,
                 GCancellable   *cancellable,
                 GError        **error)
{
  gboolean ret = FALSE;
  guint i;
  gs_unref_ptrarray GPtrArray *names = NULL;
  GOptionContext *context;

  context = g_option_context_new ("List current rollback state");
  g_option_context_add_group (context, rd_app_get_options (app));

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (!list_lvs_to_snapshot (rd_app_get_lvmh (app), &names, cancellable, error))
    goto out;

  if (names->len > 0)
    {
      for (i = 0; i < names->len; i++)
        {
          const char *path = names->pdata[i];
          
          if (!print_one_lv_status (app, path,
                                    cancellable, error))
            goto out;
        }
    }
  else
    {
      g_print ("No LVs tagged with 'rollback_include'; use --tag or --tag-vg to add them\n");
    }

  ret = TRUE;
 out:
  return ret;
}
