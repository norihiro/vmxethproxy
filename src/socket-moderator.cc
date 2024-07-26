#include <list>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include "vmxethproxy.h"
#include "socket-moderator.h"

struct info_s
{
	const struct socket_info_s *info;
	void *data;
	bool removed;
	info_s(const struct socket_info_s *_info, void *_data)
	{
		info = _info;
		data = _data;
		removed = false;
	}
};

struct socket_moderator_s
{
	std::list<struct info_s> infos;
};

socket_moderator_t *socket_moderator_create()
{
	auto s = new struct socket_moderator_s;
	return s;
}

void socket_moderator_destroy(socket_moderator_t *s)
{
	delete s;
}

void socket_moderator_add(socket_moderator_t *s, const struct socket_info_s *info, void *data)
{
	s->infos.push_back(info_s(info, data));
}

void socket_moderator_remove(socket_moderator_t *s, void *data)
{
	for (auto it = s->infos.begin(); it != s->infos.end(); it++) {
		if (it->data == data) {
			it->removed = true;
			return;
		}
	}
}

int socket_moderator_mainloop(socket_moderator_t *s)
{
	bool cont = true;

	signal(SIGPIPE, SIG_IGN);

	while (cont && !vmx_interrupted) {
		fd_set read_fds, write_fds, except_fds;
		FD_ZERO(&read_fds);
		FD_ZERO(&write_fds);
		FD_ZERO(&except_fds);

		int nfds = 0;
		uint32_t timeout_us = std::numeric_limits<uint32_t>::max();

		for (auto &info : s->infos) {
			if (info.removed)
				continue;
			if (info.info->set_fds) {
				int fd = info.info->set_fds(&read_fds, &write_fds, &except_fds, info.data);
				if (fd > nfds)
					nfds = fd;
			}

			if (info.info->timeout_us) {
				uint32_t to = info.info->timeout_us(info.data);
				if (to < timeout_us)
					timeout_us = to;
			}
		}

		struct timeval tv = {(int)(timeout_us / 1000000), (int)(timeout_us % 1000000)};
		int ret = select(nfds, &read_fds, &write_fds, &except_fds, &tv);
		if (ret < 0) {
			if (errno != EINTR)
				perror("Error: select");
			continue;
		}

		for (auto &info : s->infos) {
			if (info.removed)
				continue;
			if (info.info->process) {
				int ret = info.info->process(&read_fds, &write_fds, &except_fds, info.data);
				if (ret) {
					fprintf(stderr, "Info: exiting requested by %p\n", info.data);
					cont = false;
				}
			}
		}

		for (auto it = s->infos.begin(); it != s->infos.end();) {
			auto x = it++;
			if (x->removed)
				s->infos.erase(x);
		}
	}

	return 0;
}
