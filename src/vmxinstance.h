#pragma once

#include "vmxethproxy.h"
#include "vmxprop.h"

struct vmxinstance_type_s
{
	const char *id;
	void *(*create)(vmx_prop_ref_t pt);
	void (*start)(void *ctx, socket_moderator_t *s, proxycore_t *p);
	void (*destroy)(void *ctx);
};
