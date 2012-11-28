/*
 * salut.c
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

#include "salut.h"
#include <math.h>

static const guint THRESHOLD_END   = 1500;
static const guint THRESHOLD_BEGIN = 500;

#define HAND_BOX_SIZE 150.0
#define USE_HANDS_IN_CURTSY TRUE

static void
empty_gesture_list (SkeltrackJoint **list, int length)
{
  int l;

  if (list == NULL)
    return;

  for (l = 0; l < length; l++)
    {
      skeltrack_joint_free (list[l]);
      list[l] = NULL;
    }
}

static void
free_gesture_list (SkeltrackJoint **list, int length)
{
  if (list == NULL)
    return;

  empty_gesture_list (list, length);
  g_slice_free1 (length * sizeof (SkeltrackJoint), list);
}

static gfloat
get_distance (SkeltrackJoint *a, SkeltrackJoint *b)
{
  gfloat x, y, z;
  x = a->x - b->x;
  y = a->y - b->y;
  z = a->z - b->z;
  return sqrt (x * x + y * y + z * z);
}

static gfloat
get_points_distance (CvPoint *a, CvPoint *b)
{
  gfloat x, y;
  x = a->x - b->x;
  y = a->y - b->y;
  return sqrt (x * x + y * y);
}

static void
bow_gesture (Salut *self, SkeltrackJointList list)
{
  SkeltrackJoint *head, *previous_head;

  if (list == NULL)
    return;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);

  if (head == NULL)
    return;

  previous_head = self->gesture_list[self->gesture_index];
  if (previous_head != NULL)
    {
      gfloat x, y, z, d;
      gint dist = 100;
      gint min_head_distance = 150;
      x = previous_head->x - head->x;
      y = previous_head->y - head->y;
      z = previous_head->z - head->z;
      d = sqrt (x * x + y * y + z * z);

      if (d >= dist)
        {
          if (ABS (previous_head->z - head->z) > 100 &&
              previous_head->z > head->z &&
              previous_head->screen_y < head->screen_y)
            {
              self->gesture_list[++self->gesture_index] =
                skeltrack_joint_copy (head);
            }
          else if (sqrt (x * x + y * y) > min_head_distance ||
                   (ABS (previous_head->z - head->z) > min_head_distance &&
                    previous_head->z < head->z))
            {
              skeltrack_joint_free (self->gesture_list[self->gesture_index]);
              self->gesture_list[self->gesture_index] = NULL;
            }
          if (self->gesture_index == 2)
            {
              if (self->callback != NULL)
                self->callback(self->callback_data);

              for (; self->gesture_index >= 0; self->gesture_index--)
                {
                  skeltrack_joint_free (self->gesture_list[self->gesture_index]);
                  self->gesture_list[self->gesture_index] = NULL;
                }
              self->gesture_index = 0;
            }
        }
    }
  else
    {
      self->gesture_list[self->gesture_index] = skeltrack_joint_copy (head);
    }

}

static void
kiss_gesture          (Salut *self, SkeltrackJointList list)
{
  SkeltrackJoint *head, *left_hand, *right_hand, *hand, *previous_hand;

  if (list == NULL)
    return;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);
  right_hand = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_RIGHT_HAND);
  left_hand = skeltrack_joint_list_get_joint (list,
                                              SKELTRACK_JOINT_ID_LEFT_HAND);

  hand = NULL;
  previous_hand = NULL;

  if (head == NULL || (left_hand == NULL && right_hand == NULL))
    {
      for (self->gesture_index = 3;
           self->gesture_index >= 0; self->gesture_index--)
        {
          skeltrack_joint_free (self->gesture_list[self->gesture_index]);
          self->gesture_list[self->gesture_index] = NULL;
        }
      self->gesture_index = 0;

      return;
    }

  if (right_hand == NULL)
    hand = left_hand;
  else if (left_hand == NULL)
    hand = right_hand;

  if (hand == NULL)
    {
      if (left_hand->y < right_hand->y)
        hand = left_hand;
      else
        hand = right_hand;
    }

  previous_hand = NULL;
  if (self->gesture_index == 0)
    {
      if (self->gesture_list[0] == NULL)
        {
          self->gesture_list[0] = skeltrack_joint_copy (head);
        }
    }
  else
    {
      previous_hand = self->gesture_list[self->gesture_index];
    }

  if (previous_hand == NULL)
    {
      guint initial_hand_head_max_dist = 400;
      if (get_distance (hand, head) < initial_hand_head_max_dist)
        {
          self->gesture_index++;
          self->gesture_list[self->gesture_index] = skeltrack_joint_copy (hand);
        }
    }
  else
    {
      guint dist = get_distance (previous_hand, hand);
      if ((dist > 200) && (dist < 500) && (hand->z < previous_hand->z))
        {
          self->gesture_list[++self->gesture_index] =
            skeltrack_joint_copy (hand);
        }
    }

  if (self->gesture_index == -1 || self->gesture_index == 3)
    {
      if (self->gesture_index == 3 && self->callback)
        self->callback (self->callback_data);

      for (self->gesture_index = 3;
           self->gesture_index >= 0; self->gesture_index--)
        {
          skeltrack_joint_free (self->gesture_list[self->gesture_index]);
          self->gesture_list[self->gesture_index] = NULL;
        }
      self->gesture_index = 0;
    }

}

static void
curtsy_gesture (Salut *self, SkeltrackJointList list)
{
  SkeltrackJoint *head, *left_hand, *right_hand, *left_elbow, *right_elbow;

  if (list == NULL)
    return;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);
  right_hand = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_RIGHT_HAND);
  left_hand = skeltrack_joint_list_get_joint (list,
                                              SKELTRACK_JOINT_ID_LEFT_HAND);
  right_elbow = skeltrack_joint_list_get_joint (list,
                                                SKELTRACK_JOINT_ID_RIGHT_ELBOW);
  left_elbow = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_LEFT_ELBOW);

  if (head == NULL || (left_hand == NULL || right_hand == NULL))
    return;

  if (head)
    {
      /* Check if arms are in the correct pose,
         if not, clean the list so far */
      if (USE_HANDS_IN_CURTSY &&
          (right_hand->y < head->y ||
           left_hand->y < head->y ||
           right_hand->x > right_elbow->x ||
           left_hand->x < left_elbow->x))
        {
          for (; self->gesture_index >= 0; self->gesture_index--)
            {
              skeltrack_joint_free (self->gesture_list[self->gesture_index]);
              self->gesture_list[self->gesture_index] = NULL;
            }
          self->gesture_index = 0;
        }

      SkeltrackJoint *previous_head = self->gesture_list[self->gesture_index];
      if (previous_head != NULL)
        {
          gfloat x, y, z, d;
          gint dist_with_previous_head = 150;
          gint min_head_movement = 100;
          x = previous_head->x - head->x;
          y = previous_head->y - head->y;
          z = previous_head->z - head->z;
          d = sqrt (x * x + y * y + z * z);

          if (d >= dist_with_previous_head)
            {
              if (ABS (previous_head->z - head->z) < min_head_movement &&
                  ABS (previous_head->y - head->y) > min_head_movement)
                {
                  /* Movement reached lowest point (initial movement) */
                  if (self->gesture_index == 0 && previous_head->y < head->y)
                    {
                      self->gesture_list[++self->gesture_index] =
                        skeltrack_joint_copy (head);
                    }
                  /* Movement went back up (final movement) */
                  else if (self->gesture_index == 1 &&
                           previous_head->y > head->y)
                    {
                      self->gesture_list[++self->gesture_index] =
                        skeltrack_joint_copy (head);
                    }
                }
              else
                {
                  skeltrack_joint_free (self->gesture_list[self->gesture_index]);
                  self->gesture_list[self->gesture_index] = NULL;

                  if (self->gesture_index > 0)
                    self->gesture_index--;

                  self->gesture_list[self->gesture_index] =
                    skeltrack_joint_copy (head);
                }

              if (self->gesture_index == 2)
                {
                  if (self->callback != NULL)
                    self->callback(self->callback_data);

                  for (; self->gesture_index >= 0; self->gesture_index--)
                    {
                      skeltrack_joint_free (self->gesture_list[self->gesture_index]);
                      self->gesture_list[self->gesture_index] = NULL;
                    }
                  self->gesture_index = 0;
                }
            }
        }
      else
        {
          self->gesture_list[self->gesture_index] = skeltrack_joint_copy (head);
        }
    }
}

