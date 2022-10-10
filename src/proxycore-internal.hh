#pragma once

#include <list>

struct instance_info_s
{
	proxycore_instance_cb callback;
	void *data;
	uint32_t flags;
};

struct proxycore_s
{
	std::list<instance_info_s> instances;
	int host_id = -1;
	uint8_t host_id_revision[4] = {0, 0, 0, 0};
};
