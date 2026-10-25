#include "reward_sync.h"
#include <string.h>
void rw_init(rw_reward *s,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,uint32_t profile,int host){
 memset(s,0,sizeof *s);s->self=self;s->peer=peer;s->run=run;s->event=event;s->profile=profile;s->host=!!host;
 s->initialized=self&&peer&&self!=peer&&run&&event&&profile;
}
int rw_offer(rw_reward *s,uint32_t amount){
 if(!s->initialized||!s->host||s->terms||s->fault||amount>RW_MAX_PRIZE)return 0;
 s->amount=amount;s->terms=1;return 1;
}
int rw_receive(rw_reward *s,uint32_t from,const rc_coop_msg *m){
 if(!s->initialized||s->fault||from!=s->peer||m->to!=s->self||m->run!=s->run||m->event!=s->event)return 0;
 if(!s->host&&m->op==RC_COOP_PRIZE){
  if(m->value>RW_MAX_PRIZE)return 0;
  if(s->terms&&s->amount!=m->value){s->fault=1;return -1;}
  s->amount=m->value;s->terms=1;return 1;
 }
 if(s->host&&m->op==RC_COOP_PRIZE_ACK&&s->terms&&m->value==s->amount){s->peer_ack=1;return 1;}
 return 0;
}
int rw_ready(const rw_reward *s){return s->initialized&&!s->fault&&s->terms&&(!s->host||s->peer_ack);}
int rw_next(rw_reward *s,int64_t now,rc_coop_msg *m){
 if(!s->initialized||s->fault||!s->terms||now<s->next_tx)return 0;
 if(s->host&&s->peer_ack)return 0;
 memset(m,0,sizeof *m);m->op=s->host?RC_COOP_PRIZE:RC_COOP_PRIZE_ACK;
 m->to=s->peer;m->run=s->run;m->event=s->event;m->value=s->amount;s->next_tx=now+250000;return 1;
}
int rw_authorize(rw_reward *s,int won){
 if(!rw_ready(s))return 0;
 won=!!won;
 if(s->authorized){if(s->won!=won){s->fault=1;return -1;}return 1;}
 s->authorized=1;s->won=won;return 1;
}
int rw_credit(rw_reward *s,uint32_t profile,uint32_t balance,uint32_t *after){
 if(!rw_ready(s)||!s->authorized||s->paid||!after)return 0;
 uint32_t amount=s->won?s->amount:0;
 if(profile!=s->profile||balance>RW_MAX_BALANCE||amount>RW_MAX_BALANCE-balance)return -1;
 *after=balance+amount;s->paid=1;return 1;
}
