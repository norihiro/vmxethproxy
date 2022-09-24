#include <sys/socket.h>
#include <errno.h>
#include <stdio.h>
#include <list>
#include <algorithm>
#include <string>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "vmxethproxy.h"
#include "vmxserver.h"
#include "vmxserver-client.h"
#include "proxycore.h"
#include "util/platform.h"
#include "socket-moderator.h"

struct vmxserver_s
{
	int sock = -1;
	int sock_udp = -1;
	int port_listening = 0;
	bool primary = 0;
	std::string name;
	std::list<vmxserver_client_t *> clients;
	proxycore_t *proxy = NULL;
	socket_moderator_t *ss = NULL;
};

vmxserver_t *vmxserver_create()
{
	auto s = new struct vmxserver_s;

	s->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s->sock < 0) {
		perror("socket");
		delete s;
		return NULL;
	}

	int opt = 1;
	setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	listen(s->sock, 1);

	struct sockaddr_in me;
	memset(&me, 0, sizeof(me));
	socklen_t len = sizeof(me);
	getsockname(s->sock, (sockaddr*)&me, &len);
	s->port_listening = ntohs(me.sin_port);

	s->sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

	struct sockaddr_in addr_udp;
	memset(&addr_udp, 0, sizeof(addr_udp));
	addr_udp.sin_port = htons(9314);
	int ret = bind(s->sock_udp, (const sockaddr*)&addr_udp, sizeof(addr_udp));
	if (ret) {
		perror("bind 9314/udp");
		vmxserver_destroy(s);
		return NULL;
	}

	opt = 1;
	setsockopt(s->sock_udp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	return s;
}

void vmxserver_destroy(vmxserver_t *s)
{
	if (s->sock >= 0)
		close(s->sock);

	for (auto c : s->clients) {
		vmxserver_client_destroy(c);
	}

	delete s;
}

void vmxserver_set_primary(vmxserver_t *s, bool primary)
{
	s->primary = primary;
}

void vmxserver_set_name(vmxserver_t *s, const char *name)
{
	s->name = name;
}

static int vmxserver_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto s = (struct vmxserver_s *)data;
	if (s->sock < 0)
		return 0;

	for (auto it = s->clients.begin(); it != s->clients.end();) {
		auto x = it++;
		vmxserver_client_t *c = *x;
		if (vmxserver_client_has_error(c)) {
			vmxserver_client_destroy(c);
			s->clients.erase(x);
		}
	}

	FD_SET(s->sock, read_fds);
	FD_SET(s->sock_udp, read_fds);
	return std::max(s->sock, s->sock_udp) + 1;
}

static void process_connection(struct vmxserver_s *s)
{
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	socklen_t len_addr = sizeof(addr);

	int sock = accept(s->sock, (sockaddr*)&addr, &len_addr);
	if (sock < 0) {
		perror("accept");
		return;
	}

	uint32_t proxy_flags = 0;
	if (s->primary)
		proxy_flags |= PROXYCORE_INSTANCE_PRIMARY;
	else
		proxy_flags |= PROXYCORE_INSTANCE_SECONDARY;
	auto c = vmxserver_client_create(sock, proxy_flags, s->ss, s->proxy);
	if (c)
		s->clients.push_back(c);
}

static void process_udp(struct vmxserver_s *s)
{
	uint8_t buf[64] = {0};

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	memset(&addr, 0, len);
	ssize_t size = recvfrom(s->sock_udp, buf, sizeof(buf), 0, (struct sockaddr*)&addr, &len);
	if (size < 8 || memcmp(buf, "RDDPv1", 6) != 0)
		return;

	uint32_t port = (buf[6] << 8) | buf[7];
	addr.sin_port = htons(port);

	char peer0_0[] = {
		0x52, 0x44, 0x44, 0x50, 0x76, 0x31, 0x00, 0x00, 0x00, 0x05, 0x5f, 0x68, 0x03, 0x09, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x52, 0x6f, 0x6c, 0x61, 0x6e, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x34, 0x31, 0x32, 0x39, 0x35, 0x00, 0x00, 0x00, 0x31, 0x2c, 0x30, 0x35,
		0x31, 0x00, 0x00, 0x00, 0x4d, 0x2d, 0x32, 0x30, 0x30, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00 };
	peer0_0[6] = s->port_listening >> 8;
	peer0_0[7] = (s->port_listening & 0xFF);
	if (s->name.size() > 0)
		strncpy(peer0_0 + 0x34, s->name.c_str(), 14);

	int sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock_tcp) {
		perror("Error: socket tcp");
		return;
	}

	int ret = connect(sock_tcp, (const sockaddr*)&addr, sizeof(addr));
	if (ret) {
		perror("Error: connect tcp");
		close(sock_tcp);
		return;
	}

	size = send(sock_tcp, peer0_0, 84, 0);
	if (size != 84) {
		perror("send");
	}

	close(sock_tcp);
}

static int vmxserver_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto s = (struct vmxserver_s *)data;
	if (s->sock < 0)
		return 0;

	if (FD_ISSET(s->sock, read_fds))
		process_connection(s);

	if (FD_ISSET(s->sock_udp, read_fds))
		process_udp(s);

	return 0;
}

static struct socket_info_s socket_info = {
	vmxserver_set_fds,
	NULL, // timeout_us
	vmxserver_process,
};

void vmxserver_start(vmxserver_t *s, socket_moderator_t *ss, proxycore_t *p)
{
	s->proxy = p;
	s->ss = ss;
	socket_moderator_add(ss, &socket_info, s);
}
