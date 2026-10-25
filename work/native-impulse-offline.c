/* Actual adapter against mock native memory; never executes native EXE code. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "coop_stream.h"
#include "native_contact_impulse.h"
#include "native_world.h"
typedef struct {int unused;} CPU;
typedef struct {uint32_t car,world,peer_id;int st;} Slot;
#define NSLOT 2
#define SL_ACTIVE 1
#define G_FLOW 0x8654a4
#define FN_CAR_SETPHYS 1
static void rlog(const char *format,...){(void)format;}
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define MRF32(a) (*(float*)(uintptr_t)(a))
#define MR8(a) (*(uint8_t*)(uintptr_t)(a))
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t cars[3][0x9e0],bodies[3][0x1a0],physics[3][0x30],info[3][0x200],pairs[2][0x20];
static uint32_t live_world=0x12340000;static int64_t time_now=1000000;
static int lock_depth,original_calls,apply_calls,mode_calls,body_calls,fail_mode,refresh_calls;
static float applied_j[3],applied_r[3];
static Slot g_sl[NSLOT];static rc_session g_s;
static struct {int bound,host;uint32_t peer,car[6];cc_race control;cs_stream stream;} NC;
static struct {
 int enabled,host,owned;uint32_t world,context,generation,peer;
 struct {uint32_t world;int64_t last_rx;} client;
 struct {int active;uint32_t car,id,type;} host_car[16];
 struct {uint32_t car,id;} replica[16];
} NW;
static uint32_t addr(void *p){return (uint32_t)(uintptr_t)p;}
static int ok(uint32_t a,uint32_t n){
 if(!a||a+n<a)return 0;
 const void *starts[]={cars,bodies,physics,info,pairs};const unsigned lengths[]={sizeof cars,sizeof bodies,sizeof physics,sizeof info,sizeof pairs};
 for(int k=0;k<5;k++)if(a>=addr((void*)starts[k])&&a+n<=addr((void*)starts[k])+lengths[k])return 1;
 return 0;
}
static uint32_t rd32(uint32_t a){if(a==G_FLOW)return 6;return ok(a,4)?MR32(a):0;}
static void rd3(uint32_t a,float *v){CHECK(ok(a,12));memcpy(v,(void*)(uintptr_t)a,12);}
static uint32_t world(void){return live_world;}
static uint32_t player_car(void){return addr(cars[0]);}
static int nc_same_world(void){return NC.bound;}
static int64_t roam_plat_now_us(void){return time_now;}
static void roam_plat_lock(void){CHECK(!lock_depth);lock_depth=1;}
static void roam_plat_unlock(void){CHECK(lock_depth==1);lock_depth=0;}
static float nw_len2(const float *v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
static float nw_dist2(const float *a,const float *b){float v[3];for(int k=0;k<3;k++)v[k]=a[k]-b[k];return nw_len2(v);}
static uint32_t nw_type(uint32_t car){CHECK(car==addr(cars[1]));return 200;}
static int nc_read_car(uint32_t car,rc_state *s,int64_t now){
 (void)now;memset(s,0,sizeof *s);rd3(car+0x60,s->p);rd3(car+0x70,s->v);return 1;
}
static void body_set(CPU *c,uint32_t car,int on){
 (void)c;CHECK(!lock_depth&&car==addr(cars[1]));MR8(addr(bodies[1])+0x17)=!on;body_calls++;
}
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t car,uint32_t mode){
 (void)c;CHECK(!lock_depth);CHECK(MR8(addr(bodies[1])+0x17));mode_calls++;
 if(fn==0x5938b0){CHECK(car==addr(bodies[1])&&mode==addr(bodies[2]));car=addr(cars[1]);mode=3;}
 else CHECK(fn==FN_CAR_SETPHYS&&car==addr(cars[1]));
 MR32(car+0x28)=fail_mode&&mode==3?4:mode;
 MR32(addr(bodies[1])+0x5c)=mode==3&&!fail_mode;
 return 0;
}
void ni_apply(uint32_t body,const float *j,const float *r){
 CHECK(!lock_depth&&body==addr(bodies[1]));apply_calls++;memcpy(applied_j,j,12);memcpy(applied_r,r,12);
}
static uint32_t gcall0(CPU *c,uint32_t fn,uint32_t body){
 (void)c;CHECK(!lock_depth&&fn==0x593670&&body==addr(bodies[1]));CHECK(apply_calls>refresh_calls);refresh_calls++;return 0;
}
static rc_msg sent[128];static int nsent;
static void send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)u;CHECK(ip==0x7f000001&&port>=47210&&port<=47211);CHECK(nsent<128);CHECK(rc_decode(b,n,&sent[nsent++])==RC_OK);
}
#include "native_contact_impulse.inc"
#ifndef PLAYER_IMPULSE_ADAPTER
void ni_solve(void *pair,ni_solver_fn original){ni_traffic_solve(pair,original);}
#endif
static void setup(int host){
 memset(&g_s,0,sizeof g_s);memset(&NIP,0,sizeof NIP);memset(&NC,0,sizeof NC);memset(&NW,0,sizeof NW);memset(g_sl,0,sizeof g_sl);
 memset(cars,0,sizeof cars);memset(bodies,0,sizeof bodies);memset(pairs,0,sizeof pairs);
 time_now=1000000;nsent=lock_depth=apply_calls=original_calls=mode_calls=body_calls=fail_mode=refresh_calls=0;live_world=0x12340000;
 for(int k=0;k<3;k++){
  uint32_t car=addr(cars[k]),body=addr(bodies[k]);
  MR32(car+0x3c)=body;MR32(car+0x34)=addr(physics[k]);MR32(car+0x28)=3;
  MR32(body)=0x7a2478;MR32(body+0x130)=car;MR32(body+0x5c)=1;
  MR32(body+0x64)=addr(physics[k])+8;MR32(body+0x68)=addr(info[k]);
 }
 MR32(addr(cars[1])+0x24)=2;MR8(addr(cars[1])+0x550)=1;
 MR32(addr(pairs[0])+0xc)=addr(bodies[0]);MR32(addr(pairs[0])+0x10)=addr(bodies[1]);
 MR32(addr(pairs[1])+0xc)=addr(bodies[0]);MR32(addr(pairs[1])+0x10)=addr(bodies[2]);
 NW.enabled=1;NW.host=host;NW.owned=!host;NW.world=world();NW.generation=77;NW.peer=host?200:100;
 NW.client.world=77;NW.client.last_rx=time_now;
 NW.replica[0].car=addr(cars[1]);NW.replica[0].id=5;
 NW.host_car[0].active=1;NW.host_car[0].car=addr(cars[1]);NW.host_car[0].id=5;NW.host_car[0].type=200;
 g_s.conf.self_id=host?100:200;g_s.conf.port=host?47210:47211;
 g_s.conf.test_mode=1;g_s.conf.max_players=2;g_s.conf.rate_hz=20;
 g_s.conf.traffic=g_s.conf.traffic_contact=1;g_s.conf.world_host=host;
 g_s.conf.extensions=1;g_s.conf.extension_nonce=host?111:222;
 g_s.io.send=send_packet;
 rc_peer *p=&g_s.peer[0];p->id=NW.peer;p->state=RC_PS_EST;p->core_ver=4;p->traffic=p->traffic_contact=1;p->world_host=!host;
 p->status=RC_ST_ROAM;p->ip=0x7f000001;p->port=host?47211:47210;
 p->clock_valid=1;p->clock_sample_at=time_now;p->clock_rtt_us=1000;
 p->extension_seen=1;p->extension_at=time_now;
 p->extensions=(rc_extensions){g_s.conf.self_id,host?222:111,g_s.conf.extension_nonce,1,1};
 p->mo.n=1;p->last_snap_rx=time_now;
 g_sl[0]=(Slot){addr(cars[2]),world(),NW.peer,SL_ACTIVE};
}
static const float impulse[3]={100,0,-20},arm[3]={1,2,0};
static void __attribute__((thiscall)) ordinary(void *pair){
 (void)pair;CHECK(!lock_depth);original_calls++;
 ni_observe(addr(bodies[0]),impulse,arm); /* other side never captured */
 ni_observe(addr(bodies[1]),impulse,arm);
}
static void __attribute__((thiscall)) nested(void *pair){
 (void)pair;ni_solve(pairs[1],ordinary);CHECK(NIP.captured==0);ordinary(pair);
}
static void __attribute__((thiscall)) native_promotes(void *pair){
 MR32(addr(cars[1])+0x28)=3;MR32(addr(bodies[1])+0x5c)=1;ordinary(pair);
}
static void receive(const rc_impulse *hit,uint32_t from){
 rc_msg m={0};m.h.type=RC_MSG_IMPULSE;m.h.sender=from;m.u.impulse=*hit;
 uint8_t bytes[RC_MAX_DGRAM];int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==104);
 rc_receive(&g_s,0x7f000001,g_s.peer[0].port,bytes,n,time_now);
}
static unsigned noise=0xabc123;
static unsigned random32(void){noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;return noise;}
static rc_session sim_session[2];
static __typeof__(NW) sim_world[2];
static __typeof__(NIP) sim_impulse[2];
static struct {rc_impulse hit;int role;int64_t at;} deliveries[128];
static void lossy_sessions(void){
 unsigned confirmed=0,applied=0;
 for(int run=0;run<500;run++){
  CPU cpu={0};int nd=0;
  setup(0);ni_solve(pairs[0],ordinary);
  sim_session[0]=g_s;sim_world[0]=NW;sim_impulse[0]=NIP;
  setup(1);sim_session[1]=g_s;sim_world[1]=NW;sim_impulse[1]=NIP;
  for(time_now=1000000;time_now<=1600000;time_now+=10000)for(int role=0;role<2;role++){
   g_s=sim_session[role];NW=sim_world[role];NIP=sim_impulse[role];nsent=0;
   for(int k=0;k<nd;){
    if(deliveries[k].role!=role||deliveries[k].at>time_now){k++;continue;}
    rc_impulse h=deliveries[k].hit;deliveries[k]=deliveries[--nd];receive(&h,role?200:100);
   }
   ni_frame(&cpu,time_now);
   for(int k=0;k<nsent;k++){
    CHECK(sent[k].h.type==RC_MSG_IMPULSE);
    if(random32()%100<35)continue;
    int copies=random32()%10==0?2:1;
    while(copies--){CHECK(nd<128);deliveries[nd].hit=sent[k].u.impulse;deliveries[nd].role=!role;deliveries[nd++].at=time_now+10000+random32()%90000;}
   }
   sim_session[role]=g_s;sim_world[role]=NW;sim_impulse[role]=NIP;
  }
  CHECK(apply_calls<=1&&refresh_calls==apply_calls&&!sim_impulse[0].pending[0].active);
  if(apply_calls)applied++;
  if(!sim_impulse[0].dropped){CHECK(apply_calls==1);confirmed++;}
 }
 CHECK(confirmed>450&&applied>480);
 printf("lossy impulses:500 sessions,35%% loss/reordering/duplicates; applied %u, acknowledged before deadline %u, never double-applied\n",applied,confirmed);
}
int main(void){
 CPU cpu={0};setup(0);
 ni_observe(addr(bodies[1]),impulse,arm);CHECK(!NIP.captured);
 ni_solve(pairs[0],nested);CHECK(original_calls==2&&NIP.captured==1);
 CHECK(NIP.pending[0].hit.id==5&&NIP.pending[0].hit.impulse[0]==100&&NIP.pending[0].hit.arm[1]==2);
 NIP.pending[0].hit.seq=65535;
 __typeof__(NIP) saved_client=NIP;rc_impulse hit=NIP.pending[0].hit;
 ni_frame(&cpu,time_now);CHECK(nsent==1&&sent[0].u.impulse.op==RC_IMPULSE_DATA);
 time_now+=30000;ni_frame(&cpu,time_now);CHECK(nsent==1);
 time_now+=50000;ni_frame(&cpu,time_now);CHECK(nsent==2); /* retained until ACK */
 setup(1);receive(&hit,200);CHECK(g_s.n_impulse==1);ni_frame(&cpu,time_now);
 CHECK(apply_calls==1&&NIP.applied==1&&nsent==1&&sent[0].u.impulse.op==RC_IMPULSE_ACK);
 CHECK(applied_j[0]==100&&applied_r[1]==2);rc_impulse ack=sent[0].u.impulse;
 receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==1&&NIP.repeated==1&&nsent==2);
 hit.seq++;receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==2); /* 16-bit wrap */
 hit.seq=5;receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==3);
 hit.seq=4;receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==4);
 receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==4);
 hit.world=88;receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==4);
 setup(0);NIP=saved_client;receive(&ack,100);ni_frame(&cpu,time_now);CHECK(!NIP.pending[0].active&&!nsent);
 setup(0);ni_solve(pairs[0],ordinary);time_now+=500001;ni_frame(&cpu,time_now);CHECK(!NIP.pending[0].active&&NIP.dropped==1);
 for(int host=0;host<2;host++){
  setup(host);MR32(addr(pairs[0])+0xc)=addr(bodies[2]);ni_solve(pairs[0],ordinary);CHECK(!original_calls&&!NIP.captured);
  g_s.peer[0].extension_at=-1000000;ni_solve(pairs[0],ordinary);CHECK(original_calls==1&&!NIP.captured);
 }
 setup(0);live_world++;ni_solve(pairs[0],ordinary);CHECK(original_calls==1&&!NIP.captured);
 setup(0);g_s.peer[0].status=RC_ST_PAUSED;ni_solve(pairs[0],ordinary);CHECK(!NIP.captured);
 setup(0);g_s.conf.traffic_contact=0;ni_solve(pairs[0],ordinary);CHECK(!NIP.captured);
 setup(1);hit=saved_client.pending[0].hit;MR32(addr(cars[1])+0x28)=2;MR32(addr(bodies[1])+0x5c)=0;
 receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==1&&mode_calls==1&&body_calls==2);
 setup(1);fail_mode=1;MR32(addr(cars[1])+0x28)=2;MR32(addr(bodies[1])+0x5c)=0;
 receive(&hit,200);ni_frame(&cpu,time_now);CHECK(!apply_calls&&mode_calls==2&&body_calls==2&&MR32(addr(cars[1])+0x28)==2&&!MR8(addr(bodies[1])+0x17));
 setup(0);MR32(addr(bodies[1])+0x5c)=0;ni_solve(pairs[0],native_promotes);CHECK(NIP.captured==1);
 setup(1);receive(&hit,200);g_s.peer[0].extensions.nonce=333;ni_frame(&cpu,time_now);CHECK(!apply_calls); /* queued across nonce change */
 setup(0);ni_solve(pairs[0],ordinary);ni_forget_pending();CHECK(!NIP.pending[0].active&&NIP.dropped==1);
 setup(1);receive(&hit,200);ni_frame(&cpu,time_now);ni_forget_pending();receive(&hit,200);ni_frame(&cpu,time_now);CHECK(apply_calls==1&&NIP.repeated==1);
 lossy_sessions();
 CHECK(!lock_depth);
 printf("PASS actual impulse adapter: %u checks; exact pair scope/nesting, proxy suppression, native apply, retry/ACK, replay/wrap, World/status/lease guards, mode promotion/refusal rollback (mock engine only)\n",checks);return 0;
}
