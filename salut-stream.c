/*
 * salut-stream.c
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

#include "salut-stream.h"

#define DEPTH_FRAME_CHECK_INTERVAL 5000

#define TRANSFORM_BUFFER TRUE
static guint THRESHOLD_BEGIN = 500;

struct _BufferInfo
{
  guint16 *buffer;
  guint16 *reduced_buffer;
  gint width;
  gint height;
  gint reduced_width;
  gint reduced_height;
};

typedef struct {
  void (*callback) (SalutStream *, gpointer);
  gpointer data;
} CallbackData;

static void
on_track_joints (GObject      *obj,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  SalutStream *self;
  BufferInfo *buffer_info;
  guint16 *reduced, *buffer;
  gint width, height, reduced_width, reduced_height;
  SkeltrackJointList list;
  GError *error = NULL;

  self = (SalutStream *) user_data;
  buffer_info = self->buffer_info;
  buffer = buffer_info->buffer;
  reduced = (guint16 *) buffer_info->reduced_buffer;
  width = buffer_info->width;
  height = buffer_info->height;
  reduced_width = buffer_info->reduced_width;
  reduced_height = buffer_info->reduced_height;

  list = skeltrack_skeleton_track_joints_finish (self->skeleton,
                                                 res,
                                                 &error);

  if (error != NULL)
    {
      g_warning ("%s\n", error->message);
      g_error_free (error);
    }
  else if (list && skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD))
    {
      if (self->status == SALUT_STREAM_NO_PERSON)
        {
          self->status = SALUT_STREAM_HAS_PERSON;
          if (self->person_entered_scene_cb != NULL)
            {
              self->person_entered_scene_cb (self,
                                             self->person_entered_scene_cb_data);
            }
        }
      self->last_skeleton_lookup_successful_attempt = g_get_real_time ();

      if (self->can_detect_gesture)
        {
          salut_set_track_data (self->salut, buffer, width, height, list);
        }
    }

  g_slice_free1 (reduced_width * reduced_height * sizeof (guint16), reduced);
  buffer_info->reduced_buffer = NULL;

  skeltrack_joint_list_free (list);
}

static void
process_buffer (BufferInfo *buffer_info,
                guint dimension_factor,
                guint threshold_begin,
                guint threshold_end)
{
  guint width, height;
  gint i, j, reduced_width, reduced_height;
  guint16 *buffer, *reduced_buffer;

  buffer = buffer_info->buffer;
  width = buffer_info->width;
  height = buffer_info->height;

  g_return_if_fail (buffer != NULL);

  reduced_width = (width - width % dimension_factor) / dimension_factor;
  reduced_height = (height - height % dimension_factor) / dimension_factor;

  reduced_buffer = g_slice_alloc0 (reduced_width * reduced_height *
                                   sizeof (guint16));

  for (i = 0; i < reduced_width; i++)
    {
      for (j = 0; j < reduced_height; j++)
        {
          gint index;
          guint16 value;

          index = j * width * dimension_factor + i * dimension_factor;
          value = buffer[index];

          if (value < threshold_begin || value > threshold_end)
            {
              reduced_buffer[j * reduced_width + i] = 0;
              continue;
            }

          reduced_buffer[j * reduced_width + i] = value;
        }
    }

  buffer_info->reduced_buffer = reduced_buffer;
  buffer_info->reduced_width = reduced_width;
  buffer_info->reduced_height = reduced_height;

  return;
}

static gboolean
abort_app (gpointer user_data)
{
  g_error ("The depth camera took to long initialize, aborting application!");

  return FALSE;
}

static void
on_depth_frame (GFreenectDevice *device, gpointer user_data)
{
  gint width, height;
  gint dimension_factor;
  guint16 *depth;
  BufferInfo *buffer_info;
  gsize len;
  GFreenectFrameMode frame_mode;
  SalutStream *self;
  gint64 current_time;
  gint time_diff;

  self = (SalutStream *) user_data;

  if (self->depth_frame_check_src_id != 0)
    {
      g_source_remove (self->depth_frame_check_src_id);

      /* schedule a new re-check of the depth camera */
      self->depth_frame_check_src_id =
        g_timeout_add (DEPTH_FRAME_CHECK_INTERVAL, abort_app, self);
    }

  if (! self->tracking)
    return;

  buffer_info = self->buffer_info;

  current_time = g_get_real_time ();
  time_diff = (gint) ((current_time - self->last_skeleton_lookup_successful_attempt) / 1000);
  if (time_diff > self->lookup_interval)
    {
      time_diff = (gint) ((current_time - self->last_skeleton_lookup_attempt) / 1000);
      if (time_diff < self->lookup_interval)
        {
          return;
        }

      if (self->status == SALUT_STREAM_HAS_PERSON)
        {
          self->status = SALUT_STREAM_NO_PERSON;
          if (self->person_left_scene_cb != NULL)
            {
              self->person_left_scene_cb (self,
                                          self->person_left_scene_cb_data);
            }
        }
    }

  self->last_skeleton_lookup_attempt = current_time;

  depth = (guint16 *) gfreenect_device_get_depth_frame_raw (device,
                                                            &len,
                                                            &frame_mode);

  if (depth == NULL)
    return;

  width = frame_mode.width;
  height = frame_mode.height;

  if (TRANSFORM_BUFFER)
    {
      if (buffer_info->buffer == NULL)
        {
          buffer_info->buffer = g_slice_alloc0 (width * height * sizeof (guint16));

        }

      guint i, j;

      for (j = 0; j < width; j++)
        {
          for (i = height - 1; i > 0; i--)
            {
              buffer_info->buffer[((width -1 - j) * height + ((height - 1) - i))] = depth[i * width + j];
            }
        }

      width = height;
      height = frame_mode.width;
    }
  else
    {
      buffer_info->buffer = depth;
    }

  buffer_info->width = width;
  buffer_info->height = height;

  g_object_get (self->skeleton, "dimension-reduction", &dimension_factor, NULL);

  process_buffer (buffer_info,
                  dimension_factor,
                  THRESHOLD_BEGIN,
                  self->depth_threshold);

  skeltrack_skeleton_track_joints (self->skeleton,
                                   buffer_info->reduced_buffer,
                                   buffer_info->reduced_width,
                                   buffer_info->reduced_height,
                                   NULL,
                                   on_track_joints,
                                   self);
}

