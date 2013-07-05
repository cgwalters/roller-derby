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

#include "rd.h"
#include "util.h"
#include "libgsystem.h"

gboolean
rd_tag_one_lv (lvm_t              lvmh,
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