static gboolean
can_wave_hello (SkeltrackJoint *hand, SkeltrackJoint *elbow)
{
  return hand != NULL && elbow != NULL &&
    hand->y < elbow->y && ABS (hand->y - elbow->y) > 100;
}

static gint
get_sign (gint n)
{
  return n >= 0 ? 1 : -1;
}

static void
hello_gesture (Salut *self, SkeltrackJointList list)
{
  SkeltrackJoint *head, *left_hand, *left_elbow,
    *right_hand, *right_elbow, *elbow = NULL, *hand = NULL;

  if (list == NULL)
    return;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);
  right_hand = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_RIGHT_HAND);
  left_hand = skeltrack_joint_list_get_joint (list,
                                              SKELTRACK_JOINT_ID_LEFT_HAND);
  right_elbow = skeltrack_joint_list_get_joint (list,
                                                SKELTRACK_JOINT_ID_RIGHT_ELBOW);
  left_elbow = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_LEFT_ELBOW);

  if (head == NULL || (left_hand == NULL && right_hand == NULL))
    return;

  if (can_wave_hello (right_hand, right_elbow))
    {
      hand = right_hand;
      elbow = right_elbow;
    }
  else if (can_wave_hello (left_hand, left_elbow))
    {
      hand = left_hand;
      elbow = left_elbow;
    }

  if (hand && elbow)
    {
      SkeltrackJoint *previous_hand = self->gesture_list[self->gesture_index];
      SkeltrackJoint *previous_elbow =
        self->gesture_list[self->gesture_index + 1];

      /* Check if the hand has moved beyond the X coord of the elbow
         (in comparison with the previous values) */
      if (previous_hand == NULL ||
          get_sign (previous_hand->x - previous_elbow->x) !=
          get_sign (hand->x - elbow->x))
        {
          if (previous_hand)
            self->gesture_index += 2;

          self->gesture_list[self->gesture_index] =
            skeltrack_joint_copy (hand);
          self->gesture_list[self->gesture_index + 1] =
            skeltrack_joint_copy (elbow);
        }

      if (self->gesture_index == 8)
        {
          gint i;
          for (i = 0; i <= self->gesture_index; i += 2)
            {
              hand = self->gesture_list[i];
              elbow = self->gesture_list[i + 1];
              skeltrack_joint_free (hand);
              skeltrack_joint_free (elbow);
              self->gesture_list[i] = NULL;
              self->gesture_list[i + 1] = NULL;
            }

          if (self->callback != NULL)
            self->callback (self->callback);

          self->gesture_index = 0;
        }
    }
  else if (self->gesture_list[self->gesture_index])
    {
      skeltrack_joint_free(self->gesture_list[self->gesture_index + 1]);
      self->gesture_list[self->gesture_index + 1] = NULL;
      skeltrack_joint_free(self->gesture_list[self->gesture_index]);
      self->gesture_list[self->gesture_index] = NULL;

      if (self->gesture_index > 0)
        self->gesture_index-=2;
    }
}