static void
on_new_device (GObject      *obj,
               GAsyncResult *res,
               gpointer      user_data)
{
  GError *error = NULL;
  SalutStream *stream = NULL;
  Salut *salut;
  BufferInfo *buffer_info;
  SkeltrackSkeleton *skeleton;
  CallbackData *cb_data;
  void (*callback) (SalutStream *, gpointer);
  GFreenectDevice *device;

  cb_data = (CallbackData *) user_data;
  callback = cb_data->callback;

  device = gfreenect_device_new_finish (res, &error);

  g_assert (callback != NULL);

  if (device == NULL)
    goto leave;

  salut = salut_new ();

  skeleton = SKELTRACK_SKELETON (skeltrack_skeleton_new ());
  g_object_set (skeleton, "smoothing-factor", .25, NULL);

  buffer_info = g_slice_new0 (BufferInfo);

  stream = g_slice_new0 (SalutStream);
  stream->device = device;
  stream->skeleton = skeleton;
  stream->salut = salut;
  stream->depth_threshold = 2000;
  stream->buffer_info = buffer_info;
  stream->status = SALUT_STREAM_NO_PERSON;
  stream->lookup_interval = 2000;
  stream->last_skeleton_lookup_attempt = 0;
  stream->can_detect_gesture = FALSE;
  stream->person_entered_scene_cb = NULL;
  stream->person_entered_scene_cb_data = NULL;
  stream->person_left_scene_cb = NULL;
  stream->person_left_scene_cb_data = NULL;

  /* timeout to halt if no depth stream is received soon enough */
  stream->depth_frame_check_src_id =
    g_timeout_add (DEPTH_FRAME_CHECK_INTERVAL, abort_app, stream);

  g_signal_connect (device,
                    "depth-frame",
                    G_CALLBACK (on_depth_frame),
                    stream);

  if (! gfreenect_device_start_depth_stream (device,
                                             GFREENECT_DEPTH_FORMAT_MM,
                                             &error))
    {
      g_print ("Error starting depth stream: %s\n", error->message);
      g_error_free (error);
    }

  /* turn the kinect's led off */
  gfreenect_device_set_led (device, GFREENECT_LED_OFF, NULL, NULL, NULL);

 leave:
  callback (stream, cb_data->data);

  g_slice_free (CallbackData, cb_data);
}

void
salut_stream_new (void (*callback) (SalutStream *, gpointer), gpointer data)
{
  CallbackData *cb_data = g_slice_new (CallbackData);
  cb_data->callback = callback;
  cb_data->data = data;

  gfreenect_device_new (0,
                        GFREENECT_SUBDEVICE_CAMERA,
                        NULL,
                        on_new_device,
                        cb_data);
}

void
salut_stream_start (SalutStream *self)
{
  if (self == NULL)
    return;

  self->tracking = TRUE;
}

void
salut_stream_stop (SalutStream *self)
{
  if (self == NULL)
    return;

  self->tracking = FALSE;

  self->status = SALUT_STREAM_NO_PERSON;
}

void
salut_stream_free (SalutStream *self)
{
  if (self == NULL)
    return;

  if (self->buffer_info)
    {
      if (TRANSFORM_BUFFER)
        {
          guint width, height;
          width = self->buffer_info->width;
          height = self->buffer_info->height;
          g_slice_free1 (width * height * sizeof (guint16),
                         self->buffer_info->buffer);
      }
      g_slice_free (BufferInfo, self->buffer_info);
    }

  if (self->device != NULL)
    g_object_unref (self->device);

  if (self->skeleton != NULL)
    g_object_unref (self->skeleton);

  g_slice_free (SalutStream, self);
}

void
salut_stream_set_person_entered_cb (SalutStream *self,
                                    void (*person_entered_cb) (SalutStream *, gpointer),
                                    gpointer data)
{
  if (self == NULL)
    return;

  self->person_entered_scene_cb = person_entered_cb;
  self->person_entered_scene_cb_data = data;
}

void
salut_stream_set_person_left_cb (SalutStream *self,
                                 void (*person_left_cb) (SalutStream *, gpointer),
                                 gpointer data)
{
  if (self == NULL)
    return;

  self->person_left_scene_cb = person_left_cb;
  self->person_left_scene_cb_data = data;
}

void
salut_stream_set_person_lookup_seconds (SalutStream *self, gint msecs)
{
  if (self == NULL)
    return;

  self->lookup_interval = msecs;
}

void
salut_stream_set_depth_threshold (SalutStream *self, guint threshold)
{
  if (self == NULL)
    return;

  self->depth_threshold = threshold;
}

void
salut_stream_set_can_detect_gesture (SalutStream *self,
                                     gboolean can_detect_gesture)
{
  if (self == NULL)
    return;

  self->can_detect_gesture = can_detect_gesture;
}
