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

/*
 * scope-unit: this represents a transient unit like 'session-2.scope'
 * This is the actual cgroup delegated to the user, i.e.
 * /user/1000.user/c6.session.  The /user/1000.user part will already
 * have been created by the user_unit.
 */
#include "unit.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

#include "cgmanager.h"

typedef UnitClass ScopeUnitClass;
static GType scope_unit_get_type (void);

typedef struct
{
  Unit parent_instance;
  int id;
  int uid;
  char *slice_path;
} ScopeUnit;

G_DEFINE_TYPE (ScopeUnit, scope_unit, UNIT_TYPE)

static void
scope_unit_start (Unit *unit)
{
  ScopeUnit *pu = (ScopeUnit *) unit;
  cgmanager_create_all(pu->slice_path);
}

static void
scope_unit_stop (Unit *unit)
{
  ScopeUnit *pu = (ScopeUnit *) unit;
  cgmanager_remove_all(pu->slice_path);
}

static const gchar *
scope_unit_get_state (Unit *unit)
{
  ScopeUnit *u = (ScopeUnit *)unit;
  return u ->slice_path;
}

static void set_type(UnitClass *unit)
{
  unit->type = scope_unit;
}

Unit *
scope_unit_new (int id, GVariant *parameters)
{
    ScopeUnit *unit;
    int len, ret;
    const gchar *name, *mode, *str1;
    GVariant *v;
    GVariantIter *properties;

    g_return_val_if_fail (id >= 0, NULL);

    unit = g_object_new (scope_unit_get_type (), NULL);
    if (!unit)
        return NULL;
    unit->id = id;
    set_type((UnitClass *)unit);

    // will need for newer systemd:
    //g_variant_get (parameters, "(&s&sa(sv)a(s(a(sv)))", &name, &mode, &properties, &aux);
    g_variant_get (parameters, "(&s&sa(sv))", &name, &mode, &properties);

    while (g_variant_iter_loop(properties, "(&sv)", &str1, &v)) {
        const gchar *str2;
        gsize l;

        if (strcmp(str1, "Slice") == 0) {
            str2 = g_variant_get_string(v, &l);
            /* get the uid out of the slice name, i.e. user-1000.slice */
            ret = sscanf(str2, "user-%d.slice", &unit->uid);
            if (ret != 1) {
                free(unit);
                return NULL;
            }
        }
    }
    g_variant_iter_free (properties);

    /* path is "/user/%d.user/c%d.session".  22 + 2*(max int len) */
    len = 70;
    unit->slice_path = malloc(len);
    if (!unit->slice_path) {
        g_free(unit);
        return NULL;
    }
    ret = snprintf(unit->slice_path, len, "/user/%d.user/c%d.session", unit->uid, unit->id);
    if (ret < 0 || ret >= len) {
        g_free(unit);
        return NULL;
    }

    return (Unit *) unit;
}

static void
scope_unit_init (ScopeUnit *unit)
{
}

static void
scope_unit_class_init (UnitClass *class)
{
  class->start = scope_unit_start,
  class->stop = scope_unit_stop;
  class->get_state = scope_unit_get_state;
}
