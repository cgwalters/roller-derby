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
