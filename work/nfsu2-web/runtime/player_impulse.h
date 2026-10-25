#ifndef PLAYER_IMPULSE_H
#define PLAYER_IMPULSE_H
#include <stdint.h>
#define RC_EXT_PLAYER_IMPULSE 2u
#define RC_EXT_CONTACT_HOST 0x80000000u
#define RC_PLAYER_IMPULSE_INBOX 16
typedef struct {
 uint32_t to,source,destination,serial,world,peer_world,run,event,physics,contact_class;
 uint8_t active;
} rc_contact_scene;
/* Exact changes of physical linear/angular momentum, measured around the
 * native guest-proxy impulse calls on the host. Not a force to integrate. */
typedef struct {
 uint32_t to,source,destination,host_world,guest_world,run,event,physics,t_us;
 uint16_t seq;
 uint8_t op;
 float target_p[3],host_p[3],dp[3],dl[3];
} rc_player_impulse;
#endif
