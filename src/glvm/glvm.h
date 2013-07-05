/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 *
 * Copyright (C) 2011,2013 Colin Walters <walters@verbum.org>
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

#pragma once

#include <gio/gio.h>
#include <lvm2app.h>

G_BEGIN_DECLS

void glvm_cleanup_vg_impl (void *loc);
#define glvm_cleanup_vg __attribute__ ((cleanup(glvm_cleanup_vg_impl)))

void glvm_set_error (GError       **error,
		     lvm_t          lvmh);

gboolean glvm_split_lvpath (const char *qualified_name,
			    char      **out_vgname,
			    char      **out_lvname,
			    GError    **error);

gboolean glvm_lookup_lv (vg_t           vg,
			 const char    *lvname,
			 lv_t          *out_lv,
			 GCancellable  *cancellable,
			 GError       **error);

gboolean glvm_open_vg_lv (lvm_t           lvmh,
			  const char     *path,
			  const char     *mode,
			  int             flags,
			  vg_t           *out_vg,
			  lv_t           *out_lv,
			  GCancellable   *cancellable,
			  GError        **error);

gboolean glvm_get_string_property (lv_t                  lv,
				   const char           *propname,
				   char                **out_value,
				   GError              **error);

gboolean glvm_get_uint64_property (lv_t                  lv,
				   const char           *propname,
				   guint64              *out_value,
				   GError              **error);

gboolean glvm_get_lv_majmin (lv_t        lv,
			    gint       *out_major,
			    gint       *out_minor,
			    GError    **error);

G_END_DECLS
