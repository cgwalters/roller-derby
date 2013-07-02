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

typedef struct {
  GCancellable *cancellable;

  lvm_t lvmh;
} RollerDerbyApp;

static RollerDerbyApp *app;

static gboolean opt_verbose; 

static GOptionEntry options[] = {
  { "verbose", 'v', 0, G_OPTION_ARG_NONE, &opt_verbose, "Print more information", NULL },
};

static void
set_error_from_lvm (GError       **error,
                    lvm_t          libh)
{
  int lvmerrno = lvm_errno (libh);
  g_set_error_literal (error, G_IO_ERROR, g_io_error_from_errno (lvmerrno),
                       g_strerror (lvmerrno));
}

int
main (int    argc,
      char **argv)
{
  GError *local_error = NULL;
  GError **error = &local_error;
  RollerDerbyApp appstruct;
  GOptionContext *context;
  struct dm_list *vgnames = NULL;
  struct lvm_str_list *strl;

  memset (&appstruct, 0, sizeof (appstruct));
  app = &appstruct;

  context = g_option_context_new ("Manage LVM rollback state");
  g_option_context_add_main_entries (context, options, NULL);

  if (!g_option_context_parse (context, &argc, &argv, error))
    goto out;

  app->lvmh = lvm_init (NULL);
  if (!app->lvmh)
    {
      set_error_from_lvm (error, app->lvmh);
      goto out;
    }

  vgnames = lvm_list_vg_names (app->lvmh);
  dm_list_iterate_items (strl, vgnames)
    {
      const char *vgname = strl->str;
      g_print ("name: %s\n", vgname);
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
