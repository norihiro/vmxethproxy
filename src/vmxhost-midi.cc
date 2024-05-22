#include <stdio.h>
#include <vector>
#include <string>
#include <portmidi.h>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "misc.h"
#include "proxycore.h"
#include "util/platform.h"
#include "socket-moderator.h"
#include "vmxinstance.h"

typedef struct vmxhost_midi_s vmxhost_midi_t;

struct vmxhost_midi_s
{
	proxycore_t *proxy = NULL;
	uint32_t last_received_us;

	std::vector<uint8_t> buf_recv;

	PortMidiStream *stream_i = nullptr;
	PortMidiStream *stream_o = nullptr;

	bool connect();
	void disconnect();

	// config
	std::string name_in;
	std::string name_out;
	uint32_t no_response_timeout_us;
};

static void vmxhost_midi_destroy(void *ctx);

static void list_devices()
{
	const PmDeviceInfo *info;
	for (PmDeviceID id = 0; (info = Pm_GetDeviceInfo(id)); id++) {
		char msg[64] = {0};
		if (info->input)
			strcat(msg, " input");
		if (info->output)
			strcat(msg, " output");
		printf("%d\t%s\t%s\n", id, info->name, msg + 1);
	}
}

bool vmxhost_midi_s::connect()
{
	PmDeviceID id_in = -1;
	PmDeviceID id_out = -1;

	const PmDeviceInfo *info;
	for (PmDeviceID id = 0; (info = Pm_GetDeviceInfo(id)); id++) {
		if (id_in == -1 && info->input && (name_in.size() == 0 || name_in == info->name))
			id_in = id;
		if (id_out == -1 && info->output && (name_out.size() == 0 || name_out == info->name))
			id_out = id;
	}

	if (id_in == -1) {
		if (name_in.size())
			fprintf(stderr, "Error: cannot find input device '%s'\n", name_in.c_str());
		else
			fprintf(stderr, "Error: cannot find input device\n");
		return false;
	}

	if (id_out == -1) {
		if (name_out.size())
			fprintf(stderr, "Error: cannot find output device '%s'\n", name_out.c_str());
		else
			fprintf(stderr, "Error: cannot find output device\n");
		return false;
	}

	fprintf(stderr, "Info: opening output device %d and input device %d...\n", id_out, id_in);
	PmError ret;

	ret = Pm_OpenOutput(&stream_o, id_out, NULL, 1024, NULL, NULL, 0);
	if (ret != pmNoError) {
		fprintf(stderr, "Error: cannot open output device, error code %d\n", ret);
		exit(1);
	}

	ret = Pm_OpenInput(&stream_i, id_in, NULL, 1024, NULL, NULL);
	if (ret != pmNoError) {
		Pm_Close(stream_o);
		stream_o = nullptr;
		fprintf(stderr, "Error: cannot open input device, error code %d\n", ret);
		exit(1);
	}

	return true;
}

void vmxhost_midi_s::disconnect()
{
	if (stream_i) {
		Pm_Close(stream_i);
		stream_i = nullptr;
	}

	if (stream_o) {
		Pm_Close(stream_o);
		stream_o = nullptr;
	}
}

static bool set_name(std::string &dest, const std::string &src1, const std::string &src2)
{
	const std::string &src = src1.size() ? src1 : src2;

	if (dest != src) {
		dest = src;
		return true;
	}
	return false;
}

static void vmxhost_midi_set_prop(vmxhost_midi_t *h, vmx_prop_ref_t prop)
{
	std::string name = prop.get<std::string>("name", "");
	std::string name_in = prop.get<std::string>("name_in", "");
	std::string name_out = prop.get<std::string>("name_out", "");

	h->no_response_timeout_us = (uint32_t)(prop.get<double>("no_response_timeout", 10.0) * 1e6 + 0.5);

	if (prop.get<bool>("list_devices", false)) {
		list_devices();
		exit(0);
	}

	bool name_modified = false;

	if (name.size() && name_in.size()) {
		fprintf(stderr, "Error: name (%s) and name_in (%s) are exclusive settings.\n", name.c_str(), name_in.c_str());
		exit(1);
	}

	if (name.size() && name_out.size()) {
		fprintf(stderr, "Error: name (%s) and name_out (%s) are exclusive settings.\n", name.c_str(), name_out.c_str());
		exit(1);
	}

	name_modified |= set_name(h->name_in, name, name_in);
	name_modified |= set_name(h->name_out, name, name_out);
	if (name_modified) {
		h->disconnect();
		h->connect();
	}
}

static void *vmxhost_midi_create(vmx_prop_ref_t pt)
{
	auto h = new struct vmxhost_midi_s;

	vmxhost_midi_set_prop(h, pt);

	return h;
}

static int vmxhost_midi_set_fds(fd_set *, fd_set *, fd_set *, void *data)
{
	auto h = (struct vmxhost_midi_s *)data;
	(void)h;

	return 0;
}

static uint32_t vmxhost_midi_timeout_us(void *data)
{
	auto h = (struct vmxhost_midi_s *)data;

	if (h->stream_i) {
		PmError hasData = Pm_Poll(h->stream_i);
		if (hasData) {
			// TODO: receive stream
			// TODO: process the received data
		}
	}

	return 1280; // time for 4-byte
}

static int process_received(struct vmxhost_midi_s *h)
{
	h->last_received_us = os_gettime_us();
	vmxpacket_t pkt;
	int consumed = 0; // TODO: Process data in `buf_recv` and set into `pkt`.

	if (pkt.raw.size() == 0 && pkt.midi.size() == 0)
		return 0;

	proxycore_process_packet(h->proxy, &pkt, h, PROXYCORE_INSTANCE_HOST);

	if (consumed > 0 && h->buf_recv.size() > 0)
		return process_received(h);

	return 0;
}

static int vmxhost_midi_process(fd_set *, fd_set *, fd_set *, void *data)
{
	auto h = (struct vmxhost_midi_s *)data;
	(void)h;

	return 0;
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto h = (struct vmxhost_midi_s *)data;

	(void)h; (void)packet; // TODO: Send `packet` to `stream_o`.

	// Control Change: Bnh mmh llh
	// Program Change: Cnh pph
	// System Exclusive Message: F0h ... F7h (IDReq, IDReply, RQ1, DT1, MCS stop play record, V-Link)
}

static const struct socket_info_s socket_info = {
	vmxhost_midi_set_fds,
	vmxhost_midi_timeout_us,
	vmxhost_midi_process,
};

static void vmxhost_midi_start(void *ctx, socket_moderator_t *s, proxycore_t *p)
{
	auto h = (vmxhost_midi_t *)ctx;
	h->proxy = p;
	socket_moderator_add(s, &socket_info, h);
	proxycore_add_instance(p, proxy_callback, h, PROXYCORE_INSTANCE_HOST);
}

static void vmxhost_midi_destroy(void *ctx)
{
	auto h = (vmxhost_midi_t *)ctx;
	proxycore_remove_instance(h->proxy, proxy_callback, h);
	h->disconnect();
	delete h;
}

extern "C" const vmxinstance_type_t vmxhost_midi_type = {
	.id = "host-midi",
	.create = vmxhost_midi_create,
	.start = vmxhost_midi_start,
	.destroy = vmxhost_midi_destroy,
};
