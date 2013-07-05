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
#include <errno.h>

#include "glvm.h"
#include "libgsystem.h"

void
glvm_set_error (GError       **error,
		lvm_t          lvmh)
{
  int lvmerrno = lvm_errno (lvmh);
  g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (lvmerrno),
                       g_strerror (lvmerrno));
}

void
glvm_cleanup_vg_impl (void *loc)
{
  vg_t vg = *((vg_t*)loc);
  if (vg != NULL)
    lvm_vg_close (vg);
}

gboolean
glvm_split_lvpath (const char *qualified_name,
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

gboolean
glvm_lookup_lv (vg_t            vg,
                const char     *lvname,
                lv_t           *out_lv,
                GCancellable   *cancellable,
                GError        **error)
{
  gboolean ret = FALSE;
  lv_t ret_lv = lvm_lv_from_name (vg, lvname);

  if (ret_lv == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                   "No such LV '%s/%s'", lvm_vg_get_name (vg), lvname);
      goto out;
    }

  ret = TRUE;
  *out_lv = ret_lv;
 out:
  return ret;
}

gboolean
glvm_open_vg_lv (lvm_t           lvmh,
                 const char     *path,
                 const char     *mode,
                 int             flags,
                 vg_t           *out_vg,
                 lv_t           *out_lv,
                 GCancellable   *cancellable,
                 GError        **error)
{
  gboolean ret = FALSE;
  lv_t ret_lv = NULL;
  glvm_cleanup_vg vg_t ret_vg = NULL;
  gs_free char *vgname = NULL;
  gs_free char *lvname = NULL;

  if (!glvm_split_lvpath (path, &vgname, &lvname, error))
    goto out;

  ret_vg = lvm_vg_open (lvmh, vgname, mode, flags);
  if (ret_vg == NULL)
    {
      glvm_set_error (error, lvmh);
      goto out;
    }

  if (!glvm_lookup_lv (ret_vg, lvname, &ret_lv,
                       cancellable, error))
    goto out;
  
  ret = TRUE;
  gs_transfer_out_value (out_vg, &ret_vg);
  gs_transfer_out_value (out_lv, &ret_lv);
 out:
  return ret;
}

gboolean
glvm_get_uint64_property (lv_t                  lv,
                          const char           *propname,
                          guint64              *out_value,
                          GError              **error)
{
  gboolean ret = FALSE;
  struct lvm_property_value propval;

  propval = lvm_lv_get_property (lv, propname);
  if (!propval.is_valid)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "Invalid LVM property '%s'", propname);
      goto out;
    }
  if (!propval.is_integer)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "LVM property '%s' is not an integer", propname);
      goto out;
    }

  ret = TRUE;
  *out_value = propval.value.integer;
 out:
  return ret;
}

gboolean
glvm_get_string_property (lv_t                  lv,
                          const char           *propname,
                          char                **out_value,
                          GError              **error)
{
  gboolean ret = FALSE;
  struct lvm_property_value propval;

  propval = lvm_lv_get_property (lv, propname);
  if (!propval.is_valid)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "Invalid LVM property '%s'", propname);
      goto out;
    }
  if (!propval.is_string)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "LVM property '%s' is not a string", propname);
      goto out;
    }

  ret = TRUE;
  *out_value = g_strdup (propval.value.string);
 out:
  return ret;
}

gboolean
glvm_get_lv_majmin (lv_t        lv,
                    gint       *out_major,
                    gint       *out_minor,
                    GError    **error)
{
  gboolean ret = FALSE;
  gs_free char *path = NULL;
  struct stat stbuf;
  int res;

  if (!glvm_get_string_property (lv, "lv_path", &path, error))
    goto out;

  do
    res = stat (path, &stbuf);
  while (res == -1 && errno == EINTR);
  if (res < 0)
    {
      int errsv = errno;
      g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (errsv),
                           g_strerror (errsv));
      goto out;
    }
  
  if (!S_ISBLK (stbuf.st_mode))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                   "Not a device: %s", path);
      goto out;
    }
  
  ret = TRUE;
  *out_major = major (stbuf.st_rdev);
  *out_minor = minor (stbuf.st_rdev);
 out:
  return ret;
}
