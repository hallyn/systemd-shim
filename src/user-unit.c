/*
 * Copyright © 2014 Canonical Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the licence, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include "unit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "cgmanager.h"

typedef UnitClass UserUnitClass;
static GType user_unit_get_type (void);

typedef struct
{
  Unit parent_instance;
  int uid;
  char *slice_path;
} UserUnit;

G_DEFINE_TYPE (UserUnit, user_unit, UNIT_TYPE)

static void
user_unit_start (Unit *unit)
{
  UserUnit *pu = (UserUnit *) unit;
  cgmanager_create_all(pu->slice_path);
}

static void
user_unit_stop (Unit *unit)
{
  UserUnit *pu = (UserUnit *) unit;
  cgmanager_remove_all(pu->slice_path);
}

static const gchar *
user_unit_get_state (Unit *unit)
{
  UserUnit *u = (UserUnit *)unit;
  return u->slice_path;
}

static void set_type(UnitClass *unit)
{
  unit->type = user_unit;
}

Unit *
user_unit_new (int uid)
{
  UserUnit *unit;
  int len, ret;

  g_return_val_if_fail (uid >= 0, NULL);

  unit = g_object_new (user_unit_get_type (), NULL);
  if (!unit)
    return NULL;
  unit->uid = uid;
  set_type((UnitClass *)unit);

  /* path is "/user/%d.user".  12 + max int len */
  len = 35;
  unit->slice_path = malloc(len);
  if (!unit->slice_path) {
    g_free(unit);
    return NULL;
  }
  ret = snprintf(unit->slice_path, len, "/user/%d.user", unit->uid);
  if (ret < 0 || ret >= len) {
    g_free(unit);
    return NULL;
  }

  return (Unit *) unit;
}

static void
user_unit_init (UserUnit *unit)
{
}

static void
user_unit_class_init (UnitClass *class)
{
  class->start = user_unit_start,
  class->stop = user_unit_stop;
  class->get_state = user_unit_get_state;
}
