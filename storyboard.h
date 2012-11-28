/*
 * storyboard.h
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

#ifndef __STORYBOARD_H__
#define __STORYBOARD_H__

#include <glib.h>

G_BEGIN_DECLS

typedef struct _Storyboard Storyboard;

Storyboard *          storyboard_new             (const gchar *snippets_path);
void                  storyboard_free            (Storyboard *self);

G_END_DECLS

#endif /* __STORYBOARD_H__ */
