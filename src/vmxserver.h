#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxserver_s vmxserver_t;

vmxserver_t *vmxserver_create();
void vmxserver_set_primary(vmxserver_t *, bool primary);
void vmxserver_set_name(vmxserver_t *, const char *name);
void vmxserver_start(vmxserver_t *, socket_moderator_t *, proxycore_t *);
void vmxserver_destroy(vmxserver_t *);

#ifdef __cplusplus
}
#endif
