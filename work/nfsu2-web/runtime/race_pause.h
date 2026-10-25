/* Session-owned pause barrier; caller holds session lock. No engine access. */
#ifndef RACE_PAUSE_H
#define RACE_PAUSE_H
#include <stdint.h>
#define RC_EXT_PAUSE_CLOCK 32u
enum { CP_STATE=16,CP_HOLD,CP_ARM,CP_ARM_ACK,CP_GO,CP_GO_ACK };
enum { CP_HELD=1,CP_ARMED,CP_COMMITTED,CP_RUNNING };
typedef struct {uint8_t op;uint32_t value;} cp_packet;
typedef struct {
 uint32_t self,peer,run,event,local_seq,remote_seq,proposal;
 uint16_t generation;
 int active,host,phase,blocked,local_flags,remote_flags,remote_seen,need_sync;
 int arm_acked,go_acked;
 int64_t frame_at,peer_at,host_at,ack_at,release_at,next_tx;
 /* Local diagnostics only; never serialized or used to authorize resume. */
 uint32_t hold_reason;int64_t hold_at,hold_frame_age,hold_peer_age;
} cp_pause;
void cp_clear(cp_pause *p);
void cp_frame(cp_pause *p,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int blocked,int64_t now);
void cp_step(cp_pause *p,int64_t now,int connected);
void cp_receive(cp_pause *p,uint8_t op,uint32_t value,int64_t now,int32_t host_to_local_us);
int cp_packets(cp_pause *p,int64_t now,cp_packet out[2]);
int cp_held(const cp_pause *p);
#endif
