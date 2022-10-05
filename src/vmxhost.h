#pragma once

#include "vmxprop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxhost_s vmxhost_t;

vmxhost_t *vmxhost_create(vmx_prop_ref_t);
void vmxhost_set_prop(vmxhost_t *, vmx_prop_ref_t);
void vmxhost_start(vmxhost_t *, socket_moderator_t *, proxycore_t *);
void vmxhost_destroy(vmxhost_t *);

#ifdef __cplusplus
}
#endif
