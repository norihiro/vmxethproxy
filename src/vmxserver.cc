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
	bool primary = false;
	proxycore_t *proxy = NULL;
	socket_moderator_t *ss = NULL;

	// properties
	std::string name;
	std::list<vmxserver_client_t *> clients;
	int port_listen;
	vmx_prop_t prop;
	bool listen_udp = true;
};

void vmxserver_set_prop(vmxserver_t *s, vmx_prop_ref_t prop)
{
	s->primary = prop.get<bool>("primary", false);
	s->name = prop.get<std::string>("name", "M-200i-1");
	s->port_listen = prop.get<int>("port", 0);
	s->listen_udp = prop.get<bool>("listen-udp", true);
	s->prop = prop;
}

vmxserver_t *vmxserver_create(vmx_prop_ref_t prop)
{
	auto s = new struct vmxserver_s;

	vmxserver_set_prop(s, prop);

	s->sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s->sock < 0) {
		perror("socket");
		delete s;
		return NULL;
	}

	int opt = 1;
	setsockopt(s->sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	if (s->port_listen > 0) {
		struct sockaddr_in me;
		memset(&me, 0, sizeof(me));
		me.sin_port = htons(s->port_listen);
		int ret = bind(s->sock, (const sockaddr *)&me, sizeof(me));
		if (ret) {
			perror("bind tcp");
			vmxserver_destroy(s);
			return NULL;
		}

		s->port_listening = s->port_listen;
	}

	listen(s->sock, 1);

	if (s->port_listen <= 0) {
		struct sockaddr_in me;
		memset(&me, 0, sizeof(me));
		socklen_t len = sizeof(me);
		getsockname(s->sock, (sockaddr *)&me, &len);
		s->port_listening = ntohs(me.sin_port);
	}

	if (s->listen_udp) {
		s->sock_udp = socket(AF_INET, SOCK_DGRAM, 0);

		struct sockaddr_in addr_udp;
		memset(&addr_udp, 0, sizeof(addr_udp));
		addr_udp.sin_port = htons(9314);
		int ret = bind(s->sock_udp, (const sockaddr *)&addr_udp, sizeof(addr_udp));
		if (ret) {
			perror("bind 9314/udp");
			vmxserver_destroy(s);
			return NULL;
		}

		opt = 1;
		setsockopt(s->sock_udp, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));
	}
	else {
		s->sock_udp = -1;
	}

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

	int sock = accept(s->sock, (sockaddr *)&addr, &len_addr);
	if (sock < 0) {
		perror("accept");
		return;
	}

	uint32_t proxy_flags = 0;
	if (s->primary)
		proxy_flags |= PROXYCORE_INSTANCE_PRIMARY;
	else
		proxy_flags |= PROXYCORE_INSTANCE_SECONDARY;
	auto c = vmxserver_client_create(sock, proxy_flags, s->ss, s->proxy, s->prop);
	if (c)
		s->clients.push_back(c);
}

static void process_udp(struct vmxserver_s *s)
{
	uint8_t buf[64] = {0};

	struct sockaddr_in addr;
	socklen_t len = sizeof(addr);
	memset(&addr, 0, len);
	ssize_t size = recvfrom(s->sock_udp, buf, sizeof(buf), 0, (struct sockaddr *)&addr, &len);
	if (size < 8 || memcmp(buf, "RDDPv1", 6) != 0)
		return;

	uint32_t port = (buf[6] << 8) | buf[7];
	addr.sin_port = htons(port);

	// clang-format off
	char peer0_0[] = {
		0x52, 0x44, 0x44, 0x50, 0x76, 0x31, 0x00, 0x00, 0x00, 0x05, 0x5f, 0x68, 0x03, 0x09, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x52, 0x6f, 0x6c, 0x61, 0x6e, 0x64, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x34, 0x31, 0x32, 0x39, 0x35, 0x00, 0x00, 0x00, 0x31, 0x2c, 0x30, 0x35,
		0x31, 0x00, 0x00, 0x00, 0x4d, 0x2d, 0x32, 0x30, 0x30, 0x69, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00 };
	// clang-format on
	peer0_0[6] = s->port_listening >> 8;
	peer0_0[7] = (s->port_listening & 0xFF);
	if (s->name.size() > 0)
		strncpy(peer0_0 + 0x34, s->name.c_str(), 14);

	int sock_tcp = socket(AF_INET, SOCK_STREAM, 0);
	if (!sock_tcp) {
		perror("Error: socket tcp");
		return;
	}

	int ret = connect(sock_tcp, (const sockaddr *)&addr, sizeof(addr));
	if (ret) {
		perror("Error: connect tcp");
		close(sock_tcp);
		return;
	}

	size = send(sock_tcp, peer0_0, 84, MSG_NOSIGNAL);
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

static const struct socket_info_s socket_info = {
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
