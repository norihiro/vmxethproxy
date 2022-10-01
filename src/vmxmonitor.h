#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct vmxmonitor_s vmxmonitor_t;

vmxmonitor_t *vmxmonitor_create(proxycore_t *);
void vmxmonitor_destroy(vmxmonitor_t *);

#ifdef __cplusplus
}
#endif
