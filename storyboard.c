/*
 * storyboard.c
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

#include <clutter/clutter.h>

#include "storyboard.h"
#include "transition.h"
#include "salut-stream.h"

#define DEFAULT_TRANSITION_DURATION 1000 /* miliseconds */

#define DEFAULT_MAX_KNOCK  3
#define DEFAULT_MAX_SALUTE 5

#define TOTAL_GESTURES (sizeof (gestures)/sizeof (Gesture))

typedef struct
{
  guint transition_duration;
} VideoSnippet;

typedef struct
{
  gchar *base_path;
  VideoSnippet snippets[6];
} Gesture;

static const GestId gesture_ids[] = {BOW,
                                     KISS,
                                     HAND_INDIAN,
                                     CURTSY,
                                     HAND_EAST_COAST};

static const Gesture gestures[] =
{
  {
    "japanese",
    {  }
  },
  {
    "kiss",
    {  }
  },
  {
    "indian",
    {  }
  },
  {
    "curtsy",
    {  }
  },
  {
     "east_coast",
    {  }
  }
};

typedef enum
{
  STATUS_NONE,
  STATUS_ENTER,
  STATUS_KNOCK,
  STATUS_SALUTE,
  STATUS_FEEDBACK,
  STATUS_LEAVE
} StoryboardStatus;

static const gchar *snippet_names[] =
{
  "enter.mov",
  "enter_knock.mov",
  "knock.mov",
  "salutation.mov",
  "positive.mov",
  "negative.mov",
  "leaving.mov"
};

typedef enum
{
  SNIPPET_TYPE_ENTER       = 0,
  SNIPPET_TYPE_ENTER_KNOCK = 1,
  SNIPPET_TYPE_KNOCK       = 2,
  SNIPPET_TYPE_SALUTATION  = 3,
  SNIPPET_TYPE_POSITIVE    = 4,
  SNIPPET_TYPE_NEGATIVE    = 5,
  SNIPPET_TYPE_LEAVE       = 6
} SnippetType;

struct _Storyboard
{
  gchar *snippets_path;

  ClutterActor *stage;
  Transition *transition;

  StoryboardStatus status;
  StoryboardStatus next_status;

  SalutStream *salut_stream;

  guint max_knock;
  guint max_salute;

  guint gesture_index;
  guint knock_count;
  guint salute_count;

  gboolean person_detected;
  gboolean gesture_detected;

  gint gesture_queue[6];
  gint gesture_queue_index;

  SnippetType feedback_type;
};

static void check_status        (Storyboard *self);
static void handle_enter_status (Storyboard *self);

static guint
get_next_gesture_index (Storyboard *self)
{
  return (self->gesture_index + 1) % TOTAL_GESTURES;
}

static gchar *
get_snippet_url (Storyboard *self,
                 guint       gesture_index,
                 SnippetType snippet_type)
{
  return g_strdup_printf ("%s/%s/%s",
                          self->snippets_path,
                          gestures[gesture_index].base_path,
                          snippet_names[snippet_type]);
}

static void
set_next_snippet (Storyboard *self,
                  guint       gesture_index,
                  SnippetType snippet_type)
{
  gchar *url;

  url = get_snippet_url (self,
                         self->gesture_index,
                         snippet_type);

  transition_set_next_video (self->transition, url);

  g_free (url);
}

static void
preload_snippet (Storyboard *self,
                 guint       gesture_index,
                 SnippetType snippet_type)
{
  gchar *url;
  guint duration;

  url = get_snippet_url (self,
                         self->gesture_index,
                         snippet_type);

  duration = gestures[gesture_index].snippets[snippet_type].transition_duration;
  if (duration == 0)
    duration = DEFAULT_TRANSITION_DURATION;

  transition_preload_video (self->transition,
                            url,
                            duration);
  g_free (url);
}

static void
on_gesture_accomplished (gpointer data)
{
  Storyboard *self = (Storyboard *) data;

  if (self->status == STATUS_SALUTE && ! self->gesture_detected)
    {
      g_print ("GESTURE ACCOMPLISHED\n");

      self->gesture_detected = TRUE;
      check_status (self);
    }
}

static void
set_detect_gesture (Storyboard *self, gboolean detect)
{
  if (self->salut_stream != NULL)
    {
      g_print ("DETECTING %s\n", detect ? "ENABLED" : "DISABLED");
      salut_stream_set_can_detect_gesture (self->salut_stream, detect);
    }
}

