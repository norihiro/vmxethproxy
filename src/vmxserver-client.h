#pragma once

#include "vmxprop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxserver_client_s vmxserver_client_t;

vmxserver_client_t *vmxserver_client_create(int sock, uint32_t proxy_flags, socket_moderator_t *ss, proxycore_t *p,
					    vmx_prop_ref_t prop);
void vmxserver_client_destroy(vmxserver_client_t *c);
bool vmxserver_client_has_error(vmxserver_client_t *c);

#ifdef __cplusplus
}
#endif
