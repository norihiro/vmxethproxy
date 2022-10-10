#include "vmxethproxy.h"
#include "proxycore.h"
#include "proxycore-internal.hh"
#include "vmxpacket.h"
#include "vmxpacket-identify.h"

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

static bool reply_to_sender(proxycore_t *p, const vmxpacket_t *packet, const void *sender)
{
	for (auto &inst : p->instances) {
		if (inst.data == sender) {
			inst.callback(packet, NULL, inst.data);
			return true;
		}
	}
	return false;
}

static bool return_by_proxy(proxycore_t *p, const vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (sender_flags & PROXYCORE_INSTANCE_HOST)
		return false;

	if ((sender_flags & (PROXYCORE_INSTANCE_HOST | PROXYCORE_INSTANCE_PRIMARY)) == 0) {
		// This device is not supposed to be there. Hide from the host nor primary.

		if (vmxpacket_is_midi_idreq(packet) && p->host_id >= 0) {
			vmxpacket_t pkt_reply;
			// clang-format off
			pkt_reply.modify_midi() = std::vector<uint8_t>{
				0xF0, 0x7E, (uint8_t)p->host_id, // Device ID
				0x06, 0x02, // ID reply
				0x41, 0x24, 0x02, 0x00, 0x03, // manufacturer and device ID
				p->host_id_revision[0], p->host_id_revision[1], p->host_id_revision[2],
				p->host_id_revision[3], // software revision
				0xF7 };
			// clang-format on
			pkt_reply.make_raw();

			if (reply_to_sender(p, &pkt_reply, sender))
				return true;
		}

		// TODO: return reserved address
	}

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

static bool preprocess_packet(proxycore_t *p, vmxpacket_t *packet, uint32_t sender_flags)
{
	if ((sender_flags & PROXYCORE_INSTANCE_HOST) && vmxpacket_is_midi_idreply(packet)) {
		p->host_id = packet->midi[2];
		p->host_id_revision[0] = packet->midi[10];
		p->host_id_revision[1] = packet->midi[11];
		p->host_id_revision[2] = packet->midi[12];
		p->host_id_revision[3] = packet->midi[13];
	}

	return true;
}

void proxycore_process_packet(proxycore_t *p, vmxpacket_t *packet, const void *sender, uint32_t sender_flags)
{
	if (return_by_proxy(p, packet, sender, sender_flags))
		return;

	uint32_t receiver_types = determine_receiver_types(p, packet, sender_flags) | PROXYCORE_INSTANCE_MONITOR;

	if (!preprocess_packet(p, packet, sender_flags))
		return;

	// TODO: modify data if necessary

	for (auto &inst : p->instances) {
		if ((inst.flags & receiver_types) && inst.data != sender)
			inst.callback(packet, sender, inst.data);
	}
}

int proxycore_get_host_id(proxycore_t *p)
{
	return p->host_id;
}
