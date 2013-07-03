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

static gboolean
split_lvpath (const char *qualified_name,
              char      **out_vgname,
              char      **out_lvname,
              GError    **error)
{
  gboolean ret = FALSE;
  const char *slash = strchr (qualified_name, '/');

  if (!slash)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "Invalid argument '%s' - expected VGNAME/LVNAME",
                   qualified_name);
      goto out;
    }

  ret = TRUE;
  *out_vgname = g_strndup (qualified_name, slash - qualified_name);
  *out_lvname = g_strdup (slash+1);
 out:
  return ret;
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
  vg_t vg = NULL;
  lv_t lv;

  if (!split_lvpath (path, &vgname, &lvname, error))
    goto out;

  vg = lvm_vg_open (lvmh, vgname, "w", 0);
  if (vg == NULL)
    {
      glvm_set_error (error, lvmh);
      goto out;
    }

  lv = lvm_lv_from_name (vg, lvname);
  if (lv == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "No such LV '%s'", path);
      goto out;
    }
  
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
  if (vg)
    lvm_vg_close (vg);

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
          const char *name = names->pdata[i];
          g_print ("%s\n", name);
        }
    }
  else
    {
      g_print ("No LVs tagged with 'rollback_include'; use --tag or --tag-vg to add them\n");
    }

 out:
  if (app->lvmh)
    lvm_quit (app->lvmh);
  if (local_error != NULL)
    {
      g_printerr ("%s\n", local_error->message);
      g_error_free (local_error);
      return 1;
    }
  return 0;
}
