#pragma once

#include "vmxethproxy.h"
#include "socket-moderator.h"
#include "vmxprop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxws_s vmxws_t;

vmxws_t *vmxws_create(vmx_prop_ref_t);
void vmxws_set_prop(vmxws_t *s, vmx_prop_ref_t prop);
void vmxws_start(vmxws_t *, socket_moderator_t *, proxycore_t *);
void vmxws_destroy(vmxws_t *);

#ifdef __cplusplus
}
#endif
