/* Native career co-op coordinator. MIT, see repository LICENSE.
 * No game memory, sockets or threads here. All times are monotonic us.
 * Caller matches established peers through rc_session and supplies clock offset.
 * RELEASE is a countdown barrier action, not a call to the game engine.
 */
#ifndef COOP_RACE_H
#define COOP_RACE_H
#include "roam_core.h"
enum { CC_IDLE,CC_INVITED,CC_WAIT_LOAD,CC_LOADING,CC_COUNTDOWN,CC_RUNNING,CC_DONE,CC_CANCELLED };
enum { CC_INVITATION=1,CC_LOAD_GAME=2,CC_RELEASE_GAME=4,CC_RESULT=8,CC_CANCEL=16,CC_VERIFY_FINISH=32 };
enum { CC_WITHDRAWN=1,CC_TIMEOUT,CC_DISCONNECTED,CC_MISMATCH,CC_BUSY,CC_UNSUPPORTED,CC_ENGINE_ERROR,CC_BAD_CLOCK };
typedef struct {
    uint32_t self,peer,run,event,last_run,go_value,winner;
    int host,phase,seen_run,local_loaded,peer_loaded,go_acked,result_acked;
    uint8_t own_position,peer_position,claimed_position,claim;
    uint32_t actions,cancel_reason;
    int64_t since,release_at,next_tx,terminal_at;
} cc_race;
void cc_init(cc_race *r,uint32_t self,uint32_t peer,int host);
int cc_offer(cc_race *r,uint32_t run,uint32_t event,int64_t now);
int cc_accept(cc_race *r,uint32_t available_event,int64_t now);
int cc_continue_stage(cc_race *,uint32_t next_run,int64_t now);
void cc_loaded(cc_race *r,int64_t now);
void cc_receive(cc_race *r,uint32_t from,const rc_coop_msg *m,int64_t now,int64_t remote_to_local_us);
void cc_step(cc_race *r,int64_t now,int peer_connected);
int cc_next_packet(cc_race *r,int64_t now,rc_coop_msg *out);
uint32_t cc_take_actions(cc_race *r);
void cc_cancel(cc_race *r,uint32_t reason,int64_t now);
void cc_claim_finish(cc_race *r,uint8_t position);
/* Only the host's actual race coordinator may confirm finishes/rewards.
 * A remote FINISH packet alone never produces a team victory. */
int cc_confirm_finish(cc_race *r,uint32_t participant,uint8_t position,int64_t now);
void cc_bot_won(cc_race *r,int64_t now);
#endif
