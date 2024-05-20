#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <vector>
#include <string>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "misc.h"
#include "proxycore.h"
#include "util/platform.h"
#include "socket-moderator.h"
#include "vmxinstance.h"

typedef struct vmxhost_s vmxhost_t;

struct vmxhost_s
{
	int sock = -1;
	proxycore_t *proxy = NULL;
	uint32_t heartbeat_next_us;
	uint32_t last_received_us;

	std::vector<uint8_t> buf_recv;

	bool connect_autodiscovery();
	bool connect_manual();

	// config
	std::string bcast_discovery;
	std::string host_manual;
	int port_manual;
	uint32_t heartbeat_period_us;
	uint32_t no_response_timeout_us;
};

static void vmxhost_destroy(vmxhost_t *h);

static bool send_req(const char *host, int port)
{
	int s = socket(AF_INET, SOCK_DGRAM, 0);
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));

	// 0000   52 44 44 50 76 31 c0 c4                           RDDPv1..
	char peer0_0[] = {0x52, 0x44, 0x44, 0x50, 0x76, 0x31, (char)(port >> 8), (char)port};

	int bcast = 1;
	setsockopt(s, SOL_SOCKET, SO_BROADCAST, (void *)&bcast, sizeof(bcast));

	addr.sin_family = AF_INET;
	addr.sin_port = htons(9314);
	if (host)
		addr.sin_addr.s_addr = inet_addr(host);
	ssize_t ret = sendto(s, peer0_0, sizeof(peer0_0), 0, (sockaddr *)&addr, sizeof(addr));
	if (ret != sizeof(peer0_0))
		perror("sendto");

	close(s);
	return ret == sizeof(peer0_0);
}

static int create_tcp_listen(const char *bcast)
{
	int s1 = socket(AF_INET, SOCK_STREAM, 0);
	if (s1 < 0) {
		perror("socket");
		return -1;
	}

	int opt = 1;
	setsockopt(s1, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	listen(s1, 1);

	struct sockaddr_in me = {0, 0, 0, 0};
	socklen_t len = sizeof(me);
	getsockname(s1, (sockaddr *)&me, &len);
	int port_listening = ntohs(me.sin_port);
	// TODO: If there is no response, send request several times.
	if (!send_req(bcast, port_listening)) {
		close(s1);
		return -1;
	}

	return s1;
}

bool vmxhost_s::connect_autodiscovery()
{
	int sock_listening = create_tcp_listen(bcast_discovery.c_str());
	if (!sock_listening)
		return false;

	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));
	socklen_t len_server = sizeof(server);
	int sx = accept(sock_listening, (sockaddr *)&server, &len_server);
	if (sx < 0) {
		perror("accept");
		return false;
	}

	uint8_t data[84] = {0};
	int ret = recv(sx, data, sizeof(data), 0);
	close(sx);
	close(sock_listening);
	if (ret != 84)
		return false;

	// M-200i will send 84-byte data and soon it will close the socket.
	// 00000000  52 44 44 50 76 31 83 d8  00 05 5f 68 03 09 00 00   RDDPv1.. .._h....
	// 00000010  00 00 00 00 52 6f 6c 61  6e 64 00 00 00 00 00 00   ....Rola nd......
	// 00000020  00 00 00 00 34 31 32 39  35 00 00 00 31 2c 30 35   ....4129 5...1,05
	// 00000030  31 00 00 00 4d 2d 32 30  30 69 00 00 00 00 00 00   1...M-20 0i......
	// 00000040  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00   ........ ........
	// 00000050  00 00 00 00                                        ....
	// In this example, then iPad will connect to TCP port 33752 (83D8h).
	int port = data[6] << 8 | data[7];
	fprintf(stderr, "Info: vmxhost found '%s' '%s' '%s' '%s'\nConnecting port %d...", data + 0x14, data + 0x24,
		data + 0x2C, data + 0x34, port);

	server.sin_family = AF_INET;
	server.sin_port = htons(port);

	ret = connect(sock, (const sockaddr *)&server, sizeof(server));
	if (ret) {
		fprintf(stderr, "Error: vmxhost: connect failed: %s\n", strerror(errno));
		return false;
	}

	return true;
}

bool vmxhost_s::connect_manual()
{
	struct sockaddr_in server;
	memset(&server, 0, sizeof(server));

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(host_manual.c_str());
	server.sin_port = htons(port_manual);

	int ret = connect(sock, (const sockaddr *)&server, sizeof(server));
	if (ret) {
		fprintf(stderr, "Error: vmxhost: connect to host '%s' port %d failed: %s\n", host_manual.c_str(),
			port_manual, strerror(errno));
		return false;
	}

	return true;
}

static void vmxhost_set_prop(vmxhost_t *h, vmx_prop_ref_t prop)
{
	h->bcast_discovery = prop.get<std::string>("discovery-broadcast", "255.255.255.255");
	h->host_manual = prop.get<std::string>("host", "");
	h->port_manual = prop.get<int>("port", 0);
	h->heartbeat_period_us = (uint32_t)(prop.get<double>("heartbeat_period", 1.0) * 1e6 + 0.5);
	h->no_response_timeout_us = (uint32_t)(prop.get<double>("no_response_timeout", 10.0) * 1e6 + 0.5);
}

