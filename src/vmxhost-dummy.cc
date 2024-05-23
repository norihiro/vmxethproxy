#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "vmxhost-dummy.h"
#include "vmxpacket-identify.h"
#include "misc.h"
#include "proxycore.h"
#include "util/platform.h"
#include "socket-moderator.h"

struct vmxhost_dummy_s
{
	proxycore_t *proxy = NULL;

	uint8_t device_id;
	std::map<uint32_t, uint8_t> mem;
};

void vmxhost_dummy_set_prop(vmxhost_dummy_t *h, vmx_prop_ref_t prop)
{
	(void)h;
	(void)prop;
}

vmxhost_dummy_t *vmxhost_dummy_create(vmx_prop_ref_t pt)
{
	auto h = new struct vmxhost_dummy_s;

	vmxhost_dummy_set_prop(h, pt);

	return h;
}

static int vmxhost_dummy_set_fds(fd_set *, fd_set *, fd_set *, void *)
{
	return 0;
}

static uint32_t vmxhost_dummy_timeout_us(void *)
{
	return 10000;
}

static int vmxhost_dummy_process(fd_set *, fd_set *, fd_set *, void *data)
{
	auto h = (struct vmxhost_dummy_s *)data;
	(void)h;

	return 0;
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto h = (struct vmxhost_dummy_s *)data;

	if (vmxpacket_is_midi_dt1(packet)) {
		uint32_t addr = packet->dt_address_packed();
		uint32_t size = packet->dt_size_packed();
		fprintf(stderr, "vmxhost-dummy: DT1 %x %x\n", packet->dt_address_aligned(), packet->dt_size_packed());
		for (uint32_t i = 0; i < size; i++)
			h->mem[addr + i] = packet->midi[11 + i];
	}
	else if (vmxpacket_is_midi_rq1(packet)) {
		uint32_t addr = packet->dt_address_packed();
		uint32_t size = packet->dt_size_packed();
		fprintf(stderr, "vmxhost-dummy: RQ1 %x %x\n", packet->dt_address_aligned(), packet->dt_size_packed());
		vmxpacket_t res;
		std::vector<uint8_t> &midi = res.modify_midi();
		midi = std::vector<uint8_t>{
			0xF0,
			0x41,
			h->device_id,
			0x00,
			0x00,
			0x24,
			0x12, // DT1
			(uint8_t)((addr >> 21) & 0x7F),
			(uint8_t)((addr >> 14) & 0x7F), // address MSP
			(uint8_t)((addr >> 7) & 0x7F),
			(uint8_t)(addr & 0x7F), // address LSB
		};

		for (uint32_t i = 0; i < size; i++)
			midi.push_back(h->mem[addr + i]);

		add_midi_sum_eox(midi, 7);
		res.make_raw();

		proxycore_process_packet(h->proxy, &res, h, PROXYCORE_INSTANCE_HOST);
	}
	else if (vmxpacket_is_midi_idreq(packet)) {
		fprintf(stderr, "vmxhost-dummy: IDReq\n");
		vmxpacket_t res;
		res.modify_midi() = std::vector<uint8_t>{0xF0, 0x7E, h->device_id, // Device ID
							 0x06, 0x02,               // ID reply
							 0x41, 0x24, 0x02,
							 0x00, 0x03, // manufacturer and device ID
							 0,    0,    0,
							 0, // software revision
							 0xF7};
		res.make_raw();

		proxycore_process_packet(h->proxy, &res, h, PROXYCORE_INSTANCE_HOST);
	}
}

static const struct socket_info_s socket_info = {
	vmxhost_dummy_set_fds,
	vmxhost_dummy_timeout_us,
	vmxhost_dummy_process,
};

void vmxhost_dummy_start(vmxhost_dummy_t *h, socket_moderator_t *s, proxycore_t *p)
{
	h->proxy = p;
	socket_moderator_add(s, &socket_info, h);
	proxycore_add_instance(p, proxy_callback, h, PROXYCORE_INSTANCE_HOST);
}

void vmxhost_dummy_destroy(vmxhost_dummy_t *h)
{
	proxycore_remove_instance(h->proxy, proxy_callback, h);
	delete h;
}
