#if !defined(__APPLE__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE // for pipe2
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <assert.h>
#include <mutex>
#include <list>
#include <queue>
#include <libwebsockets.h>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "socket-moderator.h"
#include "src/vmxpacket-identify.h"
#include "proxycore.h"
#include "rangeset.h"
#include "vmxinstance.h"

// #define DEBUG_THREAD
#ifdef DEBUG_THREAD
#define ASSERT_THREAD(x)                                                                  \
	do {                                                                              \
		if (c->thread_##x != pthread_self())                                      \
			fprintf(stderr, "Error: %s: expected thread " #x "\n", __func__); \
	} while (0)
#else
#define ASSERT_THREAD(x)
#endif

typedef struct vmxws_s vmxws_t;

struct vmxws_s
{
	pthread_t thread_lws;
#ifdef DEBUG_THREAD
	pthread_t thread_main;
#endif
	bool thread_started = 0;
	volatile bool request_exit = 0;

	// variables for the main thread
	proxycore_t *proxy = NULL;
	socket_moderator_t *ss = NULL;

	// variables for the lws thread
	struct lws_protocols protocols[3];
	struct lws_http_mount mounts;
	std::list<struct vmxws_client_s *> clients;

	// variables for inter-thread communication
	std::mutex mutex;
	int pipe_lws2vmx[2];
	struct lws_context *lws_ctx;
	std::queue<vmxpacket_t *> pkts_lws2vmx;
	std::queue<vmxpacket_t *> pkts_vmx2lws;

	// prop
	int port;
	std::string http_origin;
};

struct vmxws_client_s
{
	std::queue<std::string> write_queue;
	struct lws *wsi;

	// variables for the lws thread
	rangeset<uint32_t> addresses;
};

struct session_data_s
{
	struct vmxws_client_s *ctx;
};

#ifndef _GNU_SOURCE
static int pipe2(int pipefd[2], int flags)
{
	int ret = pipe(pipefd);
	if (ret)
		return ret;

	if (flags & O_CLOEXEC) {
		fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
		fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
	}

	return 0;
}
#endif

static void ws_client_broadcast(vmxws_t *c, struct vmxws_client_s *cc_sender, const vmxpacket_t *pkt);
static void vmxws_set_prop(vmxws_t *c, vmx_prop_ref_t prop);

static void *vmxws_create(vmx_prop_ref_t prop)
{
	auto *c = new vmxws_t;
	c->mounts.origin = NULL;
#ifdef DEBUG_THREAD
	c->thread_main = pthread_self();
#endif

	vmxws_set_prop(c, prop);

	return c;
}

static void vmxws_set_prop(vmxws_t *c, vmx_prop_ref_t prop)
{
	c->port = prop.get<int>("port", 7681);
	c->http_origin = prop.get<std::string>("http_origin", "");
}

static void *vmxws_routine(void *);
static int vmxws_set_fds(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);
static int vmxws_process(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);
static void proxy_callback(const vmxpacket_t *packet, const void *, void *data);
static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len);

static const struct socket_info_s socket_info = {
	vmxws_set_fds,
	NULL, // timeout_us
	vmxws_process,
};

static void wait_pipe(int *p)
{
	char c;
	read(p[0], &c, 1);
}

static void notify_pipe(int *p)
{
	char c = 0;
	write(p[1], &c, 1);
}

static void vmxws_start(void *ctx, socket_moderator_t *ss, proxycore_t *p)
{
	auto c = (vmxws_t *)ctx;
	ASSERT_THREAD(main);
	c->proxy = p;
	c->ss = ss;
	socket_moderator_add(ss, &socket_info, c);
	proxycore_add_instance(p, proxy_callback, c, PROXYCORE_INSTANCE_MONITOR);

	pipe2(c->pipe_lws2vmx, O_CLOEXEC);

	pthread_create(&c->thread_lws, NULL, vmxws_routine, c);
	wait_pipe(c->pipe_lws2vmx);
	c->thread_started = true;
}

static void vmxws_destroy(void *ctx)
{
	auto c = (vmxws_t *)ctx;
	ASSERT_THREAD(main);
	c->request_exit = true;

	if (c->ss)
		socket_moderator_remove(c->ss, c);

	if (c->thread_started) {
		std::unique_lock<std::mutex> lock(c->mutex);
		if (c->lws_ctx)
			lws_cancel_service(c->lws_ctx);
		lock.unlock();

		void *retval;
		pthread_join(c->thread_lws, &retval);
	}

	if (c->mounts.origin)
		free((char *)c->mounts.origin);

	close(c->pipe_lws2vmx[0]);
	close(c->pipe_lws2vmx[1]);

	while (c->pkts_lws2vmx.size()) {
		vmxpacket_destroy(c->pkts_lws2vmx.front());
		c->pkts_lws2vmx.pop();
	}

	while (c->pkts_vmx2lws.size()) {
		vmxpacket_destroy(c->pkts_vmx2lws.front());
		c->pkts_vmx2lws.pop();
	}

	delete c;
}

static struct lws_context *vmxws_routine_init(vmxws_t *c)
{
	ASSERT_THREAD(lws);
	fprintf(stderr, "%s: c=%p\n", __func__, c);
	int idx = 0;

	if (c->http_origin.size()) {
		c->protocols[idx] = {"http", lws_callback_http_dummy, 0, 0, 0, NULL, 0};

		memset(&c->mounts, 0, sizeof(c->mounts));
		c->mounts.mountpoint = "/";
		c->mounts.origin = strdup(c->http_origin.c_str());
		c->mounts.def = "index.html";
		c->mounts.origin_protocol = LWSMPRO_FILE;
		c->mounts.mountpoint_len = 1;

		idx++;
	}

	c->protocols[idx++] = {"ws", callback_ws, sizeof(session_data_s), 512, 0, c, 0};
	c->protocols[idx++] = LWS_PROTOCOL_LIST_TERM;
	assert(idx <= (int)(sizeof(c->protocols) / sizeof(*c->protocols)));

	lws_set_log_level(LLL_USER | LLL_ERR | LLL_WARN, NULL);
	struct lws_context_creation_info info;
	memset(&info, 0, sizeof info);
	info.port = c->port;
	info.protocols = c->protocols;
	info.mounts = &c->mounts;
	info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

	return lws_create_context(&info);
}

static void *vmxws_routine(void *data)
{
	auto *c = (vmxws_t *)data;
	int n = 0;
	ASSERT_THREAD(lws);

	struct lws_context *lws_ctx = vmxws_routine_init(c);
	if (!lws_ctx)
		return NULL;

	c->lws_ctx = lws_ctx;

	notify_pipe(c->pipe_lws2vmx);

	while (n >= 0 && !vmx_interrupted && !c->request_exit) {
		n = lws_service(lws_ctx, 0);

		std::unique_lock<std::mutex> lock(c->mutex);
		while (c->pkts_vmx2lws.size()) {
			vmxpacket_t *packet = c->pkts_vmx2lws.front();
			c->pkts_vmx2lws.pop();

			lock.unlock();

			ws_client_broadcast(c, NULL, packet);

			vmxpacket_destroy(packet);

			lock.lock();
		}
	}

	{
		std::lock_guard<std::mutex> lock(c->mutex);
		c->lws_ctx = NULL;
	}

	lws_context_destroy(lws_ctx);

	return NULL;
}

int vmxws_set_fds(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxws_t *)data;
	ASSERT_THREAD(main);
	FD_SET(c->pipe_lws2vmx[0], read_fds);
	return c->pipe_lws2vmx[0] + 1;
}

