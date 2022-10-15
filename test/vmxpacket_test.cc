#undef NDEBUG
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <time.h>
#include "vmxethproxy.h"
#include "vmxpacket.h"

static void print_pkt(vmxpacket_t &pkt, const char *name)
{
	printf("%s.raw:", name);
	for (auto it = pkt.midi.begin(); it != pkt.midi.end(); it++)
		printf(" %x", *it);
	printf("\n%s.midi:", name);
	for (auto it = pkt.midi.begin(); it != pkt.midi.end(); it++)
		printf(" %x", *it);
	printf("\n%s.aux: %x\n", name, pkt.aux);
}

int main()
{
	srandom(time(NULL));

	for (int length = 1; length <= 16; length++) {
		vmxpacket_s pkt0;
		auto &midi0 = pkt0.modify_midi();
		midi0.resize(length);
		for (auto it = midi0.begin(); it != midi0.end(); it++)
			*it = random();
		pkt0.aux = random() & 0x0F;

		// pkt0.midi => pkt0.raw
		pkt0.make_raw();
		print_pkt(pkt0, "pkt0");

		// pkt0.raw => pkt1.midi
		vmxpacket_s pkt1;
		int len1 = pkt1.add_from_raw(&pkt0.raw[0], pkt0.raw.size());
		print_pkt(pkt1, "pkt1");
		assert(len1 == pkt0.raw.size());

		assert(pkt0.midi == pkt1.midi);
		assert(pkt0.aux == pkt1.aux);

		// pkt1.midi => pkt2.raw
		vmxpacket_s pkt2;
		pkt2.modify_midi() = pkt1.midi;
		pkt2.aux = pkt1.aux;
		print_pkt(pkt2, "pkt2");
		pkt2.make_raw();

		assert(pkt0.raw == pkt2.raw);
	}

	return 0;
}
