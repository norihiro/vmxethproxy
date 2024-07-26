#include "vmxhost-midi.cc"

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
extern "C" {
#include <cmocka.h>
}
#include <unistd.h>
#include <sys/socket.h>

volatile int vmx_interrupted = 0;

PmError Pm_OpenInput(PortMidiStream **, PmDeviceID, void *, int32_t, PmTimeProcPtr, void *)
{
	return pmNoError;
}

PmError Pm_OpenOutput(PortMidiStream **, PmDeviceID, void *, int32_t, PmTimeProcPtr, void *, int32_t)
{
	return pmNoError;
}

PmError Pm_Close(PortMidiStream *)
{
	return pmNoError;
}

int Pm_Read(PortMidiStream *, PmEvent *, int32_t)
{
	return 0;
}

PmError Pm_WriteSysEx(PortMidiStream *, PmTimestamp, unsigned char *)
{
	return pmNoError;
}

PmError Pm_Poll(PortMidiStream *)
{
	return pmNoError;
}

const PmDeviceInfo *Pm_GetDeviceInfo(PmDeviceID)
{
	return nullptr;
}

#define assert_vector_equal(v1, v2)                                         \
	do {                                                                \
		assert_int_equal((v1).size(), (v2).size());                 \
		assert_memory_equal((v1).data(), (v2).data(), (v2).size()); \
	} while (0)

typedef std::vector<uint8_t> data_t;

static data_t make_dt1(int addr, int n_data)
{
	std::vector<uint8_t> midi = std::vector<uint8_t>{
		0xF0,
		0x41,
		0x00, // device ID
		0x00,
		0x00,
		0x24,
		0x12, // DT1
		(uint8_t)((addr >> 24) & 0x7F),
		(uint8_t)((addr >> 16) & 0x7F), // address MSP
		(uint8_t)((addr >> 8) & 0x7F),
		(uint8_t)(addr & 0x7F), // address LSB
	};
	for (int i = 0; i < n_data; i++)
		midi.push_back(i);
	add_midi_sum_eox(midi, 7);
	return midi;
}

static void test_consume_midi_bytes(void **)
{
	const data_t dt1_1 = make_dt1(0x01020304, 1);
	const data_t dt1_2 = make_dt1(0x10203040, 2);

	vmxpacket_t pkt;

	assert_int_equal(consume_midi_bytes(pkt, dt1_2), dt1_2.size());
	assert_vector_equal(pkt.midi, dt1_2);

	pkt.midi.clear();

	data_t data({0x10, 0x20});                           // garbage
	data.insert(data.end(), dt1_1.begin(), dt1_1.end()); // expecting this data will be taken.
	data.insert(data.end(), dt1_2.begin(), dt1_2.end());
	assert_int_equal(consume_midi_bytes(pkt, data), 2 + dt1_1.size());
	assert_vector_equal(pkt.midi, dt1_1);

	pkt.midi.clear();
	data.clear();
	data.insert(data.end(), dt1_1.begin(), dt1_1.end() - 1); // simulating buffer overrun, ignored
	data.insert(data.end(), dt1_2.begin(), dt1_2.end());
	assert_int_equal(consume_midi_bytes(pkt, data), dt1_1.size() - 1 + dt1_2.size());
	assert_vector_equal(pkt.midi, dt1_2);

	pkt.midi.clear();
	data.clear();
	data.insert(data.end(), dt1_1.begin(), dt1_1.end() - 1); // simulating buffer overrun, ignored
	data.insert(data.end(), dt1_2.begin(), dt1_2.end() - 1); // should point to the begining of dt1_2
	assert_int_equal(consume_midi_bytes(pkt, data), dt1_1.size() - 1);
	assert_int_equal(pkt.midi.size(), 0);

	data.clear();
	data.push_back(0x01); // garbage will be just removed.
	assert_int_equal(consume_midi_bytes(pkt, data), 1);
	assert_int_equal(pkt.midi.size(), 0);

	data.clear();
	data.insert(data.end(), dt1_1.begin() + 1, dt1_1.end());
	data.insert(data.end(), dt1_2.begin(), dt1_2.end());
	assert_int_equal(consume_midi_bytes(pkt, data), dt1_1.size() - 1 + dt1_2.size());
	assert_vector_equal(pkt.midi, dt1_2);

	pkt.midi.clear();
	data.clear();
	data.insert(data.end(), dt1_1.begin() + 1, dt1_1.end());
	data.insert(data.end(), dt1_2.begin(), dt1_2.end() - 1);
	assert_int_equal(consume_midi_bytes(pkt, data), dt1_1.size() - 1);
	assert_int_equal(pkt.midi.size(), 0);
}

int main()
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_consume_midi_bytes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