int vmxws_process(fd_set *read_fds, fd_set *, fd_set *, void *data)
{
	auto c = (vmxws_t *)data;
	ASSERT_THREAD(main);

	if (FD_ISSET(c->pipe_lws2vmx[0], read_fds)) {
		wait_pipe(c->pipe_lws2vmx);

		std::unique_lock<std::mutex> lock(c->mutex);
		while (c->pkts_lws2vmx.size()) {
			vmxpacket_t *packet = c->pkts_lws2vmx.front();
			c->pkts_lws2vmx.pop();

			/* Need to unlock because proxycore_process_packet will
			 * callback ourselves and it causes deadlock. */
			lock.unlock();

			proxycore_process_packet(c->proxy, packet, c, PROXYCORE_INSTANCE_SECONDARY);

			vmxpacket_destroy(packet);

			lock.lock();
		}
	}

	return 0;
}

static void proxy_callback(const vmxpacket_t *packet, const void *, void *data)
{
	auto c = (vmxws_t *)data;
	ASSERT_THREAD(main);

	if (vmxpacket_is_midi_dt1(packet)) {
		vmxpacket_t *pkt = vmxpacket_create();
		*pkt = *packet;

		std::lock_guard<std::mutex> lock(c->mutex);
		c->pkts_vmx2lws.push(pkt);
		if (c->lws_ctx)
			lws_cancel_service(c->lws_ctx);
	}
}