static void
handle_initial_status (Storyboard *self)
{
  g_print ("status: NONE\n");

  /* pick a random gesture */
  self->gesture_index = get_next_gesture_index (self);

  /* setup salut stream to track person entering */
  if (self->salut_stream != NULL)
    {
      salut_set_gesture_to_track (self->salut_stream->salut,
                                  gesture_ids[self->gesture_index],
                                  on_gesture_accomplished,
                                  self);
    }

  self->gesture_detected = FALSE;

  self->status = STATUS_ENTER;
  handle_enter_status (self);
}

static void
handle_enter_status (Storyboard *self)
{
  g_print ("status: ENTER\n");

  /* but we are not interested in gestures yet */
  set_detect_gesture (self, FALSE);

  self->knock_count = 1;

  if (self->person_detected)
    {
      self->next_status = STATUS_SALUTE;
      self->salute_count = 1;
    }
  else
    {
      self->next_status = STATUS_KNOCK;
    }
}

static void
handle_knock_status (Storyboard *self)
{
  g_print ("status: KNOCK\n");

  /* but we are not interested in gestures yet */
  set_detect_gesture (self, FALSE);

  if (self->person_detected)
    {
      self->next_status = STATUS_SALUTE;
      self->salute_count = 1;
    }
  else if (self->knock_count >= self->max_knock)
    {
      self->next_status = STATUS_LEAVE;
    }
  else
    {
      self->next_status = STATUS_KNOCK;
    }
}

static void
handle_salute_status (Storyboard *self)
{
  g_print ("status: SALUTE\n");

  /* and to detect gesture */
  set_detect_gesture (self, TRUE);

  if (self->gesture_detected)
    {
      self->feedback_type = SNIPPET_TYPE_POSITIVE;
      self->next_status = STATUS_FEEDBACK;
    }
  else if (self->salute_count >= self->max_salute)
    {
      self->feedback_type = SNIPPET_TYPE_NEGATIVE;
      self->next_status = STATUS_FEEDBACK;
    }
  else
    {
      self->next_status = STATUS_SALUTE;
    }
}

static void
handle_feedback_status (Storyboard *self)
{
  g_print ("status: FEEDBACK\n");

  /* neither gesture */
  set_detect_gesture (self, FALSE);

  self->next_status = STATUS_NONE;
}

static void
handle_leave_status (Storyboard *self)
{
  g_print ("status: LEAVE\n");

  /* neither gesture */
  set_detect_gesture (self, FALSE);

  self->next_status = STATUS_NONE;
}

static void
check_status (Storyboard *self)
{
  switch (self->status)
    {
    case STATUS_NONE:
      handle_initial_status (self);
      break;

    case STATUS_ENTER:
      handle_enter_status (self);
      break;

    case STATUS_KNOCK:
      handle_knock_status (self);
      break;

    case STATUS_SALUTE:
      handle_salute_status (self);
      break;

    case STATUS_FEEDBACK:
      handle_feedback_status (self);
      break;

    case STATUS_LEAVE:
      handle_leave_status (self);
      break;
    }
}

static void
transition_on_finish (Transition *transition, gpointer user_data)
{
  Storyboard *self = user_data;

  if (self->status == STATUS_KNOCK || self->status == STATUS_ENTER)
    self->knock_count++;
  else if (self->status == STATUS_SALUTE)
    self->salute_count++;

  self->status = self->next_status;

  check_status (self);

  switch (self->status)
    {
    case STATUS_ENTER:
      if (! self->person_detected)
        {
          preload_snippet (self, self->gesture_index, SNIPPET_TYPE_ENTER_KNOCK);
          set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_ENTER_KNOCK);
          self->knock_count++;
        }
      else
        {
          preload_snippet (self, self->gesture_index, SNIPPET_TYPE_ENTER);
          set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_ENTER);
        }
      break;

    case STATUS_KNOCK:
      preload_snippet (self, self->gesture_index, SNIPPET_TYPE_KNOCK);
      set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_KNOCK);
      break;

    case STATUS_SALUTE:
      preload_snippet (self, self->gesture_index, SNIPPET_TYPE_SALUTATION);
      set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_SALUTATION);
      break;

    case STATUS_LEAVE:
      preload_snippet (self, self->gesture_index, SNIPPET_TYPE_LEAVE);
      set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_LEAVE);
      break;

    case STATUS_FEEDBACK:
      preload_snippet (self, self->gesture_index, self->feedback_type);
      set_next_snippet (self, self->gesture_index, self->feedback_type);
      break;

    case STATUS_NONE:
      g_assert_not_reached ();
      break;
    }
}