static IplImage *
segment_hand (guint16 *buffer,
              guint width,
              guint height,
              guint hand_x,
              guint hand_y,
              guint hand_z)
{

  IplImage* image;
  CvSize size;
  gfloat scale;
  gint box_size;
  guint i, j, x_left, x_right, y_top, y_bottom, counter, avg_x, avg_y;

  if (buffer == NULL)
    return NULL;

  scale = ((gfloat)(THRESHOLD_END - THRESHOLD_BEGIN)) / (hand_z * .8);
  box_size = round(HAND_BOX_SIZE * scale);
  box_size -= box_size % 4;

  if (box_size > width || box_size == 0)
    return NULL;

  avg_x = 0;
  avg_y = 0;
  counter = 0;
  x_left = MAX (hand_x - box_size / 2, 0);
  x_right = MIN (hand_x + box_size / 2, width);
  y_top = MAX (hand_y - box_size / 2, 0);
  y_bottom = MIN (hand_y + box_size / 2, height);

  for (i = x_left; i < x_right; i += 2)
    for (j = y_top; j < y_bottom; j += 2)
      {
        guint16 value = buffer[width * j + i];
        if (value < THRESHOLD_END && value > THRESHOLD_BEGIN && value < hand_z)
          {
            hand_x = i;
            hand_y = j;
            hand_z = value;
          }
      }

  x_left = MAX (hand_x - box_size / 2, 0);
  x_right = MIN (hand_x + box_size / 2, width);
  y_top = MAX (hand_y - box_size / 2, 0);
  y_bottom = MIN (hand_y + box_size / 2, height);

  for (i = x_left; i < x_right; i += 2)
    for (j = y_top; j < y_bottom; j += 2)
      {
        guint16 value = buffer[width * j + i];
        if (value > THRESHOLD_BEGIN && value < THRESHOLD_END && ABS (value - hand_z) < 150)
          {
            counter++;
            avg_x += i;
            avg_y += j;
          }
      }

  if (counter > 0)
    {
      avg_x /= counter;
      avg_y /= counter;
    }

  x_left = MAX (avg_x - box_size / 2, 0);
  x_right = MIN (avg_x + box_size / 2, width);
  y_top = MAX (avg_y - box_size / 2, 0);
  y_bottom = MIN (avg_y + box_size / 2, height);

  size.width = box_size;
  size.height = box_size;
  image = cvCreateImage (size, IPL_DEPTH_8U, 1);

  for (i = 0; i < image->width; i ++)
    for (j = 0; j < image->height; j ++)
      {
        if ((i + x_left) < width && (j + y_top) < height)
          {
            guint16 value = buffer[width * (y_top + j) + (x_left + i)];
            if (value > THRESHOLD_BEGIN && value < THRESHOLD_END && ABS (value - hand_z) < 150)
              {
                image->imageData[image->width * j + i] = (uchar) 255;
                continue;
              }
          }
        image->imageData[image->width * j + i] = (uchar) 0;
      }

  return image;
}

