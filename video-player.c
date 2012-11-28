/*
 * video-player.c
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

#include "video-player.h"

struct _VideoPlayer
{
  gint ref_count;

  GstElement *playbin;
  GstElement *video_sink;
  ClutterActor *texture;

  guint duration;
  guint bus_src_id;

  gint64 marker;
  guint marker_src_id;

  VideoPlayerLoadCb load_cb;
  VideoPlayerEndCb end_cb;
  VideoPlayerMarkerCb marker_cb;
  gpointer user_data;
};

static gboolean
bus_watch_func (GstBus *bus, GstMessage *message, gpointer user_data)
{
  VideoPlayer *self = user_data;

  if (GST_MESSAGE_SRC (message) != GST_OBJECT (self->playbin))
    return TRUE;

  if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_STATE_CHANGED)
    {
      GstState oldstate;
      GstState newstate;
      GstState pending;

      gst_message_parse_state_changed (message, &oldstate, &newstate, &pending);

      if (newstate == GST_STATE_PAUSED && pending == GST_STATE_PLAYING)
        {
          if (self->load_cb)
            self->load_cb (self, self->user_data);
        }
    }
  else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_EOS)
    {
      video_player_set_state (self, GST_STATE_NULL);

      if (self->end_cb)
        self->end_cb (self, self->user_data);
    }
  else if (GST_MESSAGE_TYPE (message) == GST_MESSAGE_ERROR)
    {
      GError *error = NULL;
      gchar *debug_msg = NULL;

      gst_message_parse_error (message, &error, &debug_msg);
      g_print ("BUS ERROR: %s, %s\n", error->message, debug_msg);

      g_error_free (error);
      g_free (debug_msg);
    }

  return TRUE;
}

static gboolean
on_marker (gpointer user_data)
{
  VideoPlayer *self = user_data;
  GstStateChangeReturn ret;
  GstState state;

  self->marker_src_id = 0;
  self->marker = 0;

  ret = gst_element_get_state (self->playbin,
                               &state,
                               NULL,
                               GST_CLOCK_TIME_NONE);

  if (ret != GST_STATE_CHANGE_SUCCESS || state != GST_STATE_PLAYING)
    return FALSE;

  if (self->marker_cb)
    self->marker_cb (self, self->user_data);

  return FALSE;
}

static void
video_player_free (VideoPlayer *self)
{
  ClutterActor *parent;

  if (self->marker_src_id != 0)
    g_source_remove (self->marker_src_id);

  gst_element_set_state (self->playbin, GST_STATE_NULL);
  g_source_remove (self->bus_src_id);

  parent = clutter_actor_get_parent (self->texture);
  if (parent != NULL)
    {
      clutter_actor_remove_child (clutter_actor_get_parent (self->texture),
                                  self->texture);
    }
  g_object_unref (self->texture);

  g_object_unref (self->playbin);

  g_slice_free (VideoPlayer, self);
}

/* public methods */

VideoPlayer *
video_player_new (ClutterActor        *stage,
                  VideoPlayerLoadCb    load_cb,
                  VideoPlayerEndCb     end_cb,
                  VideoPlayerMarkerCb  marker_cb,
                  gpointer             user_data)
{
  VideoPlayer *self;
  GstBus *bus;

  self = g_slice_new0 (VideoPlayer);

  self->ref_count = 1;

  self->load_cb = load_cb;
  self->end_cb = end_cb;
  self->marker_cb = marker_cb;
  self->user_data = user_data;

  /* playbin */
  self->playbin = gst_element_factory_make ("playbin2", "playbin2");

  bus = gst_element_get_bus (self->playbin);
  self->bus_src_id = gst_bus_add_watch (bus, bus_watch_func, self);

  /* video texture */
  self->texture = clutter_texture_new ();
  clutter_actor_set_opacity (self->texture, 0);
  clutter_actor_set_size (self->texture, 1920, 1080);
  g_object_ref (self->texture);

  /* video sink */
  self->video_sink = gst_element_factory_make ("cluttersink", "cluttersink");
  g_object_set (self->video_sink,
                "texture", self->texture,
                NULL);
  g_object_set (self->playbin,
                "video-sink", self->video_sink,
                NULL);

  return self;
}

void
video_player_set_uri (VideoPlayer *self, const gchar *uri)
{
  g_object_set (self->playbin,
                "uri", uri,
                NULL);
}

void
video_player_set_state (VideoPlayer *self, GstState state)
{
  gst_element_set_state (self->playbin, state);
}

GstState
video_player_get_state (VideoPlayer *self)
{
  GstStateChangeReturn ret;
  GstState state;

  ret = gst_element_get_state (self->playbin,
                               &state,
                               NULL,
                               GST_CLOCK_TIME_NONE);

  if (ret == GST_STATE_CHANGE_SUCCESS)
    return state;
  else
    return GST_STATE_NULL;
}

gboolean
video_player_set_marker (VideoPlayer *self, guint marker)
{
  GstFormat format = GST_FORMAT_TIME;
  gint64 pos;
  guint pos_msec;

  if (self->marker_src_id != 0)
    {
      g_source_remove (self->marker_src_id);
      self->marker_src_id = 0;
      self->marker = 0;
    }

  if (marker == 0)
    return TRUE;

  if (gst_element_query_position (self->playbin, &format, &pos))
    {
      pos_msec = (guint) pos / 1000000;

      if (marker < pos_msec)
        {
          return FALSE;
        }
      else
        {
          self->marker = marker;

          self->marker_src_id =
            g_timeout_add (marker - pos_msec, on_marker, self);

          return TRUE;
        }
    }
  else
    {
      return FALSE;
    }
}

guint
video_player_get_duration (VideoPlayer *self)
{
  gint64 duration = 0;
  GstFormat format = GST_FORMAT_TIME;

  gst_element_query_duration (self->playbin, &format, &duration);

  self->duration = (guint) (duration / 1000000);

  return self->duration;
}

ClutterActor *
video_player_get_texture (VideoPlayer *self)
{
  return self->texture;
}

void
video_player_ref (VideoPlayer *self)
{
  g_return_if_fail (self->ref_count > 0);

  self->ref_count++;
}

void
video_player_unref (VideoPlayer *self)
{
  g_return_if_fail (self->ref_count > 0);

  self->ref_count--;

  if (self->ref_count == 0)
    video_player_free (self);
}
