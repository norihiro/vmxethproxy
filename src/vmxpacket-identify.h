#pragma once

#ifdef __cplusplus
extern "C" {
#endif

bool vmxpacket_is_midi_idreq(const vmxpacket_t *);
bool vmxpacket_is_midi_idreply(const vmxpacket_t *);
bool vmxpacket_is_midi_dt1(const vmxpacket_t *);
bool vmxpacket_is_midi_rq1(const vmxpacket_t *);

#ifdef __cplusplus
}
#endif
