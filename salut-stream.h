/*
 * salut-stream.h
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

#ifndef __SALUT_STREAM_H__
#define __SALUT_STREAM_H__

#include <gfreenect.h>
#include <skeltrack.h>
#include "salut.h"

typedef struct _SalutStream SalutStream;
typedef struct _BufferInfo BufferInfo;

typedef enum {
  SALUT_STREAM_NO_PERSON,
  SALUT_STREAM_HAS_PERSON
} SalutStreamStatus;

struct _SalutStream
{
  GFreenectDevice *device;
  SkeltrackSkeleton *skeleton;
  Salut *salut;

  guint depth_threshold;
  BufferInfo *buffer_info;

  SalutStreamStatus status;
  gint lookup_interval;
  gint64 last_skeleton_lookup_attempt;
  gint64 last_skeleton_lookup_successful_attempt;

  gboolean can_detect_gesture;
  gboolean tracking;

  /* callbacks */
  void (*person_entered_scene_cb) (SalutStream *stream, gpointer data);
  void (*person_left_scene_cb) (SalutStream *stream, gpointer data);
  gpointer person_entered_scene_cb_data;
  gpointer person_left_scene_cb_data;

  guint depth_frame_check_src_id;
};

void salut_stream_new (void (*callback) (SalutStream *, gpointer),
                       gpointer user_data);

void salut_stream_free (SalutStream *self);

void salut_stream_set_person_lookup_seconds (SalutStream *self, gint msecs);

void salut_stream_set_depth_threshold (SalutStream *self, guint threshold);

void salut_stream_set_person_entered_cb (SalutStream *self,
                                         void (*person_entered_cb) (SalutStream *, gpointer),
                                         gpointer data);

void salut_stream_set_person_left_cb (SalutStream *self,
                                      void (*person_left_cb) (SalutStream *, gpointer),
                                      gpointer data);

void salut_stream_start (SalutStream *self);

void salut_stream_stop (SalutStream *self);

void salut_stream_set_can_detect_gesture (SalutStream *self,
                                          gboolean can_detect_gesture);

#endif /* __SALUT_STREAM_H__ */
