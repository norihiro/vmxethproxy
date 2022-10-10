#include "config-macros.h"
#include "proxycore.h"
#include "proxycore-internal.hh"

proxycore_t *proxycore_create()
{
	auto p = new struct proxycore_s;
	return p;
}

void proxycore_destroy(proxycore_t *p)
{
	delete p;
}

void proxycore_add_instance(proxycore_t *p, proxycore_instance_cb callback, void *data, uint32_t flags)
{
	if ((flags & (PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY)) == 0)
		flags |= PROXYCORE_INSTANCE_SECONDARY;
	instance_info_s info = {
		callback,
		data,
		flags,
	};
	p->instances.push_back(info);
}

void proxycore_remove_instance(proxycore_t *p, proxycore_instance_cb callback, void *data)
{
	if (!p)
		return;

	for (auto it = p->instances.begin(); it != p->instances.end(); it++) {
		if (it->callback == callback && it->data == data) {
			p->instances.erase(it);
			return;
		}
	}
}

static bool return_by_proxy(proxycore_t *p, const vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (sender_flags & PROXYCORE_INSTANCE_HOST)
		return false;

	(void)p, (void)packet, (void)sender; // TODO: check the contents of the packet
	return false;
}

static uint32_t determine_receiver_types(proxycore_t *p, const vmxpacket_t *packet, uint32_t sender_flags)
{
	(void)p, (void)packet; // TODO: check the contents of the packet
	bool is_basic = false; // TODO: check the MIDI protocol is

	if (is_basic)
		return PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY | PROXYCORE_INSTANCE_SECONDARY;
	if (sender_flags & PROXYCORE_INSTANCE_HOST)
		return PROXYCORE_INSTANCE_PRIMARY;
	if (sender_flags & PROXYCORE_INSTANCE_PRIMARY)
		return PROXYCORE_INSTANCE_HOST;

	return 0;
}

void proxycore_process_packet(proxycore_t *p, vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (return_by_proxy(p, packet, sender, sender_flags))
		return;

	uint32_t receiver_types = determine_receiver_types(p, packet, sender_flags) | PROXYCORE_INSTANCE_MONITOR;

	// TODO: modify data if necessary

	for (auto &inst : p->instances) {
		if ((inst.flags & receiver_types) && inst.data != sender)
			inst.callback(packet, sender, inst.data);
	}
}