static gfloat
get_angle (CvPoint *p1,
           CvPoint *p2,
           CvPoint *p3)
{
  gfloat x1, x2, angle1, angle2;

  x1 = (gfloat) (p1->x - p2->x);
  x2 = (gfloat) (p3->x - p2->x);
  angle1 = atan (((gfloat) (p1->y - p2->y)) / x1);
  if (x1 < 0)
    angle1 = - (M_PI - angle1);
  angle2 = atan (((gfloat) (p3->y - p2->y)) / x2);
  if (x2 < 0)
    angle2 = - (M_PI - angle2);

  return ABS (angle1 - angle2);
}

static gfloat
get_orientation_angle (CvPoint *p1,
                       CvPoint *p2,
                       CvPoint *p3)
{
  gfloat x1, x2, angle1, angle2, final_angle;
  x1 = (gfloat) (p1->x - p2->x);
  x2 = (gfloat) (p3->x - p2->x);
  angle1 = atan ((- (gfloat) (p1->y - p2->y)) / x1);

  if (x1 < 0)
    angle1 += M_PI;
  if (angle1 < 0)
    angle1 = M_PI * 2 + angle1;
  angle2 = atan ((- (gfloat) (p3->y - p2->y)) / x2);

  if (x2 < 0)
    angle2 += M_PI;
  if (angle1 < 0)
    angle1 = M_PI * 2 + angle1;

  final_angle = ABS (angle1 - angle2);

  if (final_angle > M_PI)
    {
      final_angle = M_PI * 2 - final_angle;
      if (angle1 < angle2)
        angle1 += M_PI * 2;
      else if (angle2 < angle1)
        angle2 += M_PI * 2;
    }

  return MIN (angle1, angle2) + final_angle / 2;
}