static void *vmxhost_create(vmx_prop_ref_t pt)
{
	auto h = new struct vmxhost_s;

	vmxhost_set_prop(h, pt);

	h->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (h->sock < 0) {
		fprintf(stderr, "Error: vmxhost: cannot create socket\n");
		delete h;
		return NULL;
	}

	h->heartbeat_next_us = h->last_received_us = os_gettime_us();

	if (h->host_manual.size() && h->port_manual > 0) {
		if (!h->connect_manual()) {
			vmxhost_destroy(h);
			return NULL;
		}
	}
	else {
		if (!h->connect_autodiscovery()) {
			vmxhost_destroy(h);
			return NULL;
		}
	}

	return h;
}

static int vmxhost_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto h = (struct vmxhost_s *)data;
	if (h->sock < 0) {
		fprintf(stderr, "Error: vmxhost: no valid socket. Interrupting...\n");
		vmx_interrupted = true;
		return 0;
	}

	FD_SET(h->sock, read_fds);
	return h->sock + 1;
}

static void send_heartbeat(struct vmxhost_s *h)
{
	h->heartbeat_next_us += h->heartbeat_period_us;
	if (h->sock < 0)
		return;

	uint8_t data[4] = {0, 0, 0, 0};
	ssize_t ret = send(h->sock, data, sizeof(data), 0);
	if (ret != sizeof(data)) {
		fprintf(stderr, "Error: vmxhost: Failed to send heartbeat to socket %d, errno=%d %s\n", h->sock, errno,
			strerror(errno));
		close(h->sock);
		h->sock = -1;
	}
}

static uint32_t vmxhost_timeout_us(void *data)
{
	auto h = (struct vmxhost_s *)data;

	int32_t until_hb_us = h->heartbeat_next_us - os_gettime_us();
	if (until_hb_us <= 0) {
		send_heartbeat(h);
		until_hb_us += h->heartbeat_period_us;
		if (until_hb_us < 0)
			until_hb_us = 0;
	}

	return until_hb_us;
}

static int process_received(struct vmxhost_s *h)
{
	h->last_received_us = os_gettime_us();
	vmxpacket_t pkt;
	int consumed = parse_tcp_stream(&pkt, &h->buf_recv[0], h->buf_recv.size());
	if (consumed == 0)
		return 0;
	if (consumed < 0) {
		fprintf(stderr, "Error: error to process data %02x %02x %02x %02x, size=%d. Closing...\n",
			(int)h->buf_recv.size(), h->buf_recv.size() > 0 ? h->buf_recv[0] : 0,
			h->buf_recv.size() > 1 ? h->buf_recv[1] : 0, h->buf_recv.size() > 2 ? h->buf_recv[2] : 0,
			h->buf_recv.size() > 3 ? h->buf_recv[3] : 0);
		close(h->sock);
		h->sock = -1;
		return -1;
	}

	h->buf_recv.erase(h->buf_recv.begin(), h->buf_recv.begin() + consumed);

	if (pkt.raw.size() == 0 && pkt.midi.size() == 0)
		return 0;

	proxycore_process_packet(h->proxy, &pkt, h, PROXYCORE_INSTANCE_HOST);

	if (consumed > 0 && h->buf_recv.size() > 0)
		return process_received(h);

	return 0;
}

static int vmxhost_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto h = (struct vmxhost_s *)data;

	if (h->last_received_us) {
		int32_t last_received_us = os_gettime_us() - h->last_received_us;
		if (last_received_us > (int32_t)h->no_response_timeout_us) {
			fprintf(stderr, "Error: vmxhost: no data from the host for %d ms.\n", last_received_us / 1000);
			return 1;
		}
	}

	if (h->sock < 0) {
		fprintf(stderr, "Error: vmxhost: socket error\n");
		return 1;
	}

	if (!FD_ISSET(h->sock, read_fds))
		return 0;

	if (!recv_stream(h->sock, h->buf_recv))
		return 0;

	return process_received(h);
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto h = (struct vmxhost_s *)data;

	if (!send_stream(h->sock, packet)) {
		close(h->sock);
		h->sock = -1;
	}
}

static const struct socket_info_s socket_info = {
	vmxhost_set_fds,
	vmxhost_timeout_us,
	vmxhost_process,
};

static void vmxhost_start(void *ctx, socket_moderator_t *s, proxycore_t *p)
{
	auto h = (vmxhost_t *)ctx;
	h->proxy = p;
	socket_moderator_add(s, &socket_info, h);
	proxycore_add_instance(p, proxy_callback, h, PROXYCORE_INSTANCE_HOST);
}

static void vmxhost_destroy(vmxhost_t *h)
{
	proxycore_remove_instance(h->proxy, proxy_callback, h);
	if (h->sock >= 0)
		close(h->sock);
	delete h;
}

static void vmxhost_destroy_cb(void *ctx)
{
	vmxhost_destroy((vmxhost_t *)ctx);
}

extern "C" const vmxinstance_type_t vmxhost_type = {
	.id = "host-eth",
	.create = vmxhost_create,
	.start = vmxhost_start,
	.destroy = vmxhost_destroy_cb,
};
