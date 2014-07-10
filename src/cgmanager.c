/*
 * Copyright © 2014 Canonical Limited
 *
 * lxc: linux Container library
 *
 * Authors:
 * Serge Hallyn <serge.hallyn@canonical.com>
 * Stéphane Graber <stephane.graber@canonical.com>
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

/* for g_critical */
#include <gio/gio.h>

#include "unit.h"

#include <stdio.h>
#include <libgen.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <nih-dbus/dbus_connection.h>
#include <nih-dbus/dbus_interface.h>
#include <nih-dbus/dbus_proxy.h>
#include <nih/alloc.h>
#include <nih/error.h>
#include <nih/string.h>

static NihDBusProxy *cgroup_manager = NULL;

#define CGM_SUPPORTS_GET_ABS 3
#define CGM_SUPPORTS_NAMED 4
#define CGM_SUPPORTS_ALL 5  // supports multiple controllers and "all"
#define CGM_SUPPORTS_RM_ALL 6  // supports multiple controllers and "all" for RemoveOnEmpty

static int32_t api_version;

void cgm_dbus_disconnect(void);

#define CGMANAGER_DBUS_SOCK "unix:path=/sys/fs/cgroup/cgmanager/sock"
bool cgm_dbus_connect(void)
{
	DBusError dbus_error;
	DBusConnection *connection;
	dbus_error_init(&dbus_error);

	connection = dbus_connection_open_private(CGMANAGER_DBUS_SOCK, &dbus_error);
	if (!connection) {
		dbus_error_free(&dbus_error);
		return false;
	}

	dbus_connection_set_exit_on_disconnect(connection, FALSE);
	dbus_error_free(&dbus_error);
	cgroup_manager = nih_dbus_proxy_new(NULL, connection,
				NULL /* p2p */,
				"/org/linuxcontainers/cgmanager", NULL, NULL);
	dbus_connection_unref(connection);
	if (!cgroup_manager) {
		NihError *nerr;
		nerr = nih_error_get();
		g_critical("cgmanager: Error opening proxy: %s", nerr->message);
		nih_free(nerr);
		return false;
	}

    // force fd passing negotiation and get the api version
    if (cgmanager_get_api_version_sync(NULL, cgroup_manager, &api_version) != 0) {
        NihError *nerr;
        nerr = nih_error_get();
        g_critical("Error cgroup manager api version: %s", nerr->message);
        nih_free(nerr);
        cgm_dbus_disconnect();
        return false;
    }
    if (api_version < CGM_SUPPORTS_RM_ALL) {
        g_critical("Cgmanager API version is not new enough\n");
        cgm_dbus_disconnect();
        return false;
        return false;
    }

	return true;
}

void cgm_dbus_disconnect(void)
{
	if (cgroup_manager) {
		dbus_connection_flush(cgroup_manager->connection);
		dbus_connection_close(cgroup_manager->connection);
		nih_free(cgroup_manager);
		cgroup_manager = NULL;
	}
}

bool cgm_create(const char *controller, const char *cgroup_path, int32_t *existed)
{
	if (cgroup_path[0] == '/')
		cgroup_path++;

{
FILE *f = fopen("/tmp/a", "a");
fprintf(f, "%s: called for %s\n", __func__, cgroup_path);
fclose(f);
}
	if ( cgmanager_create_sync(NULL, cgroup_manager, controller,
				       cgroup_path, existed) != 0) {
{
FILE *f = fopen("/tmp/a", "a");
fprintf(f, "%s: error for %s\n", __func__, cgroup_path);
fclose(f);
}
		NihError *nerr;
		nerr = nih_error_get();
		g_error("cgmanager: cgm_create for controller=%s, cgroup_path=%s failed: %s",
		          controller, cgroup_path, nerr->message);
		nih_free(nerr);
		return false;
	}

{
FILE *f = fopen("/tmp/a", "a");
fprintf(f, "%s: done for %s\n", __func__, cgroup_path);
fclose(f);
}
	return true;
}

bool cgm_remove(const char *controller, const char *cgroup_path, int recursive)
{
	int existed;

	if (cgroup_path[0] == '/')
		cgroup_path++;

	if ( cgmanager_remove_sync(NULL, cgroup_manager, controller,
				   cgroup_path, recursive, &existed) != 0) {
		NihError *nerr;
		nerr = nih_error_get();
		g_error("cgmanager: cgm_remove for controller=%s, cgroup_path=%s, recursive=%d failed: %s",
		          controller, cgroup_path, recursive, nerr->message);
		nih_free(nerr);
		return false;
	}

	if (existed == -1) {
		g_error("cgmanager: cgm_remove failed: %s:%s did not exist", controller, cgroup_path);
		return false;
	}
	return true;
}

bool cgm_chown(const char *controller, const char *cgroup_path, int uid, int gid)
{
	if (cgroup_path[0] == '/')
		cgroup_path++;

	if (cgmanager_chown_sync(NULL, cgroup_manager, controller,
			cgroup_path, uid, gid) != 0) {
		NihError *nerr;
		nerr = nih_error_get();
		g_error("cgmanager: cgm_chown for controller=%s, cgroup_path=%s, uid=%d, gid=%d failed: %s",
		          controller, cgroup_path, uid, gid, nerr->message);
		nih_free(nerr);
		return false;
	}

	return true;
}

bool cgm_enter(const char *controller, const char *cgroup_path, pid_t pid)
{
	if (cgroup_path[0] == '/')
		cgroup_path++;

	if (cgmanager_move_pid_sync(NULL, cgroup_manager, controller,
			cgroup_path, pid) != 0) {
		NihError *nerr;
		nerr = nih_error_get();
		g_error("cgmanager: cgm_enter for controller=%s, cgroup_path=%s, pid=%d failed: %s",
		          controller, cgroup_path, pid, nerr->message);
		nih_free(nerr);
		return false;
	}
	return true;
}

bool cgm_remove_on_empty(const char *controller, const char *cgroup_path) {
	if (cgroup_path[0] == '/')
		cgroup_path++;

	if (cgmanager_remove_on_empty_sync(NULL, cgroup_manager, controller, cgroup_path) != 0) {
		NihError *nerr;
		nerr = nih_error_get();
		g_error("cgmanager: remove_on_empty for controller=%s, cgroup_path=%s failed: %s",
		          controller, cgroup_path, nerr->message);
		nih_free(nerr);
		return false;
	}
	return true;
}

void cgmanager_create_all(ScopeUnit *unit)
{
    int32_t e;
    int i;
    const char *path = unit->slice_path;

    if (!cgm_dbus_connect())
        return;
    if (!cgm_create("all", path, &e))
        g_error("Failed to create path %s\n", path);
    if (!cgm_chown("all", path, unit->uid, -1))
        g_error("Failed to chown path %s\n", path);
    for (i = 0; i < unit->npids;  i++) {
        if (!cgm_enter("all", path, (pid_t)unit->pids[i]))
            g_error("Failed to chown path %s\n", path);
    }
    if (!cgm_remove_on_empty("all", path))
        g_error("Failed to set autoremove on path %s\n", path);
    cgm_dbus_disconnect();
}

void cgmanager_remove_all(const char *path)
{
    if (!cgm_dbus_connect())
        return;
    if (!cgm_remove("all", path, 1))
        g_error("Failed to remove path %s\n", path);
    cgm_dbus_disconnect();
}