static gboolean
defects_are_horizontal (CvSeq *defects)
{
  /* Gets the angle between the start point
     of the first defect and the end point of
     the second and checks if it between 45 and
     90 degrees which means that the defects
     are horizontal (their opening is pointing
     sideways).*/

  gfloat x, angle;
  CvPoint *p1, *p2;
  CvConvexityDefect *d1, *d2;

  d1 = CV_GET_SEQ_ELEM (CvConvexityDefect, defects, 0);
  d2 = CV_GET_SEQ_ELEM (CvConvexityDefect, defects, 1);
  p1 = d1->start;
  p2 = d2->end;

  x = (gfloat) (p1->x - p2->x);
  angle = ABS (atan (((gfloat) (p1->y - p2->y)) / x));

  if (angle < M_PI_4 || angle > M_PI_2)
    {
      return FALSE;
    }

  return TRUE;
}

static CvSeq *
get_defects (guint16* depth,
             guint width,
             guint height,
             guint start_x,
             guint start_y,
             guint start_z)
{
  IplImage *img;
  IplImage *image = NULL;
  CvSeq *points = NULL;
  CvSeq *contours = NULL;
  CvMemStorage *g_storage, *hull_storage;

  img = segment_hand (depth,
                      width,
                      height,
                      start_x,
                      start_y,
                      start_z);

  if (img == NULL)
    {
      return NULL;
    }

  image = cvCreateImage(cvGetSize(img), 8, 1);
  cvCopy(img, image, 0);
  cvSmooth(image, img, CV_MEDIAN, 7, 0, 0, 0);
  cvThreshold(img, image, 150, 255, CV_THRESH_OTSU);
  g_storage = cvCreateMemStorage (0);
  cvFindContours (image, g_storage, &contours, sizeof(CvContour),
                  CV_RETR_EXTERNAL,
                  CV_CHAIN_APPROX_SIMPLE,
                  cvPoint(0,0));
  hull_storage = cvCreateMemStorage(0);

  if (contours)
    {
      points = cvConvexHull2(contours, hull_storage, CV_CLOCKWISE, 0);
      return cvConvexityDefects (contours, points, NULL);
    }

  return NULL;
}

static CvSeq *
get_finger_defects (guint16* depth,
                    guint width,
                    guint height,
                    SkeltrackJointList list)
{
  CvSeq *defects = NULL;
  SkeltrackJoint *head, *left_hand, *right_hand, *hand = NULL;

  if (list == NULL)
    return NULL;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);
  right_hand = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_RIGHT_HAND);
  left_hand = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_LEFT_HAND);

  if (head == NULL || (left_hand == NULL && right_hand == NULL))
    return NULL;

  if (right_hand == NULL)
    hand = left_hand;
  else if (left_hand == NULL)
    hand = right_hand;
  else
    {
      if (right_hand->z < left_hand->z &&
          ABS (right_hand->z - head->z) > 150)
        {
          hand = right_hand;
        }
      else if (left_hand->z < right_hand->z &&
               ABS (left_hand->z - head->z) > 150)
        {
          hand = left_hand;
        }
    }

  if (hand == NULL)
    return NULL;


  defects = get_defects (depth,
                         width,
                         height,
                         hand->screen_x,
                         hand->screen_y,
                         hand->z);

  if (defects)
    {
      gfloat angle;
      guint i;

      for (i = 0; i < defects->total; i++)
        {
          CvConvexityDefect *defect = CV_GET_SEQ_ELEM (CvConvexityDefect, defects, i);
          angle = get_angle (defect->start, defect->depth_point, defect->end);
          if (angle > M_PI_2)
            {
              cvSeqRemove(defects, i);
              i--;
            }
        }

      return defects;
    }

  return NULL;
}

