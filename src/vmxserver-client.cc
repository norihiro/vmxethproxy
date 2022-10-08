#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include "vmxethproxy.h"
#include "vmxserver-client.h"
#include "util/platform.h"
#include "proxycore.h"
#include "vmxpacket.h"
#include "socket-moderator.h"
#include "misc.h"

struct vmxserver_client_s
{
	int sock;
	uint32_t proxy_flags;
	socket_moderator_t *ss;
	proxycore_t *proxy;
	uint32_t heartbeat_next_us;

	std::vector<uint8_t> buf_recv;
};

static void proxy_callback(const vmxpacket_t *packet, const void *sender, void *data);
static int client_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data);
static uint32_t client_timeout_us(void *data);
static int client_process(fd_set *read_fds, fd_set *, fd_set *, void *data);

static const struct socket_info_s socket_info = {
	client_set_fds,
	client_timeout_us,
	client_process,
};

vmxserver_client_t *vmxserver_client_create(int sock, uint32_t proxy_flags, socket_moderator_t *ss, proxycore_t *p)
{
	auto c = new struct vmxserver_client_s;

	c->sock = sock;
	c->proxy_flags = proxy_flags;
	c->ss = ss;
	c->proxy = p;
	c->heartbeat_next_us = os_gettime_us();

	socket_moderator_add(ss, &socket_info, c);
	proxycore_add_instance(p, proxy_callback, c, proxy_flags);

	return c;
}

void vmxserver_client_destroy(vmxserver_client_t *c)
{
	socket_moderator_remove(c->ss, c);
	proxycore_remove_instance(c->proxy, proxy_callback, c);

	if (c->sock >= 0)
		close(c->sock);

	delete c;
}

bool vmxserver_client_has_error(vmxserver_client_t *c)
{
	return c->sock < 0;
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto c = (vmxserver_client_t *)data;

	if (!send_stream(c->sock, packet)) {
		close(c->sock);
		c->sock = -1;
	}
}

static void send_heartbeat(vmxserver_client_t *c)
{
	c->heartbeat_next_us += 1000000;
	if (c->sock < 0)
		return;

	uint8_t data[4] = {0, 0, 0, 0};
	ssize_t ret = send(c->sock, data, sizeof(data), MSG_NOSIGNAL);
	if (ret != sizeof(data)) {
		perror("vmxserver_client: send");
		close(c->sock);
		c->sock = -1;
	}
}

int client_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxserver_client_t *)data;

	int32_t until_hb_us = c->heartbeat_next_us - os_gettime_us();
	if (until_hb_us <= 0)
		send_heartbeat(c);

	if (c->sock < 0)
		return 0;
	FD_SET(c->sock, read_fds);

	return c->sock + 1;
}

uint32_t client_timeout_us(void *data)
{
	auto c = (vmxserver_client_t *)data;

	if (c->sock < 0)
		return -1;

	int32_t until_hb_us = c->heartbeat_next_us - os_gettime_us();
	if (until_hb_us < 100)
		until_hb_us = 100;

	return until_hb_us;
}

static int process_received(vmxserver_client_t *c)
{
	vmxpacket_t pkt;
	int consumed = parse_tcp_stream(&pkt, &c->buf_recv[0], c->buf_recv.size());
	if (consumed == 0)
		return 0;
	if (consumed < 0) {
		fprintf(stderr, "Error: error to process data %02x %02x %02x %02x, size=%d. Closing...\n",
			(int)c->buf_recv.size(), c->buf_recv.size() > 0 ? c->buf_recv[0] : 0,
			c->buf_recv.size() > 1 ? c->buf_recv[1] : 0, c->buf_recv.size() > 2 ? c->buf_recv[2] : 0,
			c->buf_recv.size() > 3 ? c->buf_recv[3] : 0);
		close(c->sock);
		c->sock = -1;
		return 0;
	}

	c->buf_recv.erase(c->buf_recv.begin(), c->buf_recv.begin() + consumed);

	if (pkt.raw.size() == 0 && pkt.midi.size() == 0)
		return 0;
	// TODO: set flags instead of hardcode PROXYCORE_INSTANCE_PRIMARY.
	proxycore_process_packet(c->proxy, &pkt, c, PROXYCORE_INSTANCE_PRIMARY);

	if (consumed > 0 && c->buf_recv.size() > 0)
		return process_received(c);

	return 0;
}

int client_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxserver_client_t *)data;
	if (c->sock < 0)
		return 0;

	if (!FD_ISSET(c->sock, read_fds))
		return 0;

	if (!recv_stream(c->sock, c->buf_recv))
		return 0;

	return process_received(c);
}
