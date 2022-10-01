#include <cstdio>
#include <string>
#include <vector>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "proxycore.h"
#include "vmxmonitor.h"
#include "vmxpacket-identify.h"

struct vmxmonitor_s
{
	proxycore_t *proxy;
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

static void proxy_callback(const vmxpacket_t *packet, const void *sender, void *)
{
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

vmxmonitor_t *vmxmonitor_create(proxycore_t *p)
{
	auto *c = new vmxmonitor_s;

	c->proxy = p;
	proxycore_add_instance(p, proxy_callback, c, PROXYCORE_INSTANCE_MONITOR);

	return c;
}

void vmxmonitor_destroy(vmxmonitor_t *c)
{
	proxycore_remove_instance(c->proxy, proxy_callback, c);

	delete c;
}