static gboolean
hands_are_praying (guint16* depth,
                   guint width,
                   guint height,
                   SkeltrackJointList list)
{
  guint x, y, z;
  SkeltrackJoint *head, *left_elbow, *left_shoulder,
    *right_shoulder, *right_elbow;
  CvSeq *defects = NULL;

  if (list == NULL)
    return FALSE;

  head = skeltrack_joint_list_get_joint (list, SKELTRACK_JOINT_ID_HEAD);
  right_elbow = skeltrack_joint_list_get_joint (list,
                                                SKELTRACK_JOINT_ID_RIGHT_ELBOW);
  left_elbow = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_LEFT_ELBOW);
  right_shoulder = skeltrack_joint_list_get_joint (list,
                                                SKELTRACK_JOINT_ID_RIGHT_SHOULDER);
  left_shoulder = skeltrack_joint_list_get_joint (list,
                                               SKELTRACK_JOINT_ID_LEFT_SHOULDER);


  if (head == NULL || right_elbow == NULL ||
      left_elbow == NULL ||
      ((right_elbow->y < right_shoulder->y) &&
       (left_elbow->y < left_shoulder->y)))
    return FALSE;

  x = head->screen_x;
  y = right_elbow->screen_y;
  z = ((gfloat) (right_shoulder->z + left_shoulder->z)) / 2.0 - 300;

  defects = get_defects (depth, width, height, x, y, z);

  if (defects)
    {
      guint i, sum;

      sum = 0;
      for (i = 0; i < defects->total; i++)
        {
          gfloat orientation;
          CvConvexityDefect *defect = CV_GET_SEQ_ELEM (CvConvexityDefect,
                                                       defects,
                                                       i);
          orientation = get_orientation_angle (defect->start,
                                               defect->depth_point,
                                               defect->end);
          orientation = ((gint) (orientation / M_PI * 180.0)) % 360;
          if (defect->depth > 20.0 &&
              orientation < 180 &&
              orientation > 0 &&
              ABS (orientation - 90) > 25)
            {
              gfloat x1, x2, sig_x1, sig_x2;
              x1 = defect->start->x - defect->depth_point->x;
              x2 = defect->end->x - defect->depth_point->x;
              sig_x1 = x1 / ABS (x1);
              sig_x2 = x2 / ABS (x2);
              if (sig_x1 != sig_x2 || (x1 == 0 || x2 == 0))
                {
                  continue;
                }
              cvSeqRemove(defects, i);
              i--;
            }
          else
            {
              cvSeqRemove(defects, i);
              i--;
            }
        }

      if (defects->total > 1)
        {
          for (i = 1; i < defects->total; i++)
            {
              gfloat dist_hand1, dist_hand2, dist_depth_points;
              CvConvexityDefect *defect1, *defect2;
              CvPoint *defect1_top_point, *defect2_top_point;
              defect1 = CV_GET_SEQ_ELEM (CvConvexityDefect,
                                         defects,
                                         i);
              defect2 = CV_GET_SEQ_ELEM (CvConvexityDefect,
                                         defects,
                                         i - 1);

              if (defect1->end->y < defect1->start->y)
                defect1_top_point = defect1->end;
              else
                defect1_top_point = defect1->start;

              if (defect2->end->y < defect2->start->y)
                defect2_top_point = defect2->end;
              else
                defect2_top_point = defect2->start;

              dist_hand1 = get_points_distance (defect1_top_point,
                                                defect1->depth_point);
              dist_hand2 = get_points_distance (defect2_top_point,
                                                defect2->depth_point);
              dist_depth_points = get_points_distance (defect1->depth_point,
                                                       defect2->depth_point);
              if (dist_depth_points < MAX (dist_hand1, dist_hand2))
                sum++;
            }
        }

      if (sum > 0)
        return TRUE;
    }

  return FALSE;
}

