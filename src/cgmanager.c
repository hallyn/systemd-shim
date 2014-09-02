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
 *
 * Authors:
 *   Serge Hallyn <serge.hallyn@canonical.com>
 *   Stéphane Graber <stephane.graber@canonical.com>
 *   Ryan Lortie <desrt@desrt.ca>
 */

#include "cgmanager.h"

#include <stdbool.h>
#include <gio/gio.h>

#define CGM_DBUS_ADDRESS          "unix:path=/sys/fs/cgroup/cgmanager/sock"
#define CGM_REQUIRED_VERSION      6

static GDBusConnection *
cgmanager_connect (GError **error)
{
  GDBusConnection *connection;
  GVariant *reply;
  GVariant *version;

  connection = g_dbus_connection_new_for_address_sync (CGM_DBUS_ADDRESS,
                                                       G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
                                                       NULL, NULL, error);

  if (!connection)
    return NULL;

  reply = g_dbus_connection_call_sync (connection, NULL, "/org/linuxcontainers/cgmanager",
                                       "org.freedesktop.DBus.Properties", "Get",
                                       g_variant_new ("(ss)", "org.linuxcontainers.cgmanager0_0", "api_version"),
                                       G_VARIANT_TYPE ("(v)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, error);

  if (!reply)
    {
      g_object_unref (connection);
      return NULL;
    }

  g_variant_get (reply, "(v)", &version);
  g_variant_unref (reply);

  if (!g_variant_is_of_type (version, G_VARIANT_TYPE_INT32) || g_variant_get_int32 (version) < CGM_REQUIRED_VERSION)
    {
      g_set_error_literal (error, G_DBUS_ERROR, G_DBUS_ERROR_NOT_SUPPORTED, "Incorrect cgmanager API version");
      g_object_unref (connection);
      g_variant_unref (version);
      return NULL;
    }

  g_variant_unref (version);

  return connection;
}

static void
log_warning_on_error (GObject      *source,
                      GAsyncResult *result,
                      gpointer      user_data)
{
  GError *error = NULL;
  GVariant *reply;

  reply = g_dbus_connection_call_finish (G_DBUS_CONNECTION (source), result, &error);

  if (reply)
    g_variant_unref (reply);
  else
    {
      g_warning ("cgmanager method call failed: %s.  Use G_DBUS_DEBUG=message for more info.", error->message);
      g_error_free (error);
    }
}

static GDBusConnection *connection;
static gboolean initialised;

static void cgmanager_connect_wrapper(void)
{

  /* Use a separate bool to prevent repeated attempts to connect to a
   * defunct cgmanager...
   */
  if (!initialised)
    {
      GError *error = NULL;

      connection = cgmanager_connect (&error);

      if (!connection)
        {
          g_warning ("Could not connect to cgmanager: %s", error->message);
          g_error_free (error);
        }

      initialised = TRUE;
    }
}

static void
cgmanager_call (const gchar        *method_name,
                GVariant           *parameters,
                const GVariantType *reply_type)
{
  cgmanager_connect_wrapper();

  if (!connection)
    return;

  /* Avoid round-trip delays: issue all calls at once and report errors
   * asynchronously.  The user can enable GDBus debugging if they need
   * more information about the exact call that failed...
   */
  g_dbus_connection_call (connection, NULL, "/org/linuxcontainers/cgmanager",
                          "org.linuxcontainers.cgmanager0_0", method_name,
                          parameters, reply_type, G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, log_warning_on_error, NULL);
}

void
cgmanager_create (const gchar *path,
                  gint         uid,
                  guint       *pids,
                  guint        n_pids)
{
  guint i;

  if (path[0] == '/')
    path++;

  cgmanager_call ("Create", g_variant_new ("(ss)", "all", path), G_VARIANT_TYPE ("(i)"));

  if (uid != -1)
    cgmanager_call ("Chown", g_variant_new ("(ssii)", "all", path, uid, -1), G_VARIANT_TYPE_UNIT);

  for (i = 0; i < n_pids; i++)
    cgmanager_call ("MovePid", g_variant_new ("(ssi)", "all", path, pids[i]), G_VARIANT_TYPE_UNIT);
}

void
cgmanager_remove (const gchar *path)
{
  if (path[0] == '/')
    path++;

  cgmanager_call ("Remove", g_variant_new ("(ssi)", "all", path, 1), G_VARIANT_TYPE ("(i)"));
}

void
cgmanager_moveself (void)
{
  cgmanager_call ("MovePidAbs", g_variant_new ("(ssi)", "all", "/", getpid()), G_VARIANT_TYPE_UNIT);
}

static bool cg_exists(const gchar *path)
{
  cgmanager_connect_wrapper();
  GVariant *reply;
  GError *error = NULL;

  if (!connection)
    return false;

  reply = g_dbus_connection_call_sync (connection, NULL, "/org/linuxcontainers/cgmanager",
                          "org.linuxcontainers.cgmanager0_0", "GetTasks",
                          g_variant_new("(ss)", "name=systemd", path),
                          G_VARIANT_TYPE("(ai)"), G_DBUS_CALL_FLAGS_NONE,
                          -1, NULL, &error);
  if (!reply) {
    // we expect error if path does not exist
    g_error_free (error);
    return false;
  }
  return true;
}

static gchar *get_cgpath_from_scope(const gchar *scope)
{
  GVariant *reply, *array;
  GError *error = NULL;
  const gchar *child;
  int i, last;

  cgmanager_connect_wrapper();
  if (!connection)
    return NULL;

  reply = g_dbus_connection_call_sync (connection, NULL, "/org/linuxcontainers/cgmanager",
                          "org.linuxcontainers.cgmanager0_0", "ListChildren",
                          g_variant_new("(ss)", "name=systemd", "user.slice"),
                          G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (!reply) {
    g_warning ("Error getting list of sessions from cgmanager: %s", error->message);
    g_error_free (error);
    return NULL;
  }

  if (g_variant_n_children(reply) < 1) {
    g_variant_unref(reply);
    return NULL;
  }

  array = g_variant_get_child_value(reply, 0);
  if (!array) {
    g_variant_unref(reply);
    return NULL;
  }

  last = (int)g_variant_n_children(array);

  for (i = 0; i < last; i++) {
    g_variant_get_child (array, (gsize)i, "s", &child);
    gchar *path = g_strdup_printf("user.slice/%s/%s", child, scope);
    if (!path) {
        g_variant_unref(reply);
        g_variant_unref(array);
        return NULL;
    }
    if (cg_exists(path)) {
        g_variant_unref(array);
        g_variant_unref(reply);
        return path;
    }
  }
  g_variant_unref(array);
  g_variant_unref(reply);
  return NULL;
}

static void cg_kill_recursive(const gchar *path)
{
  GVariant *reply, *array;
  GError *error = NULL;
  const gchar *child;
  int i, last;

  cgmanager_connect_wrapper();
  if (!connection)
    return;

  cgmanager_call ("RemoveOnEmpty", g_variant_new ("(ss)", "all", path), G_VARIANT_TYPE_UNIT);

  /* first kill any subdirs recursively */
  reply = g_dbus_connection_call_sync (connection, NULL, "/org/linuxcontainers/cgmanager",
                          "org.linuxcontainers.cgmanager0_0", "ListChildren",
                          g_variant_new("(ss)", "name=systemd", path),
                          G_VARIANT_TYPE("(as)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (!reply) {
    g_warning ("Error getting list of sessions from cgmanager: %s", error->message);
    g_error_free (error);
    return;
  }

  if (g_variant_n_children(reply) < 1) {
    g_variant_unref(reply);
    return;
  }

  array = g_variant_get_child_value(reply, 0);
  if (!array) {
    g_variant_unref(reply);
    return;
  }

  last = (int)g_variant_n_children(array);

  for (i = 0; i < last; i++) {
    g_variant_get_child (array, (gsize)i, "s", &child);
    gchar *rpath = g_strdup_printf("%s/%s", path, child);
    if (!rpath)
        continue;
    cg_kill_recursive(rpath);
  }
  g_variant_unref(array);
  g_variant_unref(reply);

  // kill any tasks here
  reply = g_dbus_connection_call_sync (connection, NULL, "/org/linuxcontainers/cgmanager",
                          "org.linuxcontainers.cgmanager0_0", "GetTasks",
                          g_variant_new("(ss)", "name=systemd", path),
                          G_VARIANT_TYPE("(ai)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);

  if (!reply) {
    g_warning ("Error getting list of sessions from cgmanager: %s", error->message);
    g_error_free (error);
    return;
  }

  if (g_variant_n_children(reply) < 1) {
    g_variant_unref(reply);
    return;
  }

  array = g_variant_get_child_value(reply, 0);
  if (!array) {
    g_variant_unref(reply);
    return;
  }

  last = (int)g_variant_n_children(array);

  for (i = 0; i < last; i++) {
    guint32 pid;
    g_variant_get_child (array, (gsize)i, "i", &pid);
    // XXX todo - should we be nicer here, do a sigterm and wait a bit?
    kill(pid, SIGKILL);
  }
  g_variant_unref(array);
  g_variant_unref(reply);

  // and remove the directory
  cgmanager_remove(path);
  return;
}

void cgmanager_kill (const gchar *scope)
{
  gchar *cgpath;

  cgpath = get_cgpath_from_scope(scope);
  if (!cgpath)
    return;

  cg_kill_recursive (cgpath);
}
