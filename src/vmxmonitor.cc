#include <cstdio>
#include <string>
#include <vector>
#include <map>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "proxycore.h"
#include "vmxpacket-identify.h"
#include "vmxinstance.h"
#include "socket-moderator.h"
#include "util/platform.h"

typedef struct vmxmonitor_s vmxmonitor_t;

struct vmxmonitor_s
{
	proxycore_t *proxy = nullptr;

	std::map<uint32_t, uint8_t> mem;
	uint32_t last_saved_us = 0;
	bool mem_modified = false;

	// config
	std::string save_file;
	uint32_t save_period_us = 0;
};

static void monitor_midi(const vmxpacket_t *packet, const void *sender)
{
	const auto &midi = packet->midi;
	if (vmxpacket_is_midi_idreq(packet))
		printf("%p: IDReq %#02x\n", sender, midi[2]);
	else if (vmxpacket_is_midi_idreply(packet))
		printf("%p: IDReply %#02x %#02x%02x%02x%02x\n", sender, midi[2], midi[8], midi[9], midi[10], midi[11]);
	else if (vmxpacket_is_midi_rq1(packet))
		printf("%p: RQ1 %#02x %#02x%02x%02x%02x %#02x%02x%02x%02x\n", sender, midi[2], midi[7], midi[8],
		       midi[9], midi[10], midi[11], midi[12], midi[13], midi[14]);
	else if (vmxpacket_is_midi_dt1(packet)) {
		std::string str_data;
		if (midi[7] == 0x10 && midi[8] == 0x10 && midi[9] == 0x01 && midi[10] == 0x00)
			return;
		for (size_t i = 11; i + 1 < midi.size() && midi[i + 1] != 0xF7; i++) {
			char s[8] = {0};
			snprintf(s, sizeof(s) - 1, " %02x", midi[i]);
			str_data += s;
		}
		printf("%p: DT1 %#02x %#02x%02x%02x%02x%s\n", sender, midi[2], midi[7], midi[8], midi[9], midi[10],
		       str_data.c_str());
	}
	else {
		std::string str_data;
		for (size_t i = 0; i + 1 < midi.size(); i++) {
			char s[8] = {0};
			snprintf(s, sizeof(s) - 1, " %02x", midi[i]);
			str_data += s;
		}
		printf("%p: unknown midi%s\n", sender, str_data.c_str());
	}
}

static void proxy_callback(const vmxpacket_t *packet, const void *sender, void *ctx)
{
	auto c = (vmxmonitor_t *)ctx;

	if (vmxpacket_is_midi_dt1(packet)) {
		uint32_t addr = packet->dt_address_packed();
		uint32_t size = packet->dt_size_packed();
		for (uint32_t i = 0; i < size; i++)
			c->mem[addr + i] = packet->midi[11 + i];
		c->mem_modified = true;
	}

	const auto &raw = packet->raw;

	if (raw.size() >= 12 && raw[0] == 0x02) {
		monitor_midi(packet, sender);
	}
	else {
		std::string str_data;
		for (size_t i = 0; i + 1 < raw.size(); i++) {
			char s[8] = {0};
			snprintf(s, sizeof(s) - 1, " %02x", raw[i]);
			str_data += s;
		}
		printf("%p: unknown raw size=%d%s\n", sender, (int)raw.size(), str_data.c_str());
	}
}

static void vmxmonitor_set_prop(vmxmonitor_s *c, vmx_prop_ref_t prop)
{
	c->save_file = prop.get<std::string>("save_file", "");
	c->save_period_us = (uint32_t)(prop.get<float>("save_period", 0.0f) * 1e6);
}

static void *vmxmonitor_create(vmx_prop_ref_t prop)
{
	auto *c = new vmxmonitor_s;

	vmxmonitor_set_prop(c, prop);

	return c;
}

static void save_to_file(const vmxmonitor_t *c)
{
	std::string filename = c->save_file + "~";
	printf("Saving to %s\n", filename.c_str());
	FILE *fp = fopen(filename.c_str(), "w");
	if (!fp) {
		fprintf(stderr, "Error: cannot open '%s' to write\n", filename.c_str());
		return;
	}

	for (const auto x : c->mem)
		fprintf(fp, "%x\t%x\n", x.first, x.second);

	fclose(fp);

	std::rename(filename.c_str(), c->save_file.c_str());
}

static uint32_t vmxmonitor_timeout_us(void *ctx)
{
	auto c = (vmxmonitor_t *)ctx;

	if (!c->save_period_us)
		return std::numeric_limits<uint32_t>::max();

	if (!c->mem_modified)
		return c->save_period_us;

	uint32_t ts = os_gettime_us();
	if ((ts - c->last_saved_us) < c->save_period_us)
		return c->save_period_us - (ts - c->last_saved_us);

	save_to_file(c);
	c->mem_modified = false;
	c->last_saved_us = ts;

	return c->save_period_us;
}

static const struct socket_info_s socket_info = {
	nullptr, // set_fds
	vmxmonitor_timeout_us,
	nullptr, // process
};

static void vmxmonitor_start(void *ctx, socket_moderator_t *s, proxycore_t *p)
{
	auto c = (vmxmonitor_t *)ctx;
	c->proxy = p;
	socket_moderator_add(s, &socket_info, c);
	proxycore_add_instance(p, proxy_callback, c, PROXYCORE_INSTANCE_MONITOR);
}

static void vmxmonitor_destroy(void *ctx)
{
	auto c = (vmxmonitor_t *)ctx;
	if (c->proxy)
		proxycore_remove_instance(c->proxy, proxy_callback, c);

	if (c->save_file.size())
		save_to_file(c);

	delete c;
}

extern "C" const vmxinstance_type_t vmxmonitor_type = {
	.id = "monitor",
	.create = vmxmonitor_create,
	.start = vmxmonitor_start,
	.destroy = vmxmonitor_destroy,
};
