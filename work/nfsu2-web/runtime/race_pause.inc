/* Included by roam_core.c to preserve its single-source platform builds. */
#include "race_pause.h"
#define CP_FRESH 750000
/* Match the 750ms frame watchdog. A brief streamed-cell/render hitch must
 * not trigger a resume handshake. Contact leases remain shorter
 * and independently withdraw collision bodies while motion is stale. */
#define CP_FRAME 750000
#define CP_RETRY 100000
/* At the supported <=200ms RTT, retain a 500ms ARM window and another
 * 500ms for GO/ACK. Do not add two full seconds to each streaming hitch. */
#define CP_COUNTDOWN 1000000
void cp_clear(cp_pause *p){memset(p,0,sizeof *p);}
int cp_held(const cp_pause *p){return p->active&&p->phase!=CP_RUNNING;}
static int own_ready(const cp_pause *p,int64_t now){return !p->blocked&&now>=p->frame_at&&now-p->frame_at<CP_FRAME;}
static int eligible(const cp_pause *p,int64_t now){
 return own_ready(p,now)&&p->remote_seen&&!(p->remote_flags&1)&&now>=p->peer_at&&now-p->peer_at<CP_FRESH;
}
static void held(cp_pause *p){
 if(p->host){if(++p->generation==0)++p->generation;}
 p->phase=CP_HELD;p->arm_acked=p->go_acked=0;p->proposal=0;p->release_at=0;
 p->need_sync=!p->host;p->next_tx=0;
}
void cp_frame(cp_pause *p,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int blocked,int64_t now){
 if(!self||!peer||self==peer||!run||!event){cp_clear(p);return;}
 if(!p->active||p->self!=self||p->peer!=peer||p->run!=run||p->event!=event||p->host!=!!host){
  cp_clear(p);p->active=1;p->self=self;p->peer=peer;p->run=run;p->event=event;p->host=!!host;
  p->phase=CP_HELD;p->generation=p->host?1:0;p->need_sync=!p->host;p->local_flags=-1;
 }
 p->frame_at=now;p->blocked=!!blocked;
}
void cp_step(cp_pause *p,int64_t now,int connected){
 if(!p->active)return;
 int ready=connected&&eligible(p,now);
 if(!ready){if(p->phase!=CP_HELD){
  p->hold_reason=(!connected?1u:0u)|(p->blocked?2u:0u)|
   (now<p->frame_at?4u:0u)|(now-p->frame_at>=CP_FRAME?8u:0u)|
   (!p->remote_seen?16u:0u)|((p->remote_flags&1)?32u:0u)|
   (now<p->peer_at?64u:0u)|(now-p->peer_at>=CP_FRESH?128u:0u);
  p->hold_at=now;p->hold_frame_age=now-p->frame_at;p->hold_peer_age=now-p->peer_at;
  held(p);
 }}
 else if(p->host){
  if((p->phase==CP_RUNNING||p->phase==CP_COMMITTED)&&(p->remote_flags&2))held(p);
  if(p->phase==CP_HELD){
   /* 1024us units tile the 32-bit microsecond clock exactly. Millisecond
    * units do not: two PCs can differ by multiple 2^32us clock epochs,
    * which an NTP offset on the wire deliberately cannot represent. */
   p->release_at=(now+CP_COUNTDOWN+1023)&~(int64_t)1023;
   p->proposal=(uint32_t)p->generation<<16|(((uint32_t)p->release_at>>10)&65535u);
   p->phase=CP_ARMED;p->next_tx=0;
  }else if(p->phase==CP_ARMED){
   if(now>=p->release_at-500000)held(p);
   else if(p->arm_acked&&!(p->remote_flags&2)){p->phase=CP_COMMITTED;p->next_tx=0;}
  }else if(p->phase==CP_COMMITTED&&now>=p->release_at){
   if(p->go_acked&&now>=p->ack_at&&now-p->ack_at<300000)p->phase=CP_RUNNING;
   else held(p);
  }
 }else if(p->phase>=CP_ARMED){
  if(now<p->host_at||now-p->host_at>=CP_FRESH)held(p);
  else if(p->phase==CP_COMMITTED&&now>=p->release_at){p->phase=CP_RUNNING;p->need_sync=0;}
 }
 int flags=(!own_ready(p,now)?1:0)|(!p->host&&p->need_sync?2:0);
 if(flags!=p->local_flags){p->local_flags=flags;p->local_seq=(p->local_seq+1)&0x3fffffffu;p->next_tx=0;}
}
void cp_receive(cp_pause *p,uint8_t op,uint32_t value,int64_t now,int32_t offset){
 if(!p->active)return;
 if(op==CP_STATE){
  uint32_t seq=value>>2,delta=(seq-p->remote_seq)&0x3fffffffu;
  if(p->remote_seen&&delta>=0x20000000u)return;
  /* Same sequence may renew the lease, but may not change its flags. */
  if(p->remote_seen&&!delta&&(int)(value&3)!=p->remote_flags)return;
  p->remote_seq=seq;p->remote_flags=value&3;p->remote_seen=1;p->peer_at=now;return;
 }
 if(p->host){
  if(value!=p->proposal)return;
  if(op==CP_ARM_ACK&&p->phase==CP_ARMED)p->arm_acked=1;
  if(op==CP_GO_ACK&&p->phase>=CP_COMMITTED){p->go_acked=1;p->ack_at=now;}
  return;
 }
 uint16_t gen=(uint16_t)(value>>16);
 if(!gen)return;
 int16_t delta=(int16_t)(uint16_t)(gen-p->generation);
 if(p->generation&&delta<0)return;
 if(op!=CP_HOLD&&op!=CP_ARM&&op!=CP_GO)return;
 if(!p->generation||delta>0){held(p);p->generation=gen;}
 if(op==CP_HOLD){
  if((value&65535u)||p->phase!=CP_HELD)return;
  p->host_at=now;return;
 }
 if(!eligible(p,now))return;
 if(op==CP_ARM){
  if(p->phase>=CP_COMMITTED)return;
  if(p->phase==CP_ARMED){if(value==p->proposal){p->host_at=now;p->next_tx=0;}return;}
  uint32_t host_us=(uint32_t)now-(uint32_t)offset;
  int16_t units=(int16_t)(uint16_t)((value&65535u)-(uint16_t)(host_us>>10));
  int64_t remaining=(int64_t)units*1024-(host_us&1023u);
  if(remaining<250000||remaining>5000000)return;
  p->proposal=value;p->release_at=now+remaining;p->phase=CP_ARMED;
  p->need_sync=0;p->host_at=now;p->next_tx=0;return;
 }
 if(value!=p->proposal||p->phase<CP_ARMED)return;
 p->phase=p->phase==CP_RUNNING?CP_RUNNING:CP_COMMITTED;p->host_at=now;p->next_tx=0;
}
int cp_packets(cp_pause *p,int64_t now,cp_packet out[2]){
 if(!p->active||now<p->next_tx)return 0;
 p->next_tx=now+CP_RETRY;
 out[0].op=CP_STATE;out[0].value=p->local_seq<<2|(uint32_t)p->local_flags;
 if(p->host){
  out[1].op=p->phase==CP_HELD?CP_HOLD:p->phase==CP_ARMED?CP_ARM:CP_GO;
  out[1].value=p->phase==CP_HELD?(uint32_t)p->generation<<16:p->proposal;
  return 2;
 }
 if(p->phase>=CP_ARMED){out[1].op=p->phase==CP_ARMED?CP_ARM_ACK:CP_GO_ACK;out[1].value=p->proposal;return 2;}
 return 1;
}
