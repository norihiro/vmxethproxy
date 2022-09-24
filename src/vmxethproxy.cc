/* Copyright (C) 2022 Norihiro Kamae <norihiro@nagater.net>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <stdio.h>
#include "vmxethproxy.h"
#include "proxycore.h"
#include "socket-moderator.h"
#include "vmxhost.h"
#include "vmxserver.h"

int main(int argc, char **argv)
{
	(void)argc; // TODO: remove me
	(void)argv; // TODO: remove me

	// TODO: Code below is for developing.
	vmxhost_t *vmxhost = vmxhost_create();
	if (!vmxhost) {
		fprintf(stderr, "Error: failed to create V-Mixer connection.\n");
		return 1;
	}
	socket_moderator_t *s = socket_moderator_create();
	proxycore_t *p = proxycore_create();

	vmxserver_t *server_p = vmxserver_create();
	if (!server_p) {
		fprintf(stderr, "Error: failed to create listening server.\n");
		return 1;
	}
	vmxserver_set_primary(server_p, true);
	vmxserver_set_name(server_p, "M-200i-1");

	vmxhost_start(vmxhost, s, p);

	vmxserver_start(server_p, s, p);

	int ret = socket_moderator_mainloop(s);

	vmxserver_destroy(server_p);
	socket_moderator_destroy(s);
	vmxhost_destroy(vmxhost);
	proxycore_destroy(p);

	return ret;
}
