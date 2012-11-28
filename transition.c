/*
 * transition.c
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

#include "transition.h"

#include "video-player.h"

struct _Transition
{
  ClutterActor *stage;
  VideoPlayer *current;
  VideoPlayer *next;

  TransitionFinishCb finish_cb;
  gpointer user_data;

  guint duration;

  gchar *next_video;

  GHashTable *preloads;
};

typedef struct
{
  gchar *url;
  VideoPlayer *player;
  guint transition_duration;
} Preload;

static void      roll_transition            (Transition *self);
static void      video_player_on_marker     (VideoPlayer *player,
                                             gpointer     user_data);

static void
free_preload (gpointer _data)
{
  Preload *preload = _data;

  video_player_unref (preload->player);
  g_slice_free (Preload, preload);
}

static void
video_player_on_load (VideoPlayer *player, gpointer user_data)
{
  Transition *self = user_data;
  guint duration;

  if (video_player_get_state (player) == GST_STATE_PLAYING)
    {
      duration = video_player_get_duration (player);
      video_player_set_marker (player, duration - self->duration);
    }
}

static void
video_player_on_end (VideoPlayer *player, gpointer user_data)
{
  Transition *self = user_data;

  video_player_unref (self->next);
  self->next = NULL;
}

static void
video_player_on_marker (VideoPlayer *player, gpointer user_data)
{
  Transition *self = user_data;

  if (self->finish_cb != NULL)
    self->finish_cb (self, self->user_data);

  roll_transition (self);
}

static void
roll_transition (Transition *self)
{
  ClutterActor *next_tex;
  guint duration;
  VideoPlayer *temp;

  Preload *preload;

  /* get next video player */
  preload = g_hash_table_lookup (self->preloads, self->next_video);
  g_assert (preload != NULL);

  self->next = preload->player;
  video_player_ref (self->next);

  g_hash_table_remove (self->preloads, self->next_video);
  g_free (self->next_video);
  self->next_video = NULL;

  next_tex = video_player_get_texture (self->next);
  clutter_actor_set_opacity (next_tex, 0);
  clutter_actor_add_child (self->stage, next_tex);
  clutter_actor_animate (next_tex,
                         CLUTTER_EASE_IN_OUT_SINE,
                         self->duration,
                         "opacity", 255,
                         NULL);

  /*
  ClutterActor *current_tex;
  current_tex = video_player_get_texture (self->current);
  clutter_actor_animate (current_tex,
                         CLUTTER_EASE_IN_OUT_SINE,
                         self->duration,
                         "opacity", 0,
                         NULL);
  */

  duration = video_player_get_duration (self->next);
  video_player_set_marker (self->next, duration - self->duration);
  video_player_set_state (self->next, GST_STATE_PLAYING);

  /* swap players */
  temp = self->current;
  self->current = self->next;
  self->next = temp;
}

/* public methods */

Transition *
transition_new (ClutterActor       *stage,
                guint               duration,
                TransitionFinishCb  finish_cb,
                gpointer            user_data)
{
  Transition *self;

  self = g_slice_new0 (Transition);

  self->stage = stage;
  g_object_ref (stage);

  self->duration = duration;

  self->finish_cb = finish_cb;
  self->user_data = user_data;

  self->current = video_player_new (stage,
                                    video_player_on_load,
                                    video_player_on_end,
                                    video_player_on_marker,
                                    self);

  self->preloads = g_hash_table_new_full (g_str_hash,
                                          g_str_equal,
                                          g_free,
                                          free_preload);

  return self;
}

void
transition_free (Transition *self)
{
  if (self->next_video != NULL)
    g_free (self->next_video);

  if (self->current != NULL)
    video_player_unref (self->current);

  if (self->next != NULL)
    video_player_unref (self->next);

  g_hash_table_unref (self->preloads);

  g_object_unref (self->stage);

  g_slice_free (Transition, self);
}

void
transition_set_next_video (Transition  *self, const gchar *uri)
{
  if (video_player_get_state (self->current) != GST_STATE_PLAYING)
    {
      ClutterActor *tex;

      tex = video_player_get_texture (self->current);
      clutter_actor_add_child (self->stage, tex);
      clutter_actor_set_opacity (tex, 255);

      video_player_set_uri (self->current, uri);

      video_player_set_state (self->current, GST_STATE_PLAYING);
    }
  else
    {
      Preload *preload;

      preload = g_hash_table_lookup (self->preloads, uri);
      g_assert (preload != NULL);

      g_free (self->next_video);
      self->next_video = g_strdup (uri);
    }
}

void
transition_preload_video (Transition  *self,
                          const gchar *url,
                          guint        transition_duration)
{
  Preload *preload;

  preload = g_hash_table_lookup (self->preloads, url);
  if (preload == NULL)
    {
      preload = g_slice_new0 (Preload);

      preload->player = video_player_new (self->stage,
                                          video_player_on_load,
                                          video_player_on_end,
                                          video_player_on_marker,
                                          self);

      preload->url = g_strdup (url);

      video_player_set_uri (preload->player, url);
      video_player_set_state (preload->player, GST_STATE_PAUSED);

      g_hash_table_insert (self->preloads, preload->url, preload);
    }

  preload->transition_duration = transition_duration;
}
