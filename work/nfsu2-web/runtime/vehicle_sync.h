#ifndef VEHICLE_SYNC_H
#define VEHICLE_SYNC_H
#include "roam_core.h"
/* Exact serialized equipment is frozen for one race. A peer acknowledges a
 * digest only after its engine has attached and checked that vehicle. */
typedef struct {
 uint32_t self,peer,run,event,own,remote;
 int initialized,remote_seen,own_ack,loaded,fault,turn;
 uint32_t own_physics,remote_physics;
 int proof_required,physics_seen;
 int64_t next_tx;
} vs_barrier;
void vs_init(vs_barrier *,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,uint32_t digest);
int vs_expect_physics(vs_barrier *,uint32_t digest);
int vs_receive(vs_barrier *,uint32_t from,const rc_coop_msg *);
int vs_loaded(vs_barrier *,uint32_t digest);
int vs_loaded_physics(vs_barrier *,uint32_t equipment,uint32_t physics);
int vs_ready(const vs_barrier *);
int vs_next(vs_barrier *,int64_t now,rc_coop_msg *);
#endif
