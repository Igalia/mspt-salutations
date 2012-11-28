/*
 * video-player.h
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

#ifndef __VIDEO_PLAYER_H__
#define __VIDEO_PLAYER_H__

#include <gst/gst.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

typedef struct _VideoPlayer VideoPlayer;

typedef void (* VideoPlayerLoadCb) (VideoPlayer *self, gpointer user_data);
typedef void (* VideoPlayerEndCb) (VideoPlayer *self, gpointer user_data);
typedef void (* VideoPlayerMarkerCb) (VideoPlayer *self, gpointer user_data);


VideoPlayer *         video_player_new             (ClutterActor        *stage,
                                                    VideoPlayerLoadCb    load_cb,
                                                    VideoPlayerEndCb     end_cb,
                                                    VideoPlayerMarkerCb  marker_cb,
                                                    gpointer             user_data);

void                  video_player_set_uri         (VideoPlayer *self,
                                                    const gchar *uri);
void                  video_player_set_state       (VideoPlayer *self,
                                                    GstState     state);
GstState              video_player_get_state       (VideoPlayer *self);
gboolean              video_player_set_marker      (VideoPlayer *self,
                                                    guint        marker);
guint                 video_player_get_duration    (VideoPlayer *self);
ClutterActor *        video_player_get_texture     (VideoPlayer *self);

void                  video_player_ref             (VideoPlayer *self);
void                  video_player_unref           (VideoPlayer *self);

G_END_DECLS

#endif /* __VIDEO_PLAYER_H__ */
