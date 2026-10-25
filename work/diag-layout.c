#include <stdio.h>
#include <stddef.h>
#include "roam_core.h"
int main(void){
 printf("{\"pause\":%u,\"peer\":%u,\"peer_size\":%u,\"conf_pause\":%u,\"peer_pause\":%u,\"peer_clock\":%u,\"peer_clock_at\":%u,\"peer_offset\":%u}",
 (unsigned)offsetof(rc_session,pause),(unsigned)offsetof(rc_session,peer),(unsigned)sizeof(rc_peer),
 (unsigned)offsetof(rc_config,race_pause),(unsigned)offsetof(rc_peer,race_pause),
 (unsigned)offsetof(rc_peer,clock_valid),(unsigned)offsetof(rc_peer,clock_sample_at),(unsigned)offsetof(rc_peer,clock_offset_us));
}