static void
hands_pose (Salut *self,
            guint16* depth,
            guint width,
            guint height,
            SkeltrackJointList list)
{
  CvSeq *defects;

  switch (self->gest_id)
    {
    case HAND_METAL:
      defects = get_finger_defects (depth, width, height, list);
      if (defects == NULL)
        self->gesture_index = 0;
      else if (defects->total == 1)
        self->gesture_index++;
      break;

    case HAND_EAST_COAST:
      defects = get_finger_defects (depth, width, height, list);
      if (defects == NULL)
        self->gesture_index = 0;
      else if (defects->total == 2 && defects_are_horizontal (defects))
        self->gesture_index++;
      break;

    case HAND_INDIAN:
      if (hands_are_praying  (depth, width, height, list))
        self->gesture_index++;
      break;

    default:
      self->gesture_index = 0;
    }

  if (self->gesture_index == 5 && self->callback)
    {
      self->gesture_index = 0;
      self->callback (self->callback_data);
    }
}

Salut *
salut_new (void)
{
  Salut *salut;
  salut = g_slice_new (Salut);
  salut->gest_id = NONE;
  salut->callback = NULL;
  salut->callback_data = NULL;
  salut->gesture_list_length = 0;
  salut->gesture_list = NULL;
  salut->gesture_index = 0;

  return salut;
}

void
salut_set_gesture_to_track (Salut *self,
                            GestId id,
                            void (*callback) (gpointer),
                            gpointer callback_data)
{
  if (self->gest_id == id)
    empty_gesture_list (self->gesture_list, self->gesture_list_length);
  else
    {
      free_gesture_list (self->gesture_list, self->gesture_list_length);
      self->gesture_list = NULL;
      self->gesture_list_length = 0;
    }
  self->gesture_index = 0;

  switch (id)
    {
    case NONE:
    case TOTAL_GESTURES:
    case HAND_METAL:
    case HAND_EAST_COAST:
    case HAND_INDIAN:
      break;

    case BOW:
      self->gesture_list_length = 3;
      self->gesture_list =
        g_slice_alloc0 (self->gesture_list_length * sizeof (SkeltrackJoint));
      break;

    case HAND_WAVE:
      self->gesture_list_length = 6;
      self->gesture_list =
        g_slice_alloc0 (self->gesture_list_length * sizeof (SkeltrackJoint));
      break;

    case CURTSY:
      self->gesture_list_length = 2;
      self->gesture_list =
        g_slice_alloc0 (self->gesture_list_length * sizeof (SkeltrackJoint));
      break;

    case KISS:
      self->gesture_list_length = 3;
      self->gesture_list =
        g_slice_alloc0 (self->gesture_list_length * sizeof (SkeltrackJoint));
      break;
    }

  self->gest_id = id;
  self->callback = callback;
  self->callback_data = callback_data;
}

void
salut_set_track_data (Salut *self,
                      guint16 *depth,
                      guint width,
                      guint height,
                      SkeltrackJointList list)
{
  switch (self->gest_id)
    {
    case NONE:
    case TOTAL_GESTURES:
      break;

    case BOW:
      bow_gesture (self, list);
      break;

    case KISS:
      kiss_gesture (self, list);
      break;

    case CURTSY:
      curtsy_gesture (self, list);
      break;

    case HAND_WAVE:
      hello_gesture (self, list);
      break;

    case HAND_METAL:
    case HAND_EAST_COAST:
    case HAND_INDIAN:
      hands_pose (self, depth, width, height, list);
      break;
    }
}
