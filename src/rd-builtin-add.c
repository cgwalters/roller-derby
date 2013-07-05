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
#include "rd-main.h"
#include "util.h"
#include "libgsystem.h"

gboolean
rd_builtin_add (int             argc,
                char          **argv,
                RdApp          *app,
                GCancellable   *cancellable,
                GError        **error)
{
  gboolean ret = FALSE;
  GOptionContext *context;
  const char *lvname;

  context = g_option_context_new ("LVPATH: Add logical volume LVPATH to rollback");
  g_option_context_add_group (context, rd_app_get_options (app));

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  if (argc == 0)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVAL,
                           "Must specify LVPATH");
      goto out;
    }
  lvname = argv[0];

  if (!rd_tag_one_lv (rd_app_get_lvmh (app), lvname, TRUE,
                      cancellable, error))
    goto out;

  g_print ("Added %s to rollback\n", lvname);

  ret = TRUE;
 out:
  return ret;
}
