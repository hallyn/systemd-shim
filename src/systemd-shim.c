/*
 * Copyright © 2013 Canonical Limited
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the licence, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307,
 * USA.
 */

#include <gio/gio.h>

#include "unit.h"
#include "virt.h"

#include "systemd-iface.h"

#include <stdlib.h>
#include <stdio.h> /* for debugging only */

GDBusInterfaceInfo *manager_info = NULL, *scope_info = NULL;

static gboolean
exit_on_inactivity (gpointer user_data)
{
  extern gboolean in_shutdown;

  if (!in_shutdown)
    {
      GDBusConnection *system_bus;

      system_bus = g_bus_get_sync (G_BUS_TYPE_SYSTEM, NULL, NULL);
      g_dbus_connection_flush_sync (system_bus, NULL, NULL);
      g_object_unref (system_bus);

      exit (0);
    }

  return FALSE;
}

static void
had_activity (void)
{
  static gint inactivity_timeout;

  if (inactivity_timeout)
    g_source_remove (inactivity_timeout);

  inactivity_timeout = g_timeout_add (10000, exit_on_inactivity, NULL);
}

static void
mngr_method_call (GDBusConnection       *connection,
                  const gchar           *sender,
                  const gchar           *object_path,
                  const gchar           *interface_name,
                  const gchar           *method_name,
                  GVariant              *parameters,
                  GDBusMethodInvocation *invocation,
                  gpointer               user_data)
{
  GError *error = NULL;

{
  FILE *f = fopen("/tmp/x", "a");
  fprintf(f, "iface %s method %s\n", interface_name, method_name);
  fclose(f);
}

  if (g_str_equal (method_name, "GetUnitFileState"))
    {
      Unit *unit;

      unit = lookup_unit (parameters, &error);

      if (unit)
        {
          g_dbus_method_invocation_return_value (invocation,
                                                 g_variant_new ("(s)", unit_get_state (unit)));
          g_object_unref (unit);
          goto success;
        }
    }

  else if (g_str_equal (method_name, "DisableUnitFiles"))
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(a(sss))", NULL));
      goto success;
    }

  else if (g_str_equal (method_name, "EnableUnitFiles"))
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(ba(sss))", TRUE, NULL));
      goto success;
    }

  else if (g_str_equal (method_name, "Reload"))
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
      goto success;
    }

  else if (g_str_equal (method_name, "Subscribe"))
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
      goto success;
    }

  else if (g_str_equal (method_name, "Unsubscribe"))
    {
      g_dbus_method_invocation_return_value (invocation, NULL);
      goto success;
    }

  else if (g_str_equal (method_name, "StopUnit"))
    {
      Unit *unit;

      unit = lookup_unit (parameters, &error);

      if (unit)
        {
          unit_stop (unit);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", "/"));
          g_object_unref (unit);
          goto success;
        }
    }

  else if (g_str_equal (method_name, "StartUnit"))
    {
      Unit *unit;

      unit = lookup_unit (parameters, &error);

      if (unit)
        {
          unit_start (unit);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", "/"));
          g_dbus_connection_emit_signal (connection, sender, "/org/freedesktop/systemd1",
                                         "org.freedesktop.systemd1.Manager", "JobRemoved",
                                         g_variant_new ("(uoss)", 0, "/", "", ""), NULL);
          g_object_unref (unit);
          goto success;
        }
    }
  else if (g_str_equal (method_name, "StartTransientUnit"))
    {
      Unit *unit;

      unit = lookup_unit (parameters, &error);

      if (unit)
        {
          GVariant *properties;

          properties = g_variant_get_child_value (parameters, 2);
          unit_start_transient (unit, properties);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", "/"));
          g_dbus_connection_emit_signal (connection, sender, "/org/freedesktop/systemd1",
                                         "org.freedesktop.systemd1.Manager", "JobRemoved",
                                         g_variant_new ("(uoss)", 0, "/", unit_get_state(unit), "done"), NULL);
          g_variant_unref (properties);
          g_object_unref (unit);
          goto success;
      }
  }
  else if (g_str_equal (method_name, "Abandon"))
    {
{
      FILE *f = fopen("/tmp/x", "a");
      fprintf(f, "Called for scope:abandon\n");
      fclose(f);
}
#if 0
      Unit *unit;

      unit = lookup_unit (parameters, &error);

      if (unit)
        {
          unit_abandon (unit); // set remove_on_empty, then try to remove
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(o)", "/"));
          g_dbus_connection_emit_signal (connection, sender, "/org/freedesktop/systemd1",
                                         "org.freedesktop.systemd1.Manager", "JobRemoved",
                                         g_variant_new ("(uoss)", 0, "/", "", ""), NULL);
          g_object_unref (unit);
          goto success;
        }
#endif
  }

  else
    g_assert_not_reached ();

  g_dbus_method_invocation_return_gerror (invocation, error);
  g_error_free (error);

