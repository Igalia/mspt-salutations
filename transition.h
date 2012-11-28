/*
 * transition.h
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

#ifndef __TRANSITION_H__
#define __TRANSITION_H__

#include <gst/gst.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _Transition Transition;

typedef void (* TransitionFinishCb) (Transition *self, gpointer user_data);

Transition *          transition_new             (ClutterActor       *stage,
                                                  guint               duration,
                                                  TransitionFinishCb  finish_cb,
                                                  gpointer            user_data);
void                  transition_free            (Transition *self);

void                  transition_set_next_video  (Transition  *self,
                                                  const gchar *uri);

void                  transition_preload_video   (Transition  *self,
                                                  const gchar *url,
                                                  guint        transition_duration);

G_END_DECLS

#endif /* __TRANSITION_H__ */
