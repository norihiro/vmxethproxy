#ifndef _GNU_SOURCE
#define _GNU_SOURCE // for pipe2
#endif
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <pthread.h>
#include <mutex>
#include <list>
#include <queue>
#include <libwebsockets.h>
#include "vmxethproxy.h"
#include "vmxws.h"
#include "vmxpacket.h"
#include "src/vmxpacket-identify.h"
#include "proxycore.h"

// #define DEBUG_THREAD
#ifdef DEBUG_THREAD
#define ASSERT_THREAD(x) do { if (c->thread_##x != pthread_self()) fprintf(stderr, "Error: %s: expected thread " #x"\n", __func__); } while (0)
#else
#define ASSERT_THREAD(x)
#endif

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
	struct lws_context *lws_ctx;
	struct lws_protocols protocols[3];
	struct lws_http_mount mounts;
	std::list<struct vmxws_client_s *> clients;

	// variables for inter-thread communication
	std::mutex mutex;
	int pipe_lws2vmx[2];
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
};

struct session_data_s
{
	struct vmxws_client_s *ctx;
};

static void ws_client_broadcast(vmxws_t *c, struct vmxws_client_s *cc_sender, const vmxpacket_t *pkt);

vmxws_t *vmxws_create(vmx_prop_ref_t prop)
{
	auto *c = new vmxws_t;
	c->mounts.origin = NULL;
#ifdef DEBUG_THREAD
	c->thread_main = pthread_self();
#endif

	vmxws_set_prop(c, prop);

	return c;
}

void vmxws_set_prop(vmxws_t *c, vmx_prop_ref_t prop)
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

void vmxws_start(vmxws_t *c, socket_moderator_t *ss, proxycore_t *p)
{
	ASSERT_THREAD(main);
	c->proxy = p;
	c->ss = ss;
	socket_moderator_add(ss, &socket_info, c);
	proxycore_add_instance(p, proxy_callback, c, PROXYCORE_INSTANCE_HOST);

	pipe2(c->pipe_lws2vmx, O_CLOEXEC);

	pthread_create(&c->thread_lws, NULL, vmxws_routine, c);
	wait_pipe(c->pipe_lws2vmx);
	c->thread_started = true;
}

void vmxws_destroy(vmxws_t *c)
{
	ASSERT_THREAD(main);
	c->request_exit = true;

	if (c->ss)
		socket_moderator_remove(c->ss, c);

	if (c->thread_started) {
		lws_cancel_service(c->lws_ctx);
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
	int n;
	ASSERT_THREAD(lws);

	c->lws_ctx = vmxws_routine_init(c);
	if (!c->lws_ctx)
		return NULL;

	notify_pipe(c->pipe_lws2vmx);

	while (n >= 0 && !vmx_interrupted && !c->request_exit) {
		n = lws_service(c->lws_ctx, 0);

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

	lws_context_destroy(c->lws_ctx);
	c->lws_ctx = NULL;

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

		std::lock_guard<std::mutex> lock(c->mutex);
		while (c->pkts_lws2vmx.size()) {
			vmxpacket_t *packet = c->pkts_lws2vmx.front();
			c->pkts_lws2vmx.pop();

			proxycore_process_packet(c->proxy, packet, c, PROXYCORE_INSTANCE_SECONDARY);

			vmxpacket_destroy(packet);
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
		cc->write_queue.push(str);
		lws_callback_on_writable(cc->wsi);
	}
}

static void ws_client_received(vmxws_t *c, struct vmxws_client_s *cc, const char *data)
{
	ASSERT_THREAD(lws);
	vmxpacket_t *packet = vmxpacket_create();
	// TODO: It's not good to access proxy from the lws thread.
	if (!vmxpacket_from_string(packet, data, proxycore_get_host_id(c->proxy))) {
		fprintf(stderr, "Error: cannot create packet.\n");
		return;
	}

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
		fprintf(stderr, "%s: LWS_CALLBACK_SERVER_WRITEABLE cc=%p\n", __func__, sd->ctx);
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
		fprintf(stderr, "%s: LWS_CALLBACK_RECEIVE\n", __func__);
		if (sd->ctx) {
			std::string data((char *)in, (char *)in + len);
			ws_client_received(c, sd->ctx, data.c_str());
		}
		break;
	default:
		break;
	}

	return 0;
}