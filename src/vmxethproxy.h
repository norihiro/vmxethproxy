#pragma once

#include "config-macros.h"

typedef struct proxycore_s proxycore_t;
typedef struct vmxhost_s vmxhost_t;
typedef struct vmxcache_s vmxcache_t;
typedef struct socket_moderator_s socket_moderator_t;
typedef struct vmxpacket_s vmxpacket_t;
typedef struct vmxinstance_type_s vmxinstance_type_t;

extern "C" volatile int vmx_interrupted;
