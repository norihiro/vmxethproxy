#pragma once

#ifdef __cplusplus
#include <vector>
bool recv_stream(int fd, std::vector<unsigned char> &buf);
bool send_stream(int fd, const vmxpacket_t *p);
#endif
