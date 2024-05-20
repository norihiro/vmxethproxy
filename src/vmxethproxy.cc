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
#include <list>
#include <string>
#include <signal.h>
#include "vmxethproxy.h"
#include "proxycore.h"
#include "socket-moderator.h"
#include "vmxinstance.h"
#include "vmxprop.h"

volatile int vmx_interrupted;

extern "C" {
#define ITEM(t) extern const vmxinstance_type_t t;
#include "vmxinstance-list.h"
#undef ITEM
}

static const vmxinstance_type_t *vmxinstance_types[] = {
#define ITEM(t) &t,
#include "vmxinstance-list.h"
#undef ITEM
};

struct vmxinstance_s
{
	const vmxinstance_type_t *type;
	void *ctx;
};

static const vmxinstance_type_t *find_type(const char *id)
{
	for (auto it : vmxinstance_types) {
		if (strcmp(it->id, id) == 0)
			return it;
	}
	return NULL;
}

static void create_instance(std::list<vmxinstance_s> &instances, const char *id, vmx_prop_ref_t pt)
{
	vmxinstance_s inst;
	inst.type = find_type(id);
	if (!inst.type) {
		throw "Unknown ID";
	}

	inst.ctx = inst.type->create(pt);
	if (!inst.ctx) {
		throw "Failed to create instance";
	}

	instances.push_back(inst);
}

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
	socket_moderator_t *s = NULL;
	proxycore_t *p = NULL;

	std::list<vmxinstance_s> instances;
};

static bool startup(main_context_s &ctx, vmx_prop_ref_t pt)
{
	boost::optional<vmx_prop_t &> pt_instances = pt.get_child_optional("instances");
	if (pt_instances) {
		for (auto &it : *pt_instances) {
			auto id = it.second.get<std::string>("id", "");
			if (id == "") {
				fprintf(stderr, "Error: cannot find instance id.\n");
				return false;
			}

			vmx_prop_t empty_settings;
			auto pt_settings = it.second.get_child_optional("settings");
			vmx_prop_t &settings = pt_settings ? pt_settings.get() : empty_settings;

			try {
				create_instance(ctx.instances, id.c_str(), settings);
			} catch (const char *e) {
				fprintf(stderr, "Error: id='%s': %s\n", id.c_str(), e);
				return false;
			}
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

	for (vmxinstance_s inst : ctx.instances) {
		inst.type->start(inst.ctx, ctx.s, ctx.p);
	}

	int ret = socket_moderator_mainloop(ctx.s);

	for (auto it = ctx.instances.begin(); it != ctx.instances.end();) {
		it->type->destroy(it->ctx);
		ctx.instances.erase(it++);
	}

	socket_moderator_destroy(ctx.s);
	proxycore_destroy(ctx.p);

	return ret;
}
