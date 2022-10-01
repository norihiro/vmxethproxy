#include <sys/socket.h>
#include "vmxethproxy.h"
#include "vmxpacket.h"
#include "misc.h"

bool recv_stream(int fd, std::vector<unsigned char> &buf)
{
	unsigned char b[2048];
	ssize_t ret = recv(fd, b, sizeof(b), 0);
	if (ret < 0)
		return false;

	buf.insert(buf.end(), b, b + ret);

	return true;
}

bool send_stream(int fd, const vmxpacket_t *p)
{
	ssize_t length = p->raw.size();
	const unsigned char *buf = &p->raw[0];
	while (length > 0) {
		ssize_t ret = send(fd, buf, length, MSG_NOSIGNAL);
		if (ret <= 0)
			return false;
		buf += ret;
		length -= ret;
	}
	return true;
}
