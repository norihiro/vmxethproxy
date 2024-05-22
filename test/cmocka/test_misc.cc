#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
#include <unistd.h>
#include <sys/socket.h>

#include <vector>
#include "vmxpacket.h"
#include "misc.h"

static void test_failures(void **)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	close(fd);

	// `recv` will fail.
	std::vector<unsigned char> buf;
	assert_int_equal(recv_stream(fd, buf), false);

	// `send` will fail.
	vmxpacket_t pkt;
	pkt.raw.push_back(0);
	assert_int_equal(send_stream(fd, &pkt), false);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_failures),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
