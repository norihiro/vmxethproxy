#pragma once

#include "vmxprop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxhost_dummy_s vmxhost_dummy_t;

vmxhost_dummy_t *vmxhost_dummy_create(vmx_prop_ref_t);
void vmxhost_dummy_set_prop(vmxhost_dummy_t *, vmx_prop_ref_t);
void vmxhost_dummy_start(vmxhost_dummy_t *, socket_moderator_t *, proxycore_t *);
void vmxhost_dummy_destroy(vmxhost_dummy_t *);

#ifdef __cplusplus
}
#endif
