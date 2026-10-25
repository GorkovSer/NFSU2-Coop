#ifndef REWARD_SYNC_H
#define REWARD_SYNC_H
#include "roam_core.h"
#define RW_MAX_PRIZE 1000000u
#define RW_MAX_BALANCE 0x7fffffffu
/* Only the prize is shared. Balances, purchases and sponsor bonuses never
 * cross the wire. State belongs to one profile and one accepted race run. */
typedef struct {
 uint32_t self,peer,run,event,profile,amount;
 int host,initialized,terms,peer_ack,authorized,won,paid,fault;
 int64_t next_tx;
} rw_reward;
void rw_init(rw_reward *,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,uint32_t profile,int host);
int rw_offer(rw_reward *,uint32_t amount);
int rw_receive(rw_reward *,uint32_t from,const rc_coop_msg *);
int rw_ready(const rw_reward *);
int rw_next(rw_reward *,int64_t now,rc_coop_msg *);
int rw_authorize(rw_reward *,int team_won);
/* 1: one local credit/settlement, 0: already settled/not authorized,
 * -1: wrong profile or overflow. Caller applies returned balance immediately. */
int rw_credit(rw_reward *,uint32_t current_profile,uint32_t local_balance,uint32_t *new_balance);
#endif