success:
  had_activity ();
}

static GVariant *
mngr_get_property (GDBusConnection  *connection,
                   const gchar      *sender,
                   const gchar      *object_path,
                   const gchar      *interface_name,
                   const gchar      *property_name,
                   GError          **error,
                   gpointer          user_data)
{
  const gchar *id = "";

  had_activity ();

  g_assert_cmpstr (property_name, ==, "Virtualization");

  detect_virtualization (&id);

  return g_variant_new ("s", id);
}

GDBusInterfaceVTable mngr_vtable = {
  mngr_method_call,
  mngr_get_property,
};

static gchar **
subtree_enumerate (GDBusConnection       *connection,
                   const gchar           *sender,
                   const gchar           *object_path,
                   gpointer               user_data)
{
  gchar **nodes;
  GPtrArray *p;

  p = g_ptr_array_new ();
  g_ptr_array_add (p, g_strdup ("Manager"));
  g_ptr_array_add (p, g_strdup ("Scope"));
  g_ptr_array_add (p, NULL);
  nodes = (gchar **) g_ptr_array_free (p, FALSE);

  return nodes;
}

static GDBusInterfaceInfo **
subtree_introspect (GDBusConnection       *connection,
                    const gchar           *sender,
                    const gchar           *object_path,
                    const gchar           *node,
                    gpointer               user_data)
{
  GPtrArray *p;

  p = g_ptr_array_new ();
  
  g_ptr_array_add (p, g_dbus_interface_info_ref (manager_info));
  g_ptr_array_add (p, g_dbus_interface_info_ref (scope_info));
  g_ptr_array_add (p, NULL);

  return (GDBusInterfaceInfo **) g_ptr_array_free (p, FALSE);
}

static const GDBusInterfaceVTable *
subtree_dispatch (GDBusConnection             *connection,
                  const gchar                 *sender,
                  const gchar                 *object_path,
                  const gchar                 *interface_name,
                  const gchar                 *node,
                  gpointer                    *out_user_data,
                  gpointer                     user_data)
{
  return &mngr_vtable;
}

const GDBusSubtreeVTable subtree_vtable =
{
  subtree_enumerate,
  subtree_introspect,
  subtree_dispatch
};

static void
shim_bus_acquired (GDBusConnection *connection,
                   const gchar     *name,
                   gpointer         user_data)
{
  guint rid;

  rid = g_dbus_connection_register_subtree (connection,
		  "/org/freedesktop/systemd1",
		  &subtree_vtable,
		  G_DBUS_SUBTREE_FLAGS_NONE,
		  NULL,  /* user_data */
		  NULL,  /* user_data_free_func */
		  NULL); /* GError** */
  g_assert (rid > 0);
}

static void
shim_name_lost (GDBusConnection *connection,
                const gchar     *name,
                gpointer         user_data)
{
  g_critical ("Unable to acquire bus name '%s'.  Quitting.", name);
  exit (1);
}

int
main (void)
{
  GDBusNodeInfo *node;

  node = g_dbus_node_info_new_for_xml (systemd_iface, NULL);
  g_assert( node );
  manager_info = g_dbus_node_info_lookup_interface (node, "org.freedesktop.systemd1.Manager");
  scope_info = g_dbus_node_info_lookup_interface (node, "org.freedesktop.systemd1.Scope");
  g_assert( manager_info && scope_info );

  g_bus_own_name (G_BUS_TYPE_SYSTEM,
                  "org.freedesktop.systemd1",
                  G_BUS_NAME_OWNER_FLAGS_NONE,
                  shim_bus_acquired,
                  NULL, /* name_acquired */
                  shim_name_lost,
                  NULL, NULL);

  cgmanager_moveself();
  while (1)
    g_main_context_iteration (NULL, TRUE);

  g_dbus_node_info_unref (node);
}
