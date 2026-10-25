/* Opt-in extension negotiation; no HELLO flag bits are reused. */
#ifndef CONTACT_IMPULSE_H
#define CONTACT_IMPULSE_H
#include <stdint.h>
#define RC_EXT_TRAFFIC_IMPULSE 1u
#define RC_IMPULSE_INBOX 16
enum {RC_IMPULSE_DATA=1,RC_IMPULSE_ACK=2};
typedef struct {
 uint32_t to,nonce,echo,features,serial;
} rc_extensions;
/* Native solver arguments, not a pose or velocity correction. Arm is the
 * world-space offset from body COM to contact, as passed to Body+50. */
typedef struct {
 uint32_t to,source,destination,world,context,id,t_us;
 uint16_t seq;
 uint8_t op;
 float p[3],player[3],impulse[3],arm[3];
} rc_impulse;
#endif