inline static std::string packet_to_string(const vmxpacket_t *pkt)
{
	if (!vmxpacket_is_midi_dt1(pkt))
		return "";

	char s[64];
	s[sizeof(s) - 1] = 0;
	snprintf(s, sizeof(s) - 1, "DT1 %08x", pkt->dt_address_aligned());
	std::string str = s;
	uint32_t size = pkt->dt_size_packed();
	for (uint32_t i = 0; i < size; i++) {
		snprintf(s, sizeof(s) - 1, " %02x", pkt->midi[11 + i]);
		str += s;
	}

	return str;
}

static void ws_client_broadcast(vmxws_t *c, struct vmxws_client_s *cc_sender, const vmxpacket_t *pkt)
{
	ASSERT_THREAD(lws);
	std::string str = packet_to_string(pkt);

	for (struct vmxws_client_s *cc : c->clients) {
		if (cc == cc_sender)
			continue;
		// TODO: check not only the begining but also any addresses are in the range.
		if (cc->addresses.test(pkt->dt_address_packed())) {
			cc->write_queue.push(str);
			lws_callback_on_writable(cc->wsi);
		}
	}
}

static void ws_client_received(vmxws_t *c, struct vmxws_client_s *cc, char *data)
{
	ASSERT_THREAD(lws);
	vmxpacket_t *packet = vmxpacket_create();
	bool rq0 = false;

	// TODO: It's not good to access proxy from the lws thread.
	if (strncmp(data, "RQ0", 3) == 0) {
		data[2] = '1';
		rq0 = true;
	}

	if (!vmxpacket_from_string(packet, data, proxycore_get_host_id(c->proxy))) {
		fprintf(stderr, "Error: cannot create packet.\n");
		return;
	}

	if (strncmp(data, "RQ1", 3) == 0) {
		uint32_t begin = packet->dt_address_packed();
		uint32_t end = begin + packet->dt_size_packed();
		cc->addresses.add(begin, end);
	}

	if (rq0)
		return;

	if (strncmp(data, "DT1", 3) == 0)
		ws_client_broadcast(c, cc, packet);

	std::lock_guard<std::mutex> lock(c->mutex);
	c->pkts_lws2vmx.push(packet);

	notify_pipe(c->pipe_lws2vmx);
}

static int callback_ws(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	auto *sd = (struct session_data_s *)user;
	auto c = (vmxws_t *)lws_get_protocol(wsi)->user;
	ASSERT_THREAD(lws);

	switch (reason) {
	case LWS_CALLBACK_PROTOCOL_INIT:
		break;
	case LWS_CALLBACK_PROTOCOL_DESTROY:
		break;
	case LWS_CALLBACK_ESTABLISHED:
		if (sd) {
			std::lock_guard<std::mutex> lock(c->mutex);
			sd->ctx = new vmxws_client_s;
			sd->ctx->wsi = wsi;
			c->clients.push_back(sd->ctx);
		}
		break;
	case LWS_CALLBACK_CLOSED:
		if (sd->ctx) {
			std::lock_guard<std::mutex> lock(c->mutex);
			c->clients.remove(sd->ctx);
			delete sd->ctx;
			sd->ctx = NULL;
		}
		break;
	case LWS_CALLBACK_SERVER_WRITEABLE:
		if (sd->ctx) {
			while (sd->ctx->write_queue.size()) {
				std::string &str = sd->ctx->write_queue.front();
				std::vector<char> buf(LWS_PRE);
				buf.insert(buf.end(), str.begin(), str.end());

				int r = lws_write(wsi, (uint8_t *)&buf[LWS_PRE], buf.size() - LWS_PRE, LWS_WRITE_TEXT);
				if (r != (int)str.size()) {
					fprintf(stderr, "Error: vmxws %p %p: Failed to write data\n", c, sd->ctx);
					break;
				}

				sd->ctx->write_queue.pop();
			}
		}
		break;
	case LWS_CALLBACK_RECEIVE:
		if (sd->ctx) {
			std::vector<char> data((char *)in, (char *)in + len);
			data.push_back(0);
			ws_client_received(c, sd->ctx, &data[0]);
		}
		break;
	default:
		break;
	}

	return 0;
}

extern "C" const vmxinstance_type_t vmxws_type = {
	.id = "ws",
	.create = vmxws_create,
	.start = vmxws_start,
	.destroy = vmxws_destroy,
};