static void
person_entered_scene (Storyboard *self)
{
  if (! self->person_detected)
    {
      g_print ("PERSON ENTERED\n");

      self->person_detected = TRUE;

      check_status (self);
    }
}

static void
person_left_scene (Storyboard *self)
{
  if (self->person_detected)
    {
      g_print ("PERSON LEFT\n");

      self->person_detected = FALSE;

      if (self->status == STATUS_SALUTE)
        self->salute_count = self->max_salute;

      check_status (self);
    }
}

static void
stage_on_key_pressed (ClutterActor *actor,
                      ClutterEvent *event,
                      gpointer      user_data)
{
  Storyboard *self = user_data;
  guint key = clutter_event_get_key_symbol (event);

  switch (key)
    {
    case CLUTTER_KEY_Return:
      person_entered_scene (self);
      break;

    case CLUTTER_KEY_Escape:
      person_left_scene (self);
      break;

    case CLUTTER_KEY_space:
      on_gesture_accomplished (self);
    }
}

static void
on_person_entered_cb (SalutStream *stream, gpointer data)
{
  Storyboard *self;
  self = (Storyboard *) data;

  person_entered_scene (self);
}

static void
on_person_left_cb (SalutStream *stream, gpointer data)
{
  Storyboard *self;
  self = (Storyboard *) data;

  person_left_scene (self);
}

static void
on_salut_stream_ready (SalutStream *stream, gpointer data)
{
  Storyboard *self = data;

  if (stream == NULL)
    {
      /* @TODO: Abort if no kinect detected!!!!!!! */
      /* g_error ("Oops! Problem initiating the stream!\n"); */
      g_error ("Oops! Problem initiating the stream. Is the kinect connected?!\n");
      return;
    }

  self->salut_stream = stream;

  check_status (self);
  set_next_snippet (self, self->gesture_index, SNIPPET_TYPE_ENTER_KNOCK);

  salut_stream_set_person_lookup_seconds (self->salut_stream, 1000);
  salut_stream_set_depth_threshold (self->salut_stream, 2000);
  salut_stream_set_person_entered_cb (self->salut_stream,
                                      on_person_entered_cb,
                                      self);
  salut_stream_set_person_left_cb (self->salut_stream,
                                   on_person_left_cb,
                                   self);

  salut_set_gesture_to_track (self->salut_stream->salut,
                              gesture_ids[self->gesture_index],
                              on_gesture_accomplished,
                              self);

  salut_stream_start (self->salut_stream);
}

/* public methods */

Storyboard *
storyboard_new (const gchar *snippets_path)
{
  Storyboard *self;
  ClutterColor bg_color = {200, 200, 200, 255};

  self = g_slice_new0 (Storyboard);

  self->max_knock = DEFAULT_MAX_KNOCK;
  self->max_salute = DEFAULT_MAX_SALUTE;
  self->gesture_index = g_random_int_range (0, TOTAL_GESTURES-1);

  if (g_strstr_len (snippets_path, -1, "file://") != snippets_path)
    self->snippets_path = g_strdup_printf ("file://%s", snippets_path);
  else
    self->snippets_path = g_strdup (snippets_path);

  self->stage = clutter_stage_new ();
  clutter_stage_hide_cursor (CLUTTER_STAGE (self->stage));
  g_signal_connect (self->stage,
                    "destroy",
                    G_CALLBACK (clutter_main_quit),
                    NULL);
  g_signal_connect (self->stage,
                    "key-press-event",
                    G_CALLBACK (stage_on_key_pressed),
                    self);

  clutter_actor_set_size (self->stage, 1920, 1080);
  clutter_stage_set_fullscreen (CLUTTER_STAGE (self->stage), TRUE);
  clutter_actor_set_background_color (self->stage, &bg_color);

  /* transition */
  self->transition = transition_new (self->stage,
                                     1000,
                                     transition_on_finish,
                                     self);

  /* salut stream */
  salut_stream_new (on_salut_stream_ready, self);

  g_object_unref (self->stage);

  clutter_actor_show (self->stage);

  /* initial state */
  self->status = STATUS_NONE;

  return self;
}

void
storyboard_free (Storyboard *self)
{
  g_free (self->snippets_path);

  g_signal_handlers_disconnect_by_func (self->stage,
                                        clutter_main_quit,
                                        NULL);

  transition_free (self->transition);
  if (self->salut_stream != NULL)
    salut_stream_free (self->salut_stream);

  g_slice_free (Storyboard, self);
}
