/*
 * main.c
 *
 * MSPT Salutations, interactive installation
 *
 * Copyright (C) 2012, Igalia S.L.
 *
 * Authors:
 *   Joaquim Rocha <jrocha@igalia.com>
 *   Eduardo Lima Mitev <elima@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 3, or (at your option) any later version as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Affero General Public License at http://www.gnu.org/licenses/gpl.html
 * for more details.
 */

#include <gst/gst.h>
#include <clutter/clutter.h>

#include "storyboard.h"

static Storyboard *storyboard = NULL;

gint
main (gint argc, gchar *argv[])
{
  ClutterInitError clutter_init_error;
  const gchar *snippets_path;

  g_print ("MSPT Salutations\n");
  g_print ("Copyright (C) 2012, Igalia Interactivity <http://www.igalia.com/interactivity>\n");

  /* check for required first argument (path to snippets) */
  if (argc < 2)
    {
      g_print ("\nUsage: %s <absolute-path-to-video-snippets>\n\n", argv[0]);
      return -1;
    }

  /* init */
  gst_init (&argc, &argv);

  clutter_init_error = clutter_init (&argc, &argv);
  if (clutter_init_error != CLUTTER_INIT_SUCCESS)
    {
      g_print ("Error initializing clutter\n");
      return -1;
    }

  snippets_path = argv[1];

  /* storyboard */
  storyboard = storyboard_new (snippets_path);

  /* start the show */
  clutter_main ();

  /* free stuff */
  storyboard_free (storyboard);

  return 0;
}
