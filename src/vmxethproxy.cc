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
#include <vector>
#include <signal.h>
#include "vmxethproxy.h"
#include "proxycore.h"
#include "socket-moderator.h"
#include "vmxhost.h"
#include "vmxhost-dummy.h"
#include "vmxserver.h"
#include "vmxmonitor.h"
#include "vmxws.h"
#include "vmxprop.h"

volatile int vmx_interrupted;

static bool parse_arguments(vmx_prop_t &pt, int argc, char **argv)
{
	const char *config_file = NULL;

	for (int i = 1; i < argc; i++) {
		char *ai = argv[i];
		if (ai[0] == '-') {
			char c;
			while ((c = *++ai)) {
				switch (c) {
				case 'c':
					if (!config_file && i + 1 < argc)
						config_file = argv[++i];
					else {
						fprintf(stderr, "Error: parsing %s\n", argv[i]);
						return false;
					}
					break;
				default:
					fprintf(stderr, "Error: unknown option %c in %s", c, argv[i]);
					return false;
				}
			}
		}
	}

	if (config_file) {
		if (!load_ptree(pt, config_file)) {
			fprintf(stderr, "Error: loading file '%s'\n", config_file);
			return false;
		}
	}

	return true;
}

struct main_context_s
{
	vmxhost_t *vmxhost = NULL;
	vmxhost_dummy_t *vmxhost_dummy = NULL;
	socket_moderator_t *s = NULL;
	proxycore_t *p = NULL;
	vmxws_t *ws = NULL;

	std::vector<vmxserver_t *> servers;
};

static bool startup(main_context_s &ctx, vmx_prop_ref_t pt)
{
	boost::optional<vmx_prop_t &> pt_vmxhost = pt.get_child_optional("host");
	if (pt_vmxhost && pt_vmxhost->get<bool>("dummy", false)) {
		ctx.vmxhost_dummy = vmxhost_dummy_create(*pt_vmxhost);
		if (!ctx.vmxhost_dummy) {
			fprintf(stderr, "Error: failed to create dummy host.\n");
			return false;
		}
	}
	else if (pt_vmxhost) {
		ctx.vmxhost = vmxhost_create(*pt_vmxhost);
		if (!ctx.vmxhost) {
			fprintf(stderr, "Error: failed to create V-Mixer connection.\n");
			return false;
		}
	}

	boost::optional<vmx_prop_t &> pt_servers = pt.get_child_optional("servers");
	if (pt_servers) {
		for (auto &it : *pt_servers) {
			vmxserver_t *s = vmxserver_create(it.second);
			if (!s) {
				fprintf(stderr, "Error: failed to create a server.\n");
				return false;
			}
			ctx.servers.push_back(s);
		}
	}

	boost::optional<vmx_prop_t &> pt_ws = pt.get_child_optional("ws");
	if (pt_ws) {
		ctx.ws = vmxws_create(*pt_ws);
		if (!ctx.ws) {
			fprintf(stderr, "Error: failed to create websockets instance.\n");
			return false;
		}
	}

	return true;
}

static void sigint_handler(int)
{
	vmx_interrupted = 1;
}

int main(int argc, char **argv)
{
	vmx_prop_t pt;

	vmx_interrupted = 0;
	signal(SIGINT, sigint_handler);
	signal(SIGTERM, sigint_handler);

	if (!parse_arguments(pt, argc, argv)) {
		fprintf(stderr, "Error: failed to parse arguments\n");
		return 1;
	}

	main_context_s ctx;

	if (!startup(ctx, pt)) {
		fprintf(stderr, "Error: failed to initialize.\n");
		return 1;
	}

	ctx.s = socket_moderator_create();
	ctx.p = proxycore_create();

	vmxmonitor_t *monitor = vmxmonitor_create(ctx.p);

	if (ctx.vmxhost)
		vmxhost_start(ctx.vmxhost, ctx.s, ctx.p);

	if (ctx.vmxhost_dummy)
		vmxhost_dummy_start(ctx.vmxhost_dummy, ctx.s, ctx.p);

	for (vmxserver_t *s : ctx.servers) {
		vmxserver_start(s, ctx.s, ctx.p);
	}

	if (ctx.ws)
		vmxws_start(ctx.ws, ctx.s, ctx.p);

	int ret = socket_moderator_mainloop(ctx.s);

	if (ctx.ws)
		vmxws_destroy(ctx.ws);
	for (vmxserver_t *s : ctx.servers)
		vmxserver_destroy(s);
	socket_moderator_destroy(ctx.s);
	if (ctx.vmxhost_dummy)
		vmxhost_dummy_destroy(ctx.vmxhost_dummy);
	if (ctx.vmxhost)
		vmxhost_destroy(ctx.vmxhost);
	vmxmonitor_destroy(monitor);
	proxycore_destroy(ctx.p);

	return ret;
}
