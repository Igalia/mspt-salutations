/*
 * salut.h
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

#ifndef __SALUT_H__
#define __SALUT_H__

#include <skeltrack-joint.h>
#include <glib.h>
#include <opencv2/imgproc/imgproc_c.h>
#include <opencv2/highgui/highgui_c.h>


typedef enum
{
  NONE,
  BOW,
  KISS,
  CURTSY,
  HAND_WAVE,
  HAND_EAST_COAST,
  HAND_METAL,
  HAND_INDIAN,
  TOTAL_GESTURES
} GestId;

typedef struct
{
  GestId gest_id;
  void (*callback) (gpointer data);
  gpointer callback_data;

  SkeltrackJoint **gesture_list;
  gint gesture_index;
  gint gesture_list_length;
} Salut;

typedef struct
{
  CvSeq *defects;
  guint box_x;
  guint box_y;
  guint box_width;
  guint box_height;
} HandData;

Salut*  salut_new                     (void);

void    salut_set_gesture_to_track    (Salut *self,
                                       GestId id,
                                       void (*callback) (gpointer),
                                       gpointer callback_data);

void    salut_set_track_data          (Salut *self,
                                       guint16 *depth,
                                       guint width,
                                       guint height,
                                       SkeltrackJointList list);

#endif /* __SALUT_H */
