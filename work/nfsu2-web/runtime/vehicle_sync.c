#include "vehicle_sync.h"
#include <string.h>
void vs_init(vs_barrier *s,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,uint32_t digest){
 memset(s,0,sizeof *s);s->self=self;s->peer=peer;s->run=run;s->event=event;s->own=digest;
 s->initialized=self&&peer&&self!=peer&&run&&event;
}
int vs_receive(vs_barrier *s,uint32_t from,const rc_coop_msg *m){
 if(!s->initialized||s->fault||from!=s->peer||m->to!=s->self||m->run!=s->run||m->event!=s->event)return 0;
 if(m->op==RC_COOP_VEHICLE){
  if(s->remote_seen&&s->remote!=m->value){s->fault=1;return -1;}
  s->remote_seen=1;s->remote=m->value;return 1;
 }
 if(m->op==RC_COOP_VEHICLE_PHYSICS&&s->proof_required){
  if(s->physics_seen&&s->remote_physics!=m->value){s->fault=1;return -1;}
  s->physics_seen=1;s->remote_physics=m->value;return 1;
 }
 if(m->op==RC_COOP_VEHICLE_ACK&&m->value==s->own){s->own_ack=1;return 1;}
 return 0;
}
int vs_loaded(vs_barrier *s,uint32_t digest){
 if(!s->initialized||s->fault||s->proof_required||!s->remote_seen||digest!=s->remote)return 0;
 s->loaded=1;return 1;
}
int vs_expect_physics(vs_barrier *s,uint32_t digest){
 if(!s->initialized||s->fault||s->proof_required||s->loaded||s->own_ack)return 0;
 s->proof_required=1;s->own_physics=digest;return 1;
}
int vs_loaded_physics(vs_barrier *s,uint32_t equipment,uint32_t physics){
 if(!s->initialized||s->fault||!s->proof_required||!s->remote_seen||!s->physics_seen||equipment!=s->remote)return 0;
 if(physics!=s->remote_physics){s->fault=1;return 0;}
 s->loaded=1;return 1;
}
int vs_ready(const vs_barrier *s){return s->initialized&&!s->fault&&s->loaded&&s->own_ack&&(!s->proof_required||s->physics_seen);}
int vs_next(vs_barrier *s,int64_t now,rc_coop_msg *m){
 if(!s->initialized||s->fault||now<s->next_tx)return 0;
 memset(m,0,sizeof *m);m->to=s->peer;m->run=s->run;m->event=s->event;
 if(s->proof_required){
  if(s->turn==1){m->op=RC_COOP_VEHICLE_PHYSICS;m->value=s->own_physics;s->turn=s->loaded?2:0;}
  else if(s->turn==2&&s->loaded){m->op=RC_COOP_VEHICLE_ACK;m->value=s->remote;s->turn=0;}
  else {m->op=RC_COOP_VEHICLE;m->value=s->own;s->turn=1;}
 }else if(s->loaded&&s->turn){m->op=RC_COOP_VEHICLE_ACK;m->value=s->remote;s->turn=0;}
 else {m->op=RC_COOP_VEHICLE;m->value=s->own;s->turn=1;}
 s->next_tx=now+(s->loaded?125000:250000);return 1;
}
