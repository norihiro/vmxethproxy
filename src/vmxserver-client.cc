#include <sys/socket.h>
#include <unistd.h>
#include <cstdio>
#include <cstdint>
#include "vmxethproxy.h"
#include "vmxserver-client.h"
#include "util/platform.h"
#include "proxycore.h"
#include "socket-moderator.h"

struct vmxserver_client_s
{
	int sock;
	uint32_t proxy_flags;
	socket_moderator_t *ss;
	proxycore_t *proxy;
	uint32_t heartbeat_next_us;
};

static void proxy_callback(const vmxpacket_t *packet, const void *sender, void *data);
static int client_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data);
static uint32_t client_timeout_us(void *data);
static int client_process(fd_set *read_fds, fd_set *, fd_set *, void *data);

static struct socket_info_s socket_info = {
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

	(void)c, (void)packet, (void)data; // TODO: implement me
}

static void send_heartbeat(vmxserver_client_t *c)
{
	c->heartbeat_next_us += 1000000;
	if (c->sock < 0)
		return;

	uint8_t data[4] = {0, 0, 0, 0};
	ssize_t ret = send(c->sock, data, sizeof(data), 0);
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

int client_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxserver_client_t *)data;

	if (!FD_ISSET(c->sock, read_fds))
		return 0;

	// TODO: implement me

	return 0;
}
