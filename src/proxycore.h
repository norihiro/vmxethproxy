#pragma once

#include <cstdint>
#include <sys/select.h>
#include "vmxethproxy.h"

#ifdef __cplusplus
extern "C" {
#endif

proxycore_t *proxycore_create();
void proxycore_destroy(proxycore_t *p);

typedef void (*proxycore_instance_cb)(const vmxpacket_t *packet, const void *sender, void *data);
#define PROXYCORE_INSTANCE_HOST (1 << 0)
#define PROXYCORE_INSTANCE_PRIMARY (1 << 1)
#define PROXYCORE_INSTANCE_SECONDARY (1 << 2)
#define PROXYCORE_INSTANCE_MONITOR (1 << 3)
void proxycore_add_instance(proxycore_t *p, proxycore_instance_cb callback, void *data, uint32_t flags);
void proxycore_remove_instance(proxycore_t *p, proxycore_instance_cb callback, void *data);
void proxycore_process_packet(proxycore_t *p, vmxpacket_t *packet, const void *sender, uint32_t sender_flags);

int proxycore_get_host_id(proxycore_t *p);

#ifdef __cplusplus
}
#endif
