#pragma once

#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*socket_info_set_fds_cb)(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);
typedef int (*socket_info_process_cb)(fd_set *read_fds, fd_set *write_fds, fd_set *except_fds, void *data);

struct socket_info_s
{
	socket_info_set_fds_cb set_fds;
	uint32_t (*timeout_us)(void *data);
	socket_info_process_cb process;
};

socket_moderator_t *socket_moderator_create();
void socket_moderator_destroy(socket_moderator_t *);
int socket_moderator_mainloop(socket_moderator_t *);

void socket_moderator_add(socket_moderator_t *s, const struct socket_info_s *info, void *data);
void socket_moderator_remove(socket_moderator_t *s, void *data);

#ifdef __cplusplus
}
#endif
