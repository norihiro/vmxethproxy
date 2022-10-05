#pragma once

#include "vmxprop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxserver_s vmxserver_t;

vmxserver_t *vmxserver_create(vmx_prop_ref_t);
void vmxserver_set_prop(vmxserver_t *s, vmx_prop_ref_t prop);
void vmxserver_start(vmxserver_t *, socket_moderator_t *, proxycore_t *);
void vmxserver_destroy(vmxserver_t *);

#ifdef __cplusplus
}
#endif
