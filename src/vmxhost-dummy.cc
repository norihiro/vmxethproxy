#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <map>
#include <string>
#include <fstream>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "vmxpacket-identify.h"
#include "misc.h"
#include "proxycore.h"
#include "util/platform.h"
#include "socket-moderator.h"
#include "vmxinstance.h"

typedef struct vmxhost_dummy_s vmxhost_dummy_t;

struct vmxhost_dummy_s
{
	proxycore_t *proxy = NULL;

	uint8_t device_id;
	std::map<uint32_t, uint8_t> mem;

	// config
	std::string save_file;
};

static void load_from_file(vmxhost_dummy_t *h, const char *filename);
static void save_to_file(const vmxhost_dummy_t *h, const char *filename);

static void vmxhost_dummy_set_prop(vmxhost_dummy_t *h, vmx_prop_ref_t prop)
{
	std::string load_file = prop.get<std::string>("load_file", "");
	h->save_file = prop.get<std::string>("save_file", "");

	if (load_file.size())
		load_from_file(h, load_file.c_str());
}

static void *vmxhost_dummy_create(vmx_prop_ref_t pt)
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

static void vmxhost_dummy_start(void *ctx, socket_moderator_t *s, proxycore_t *p)
{
	auto h = (vmxhost_dummy_t *)ctx;
	h->proxy = p;
	socket_moderator_add(s, &socket_info, h);
	proxycore_add_instance(p, proxy_callback, h, PROXYCORE_INSTANCE_HOST);
}

static void vmxhost_dummy_destroy(void *ctx)
{
	auto h = (vmxhost_dummy_t *)ctx;
	proxycore_remove_instance(h->proxy, proxy_callback, h);

	if (h->save_file.size())
		save_to_file(h, h->save_file.c_str());
	delete h;
}

static void load_from_file(vmxhost_dummy_t *h, const char *filename)
{
	std::ifstream ifs(filename);
	for (std::string line; std::getline(ifs, line);) {
		uint32_t a, v;
		if (sscanf(line.c_str(), "%x%x", &a, &v) == 2)
			h->mem[a] = v;
	}
}

static void save_to_file(const vmxhost_dummy_t *h, const char *filename)
{
	FILE *fp = fopen(filename, "w");
	if (!fp) {
		fprintf(stderr, "Error: cannot open '%s' to write\n", filename);
		return;
	}

	for (const auto x : h->mem) {
		fprintf(fp, "%x\t%x\n", x.first, x.second);
	}

	fclose(fp);
}

extern "C" const vmxinstance_type_t vmxhost_dummy_type = {
	.id = "host-dummy",
	.create = vmxhost_dummy_create,
	.start = vmxhost_dummy_start,
	.destroy = vmxhost_dummy_destroy,
};
