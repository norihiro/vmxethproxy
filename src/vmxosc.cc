#include "lo/lo.h"
#include "vmxethproxy.h"
#include "vmxinstance.h"
#include "socket-moderator.h"
#include "proxycore.h"
#include "src/vmxpacket-identify.h"

struct vmxosc_s
{
	lo_server s;

	// prop
	int port;
};

typedef struct vmxosc_s vmxosc_t;

static int vmxosc_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxosc_t *)data;
	if (!c->s)
		return 0;

	int lo_fd = lo_server_get_socket_fd(c->s);
	if (lo_fd < 0) {
		lo_server_recv_noblock(c->s, 0);
		return 0;
	}

	FD_SET(lo_fd, read_fds);
	return lo_fd + 1;
}

static int vmxosc_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxosc_t *)data;
	if (!c->s)
		return 0;

	int lo_fd = lo_server_get_socket_fd(c->s);
	if (lo_fd >= 0 && FD_ISSET(lo_fd, read_fds)) {
		lo_server_recv_noblock(c->s, 0);
	}

	return 0;
}

static const struct socket_info_s socket_info = {
	vmxosc_set_fds,
	NULL, // timeout_us
	vmxosc_process,
};

void error_handler(int num, const char *msg, const char *path)
{
	fprintf(stderr, "liblo server error %d in path %s: %s\n", num, path, msg);
}

static int generic_handler(const char *path, const char *types, lo_arg **argv,
		int argc, lo_message data, void *user_data)
{
	auto c = (vmxosc_t *)user_data;

	(void)data;

	printf("c=%p message from %s:%s path: <%s>\n", c,
			lo_address_get_hostname(addr_from),
			lo_address_get_port(addr_from),
			path);
	for (int i = 0; i < argc; i++) {
		printf("arg %d '%c' ", i, types[i]);
		lo_arg_pp((lo_type)types[i], argv[i]);
		printf("\n");
	}
	printf("\n");
	fflush(stdout);

	return 0;
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto c = (vmxosc_t *)data;

	// TODO: might need to self-monitor or loopback the command

	if (vmxpacket_is_midi_dt1(packet)) {
		(void)c; // TODO: Implement to send packet
		// TODO: Use lo_send_message_from to send message(s).
	}
}


static void vmxosc_start(void *ctx, socket_moderator_t *ss, proxycore_t *p)
{
	auto c = (vmxosc_t *)ctx;

	char port_s[8] = {0};
	snprintf(port_s, sizeof(port_s) - 1, "%d", c->port & 0xFFFF);
	c->s = lo_server_new(port_s, error_handler);
	if (!c->s) {
		fprintf(stderr, "Error: Failed to create OSC server\n");
		exit(1);
	}

	lo_server_add_method(c->s, NULL, NULL, generic_handler, c);

	socket_moderator_add(ss, &socket_info, c);
	proxycore_add_instance(p, proxy_callback, c, PROXYCORE_INSTANCE_MONITOR);
}

static void vmxosc_set_prop(vmxosc_t *c, vmx_prop_ref_t prop)
{
	c->port = prop.get<int>("port", 7770);
	// TODO: Implement property `proto` to select UDP or TCP
	// TODO: Optionally search listeners using Avahi. TouchOSC, for example, is found as `_osc._udp`.
}

static void *vmxosc_create(vmx_prop_ref_t prop)
{
	auto *c = new vmxosc_s;

	vmxosc_set_prop(c, prop);

	return c;
}

static void vmxosc_destroy(void *ctx)
{
	auto c = (vmxosc_t *)ctx;
	delete c;
}

extern "C" const vmxinstance_type_t vmxosc_type = {
	.id = "osc",
	.create = vmxosc_create,
	.start = vmxosc_start,
	.destroy = vmxosc_destroy,
};
