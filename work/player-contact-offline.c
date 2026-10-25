/* Actual native contact adapter against explicit synthetic engine objects. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_stream.h"
#include "coop_race.h"
typedef void CPU;
typedef struct {uint32_t car,peer_id;float com[3],drawn[3];unsigned pose_frames;int ghost,paused;} Slot;
#define NSLOT 1
#define SL_ACTIVE 2
#define SL_LOADING 1
static struct {uint32_t car,world;int st;} g_sl[NSLOT];
static uint32_t world(void){return 123;}
#define MR32(a) (*(uint32_t *)(uintptr_t)(a))
#define MR8(a) (*(uint8_t *)(uintptr_t)(a))
#define MRF32(a) (*(float *)(uintptr_t)(a))
#define JUMP_M 50.f
#define CLEAR_M 6.f
#define FN_CAR_SETPHYS 1
#define FN_CAR_BODYPOS 2
static uint32_t car[0x9e0/4],me[0x9e0/4],body[0x200/4],mebody[0x200/4],physics[0x200/4],ride[0x200/4],ref[16];
static rc_session g_s;
static struct {int bound;uint32_t peer;cc_race control;cs_stream stream;} NC;
static int64_t now=10000000;
static int solid,mode_calls,bodpos_calls,checks,remap;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t addr(void *p){return (uint32_t)(uintptr_t)p;}
static int nc_remote_car(uint32_t p){return p==addr(car);}
static uint32_t rd32(uint32_t p){return MR32(p);}
static int ok(uint32_t a,unsigned n){
 void *blocks[]={car,me,body,physics,ride,ref,mebody};unsigned sizes[]={sizeof car,sizeof me,sizeof body,sizeof physics,sizeof ride,sizeof ref,sizeof mebody};
 for(unsigned i=0;i<7;i++)if(a>=addr(blocks[i])&&a-addr(blocks[i])<=sizes[i]&&n<=sizes[i]-(a-addr(blocks[i])))return 1;
 return 0;
}
static int env_int(const char *s,int d,int lo,int hi){(void)s;(void)d;(void)lo;(void)hi;return 1;}
static void roam_plat_lock(void){}
static void roam_plat_unlock(void){}
static int64_t roam_plat_now_us(void){return now;}
static uint32_t player_car(void){return addr(me);}
static void body_set(CPU *c,uint32_t p,int enabled){(void)c;CHECK(p==addr(car));solid=enabled;}
static void rd3(uint32_t p,float out[3]){memcpy(out,(void*)(uintptr_t)p,12);}
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t p,uint32_t mode){
 (void)c;CHECK(fn==FN_CAR_SETPHYS&&p==addr(car)&&mode==3&&!solid);mode_calls++;
 car[0x28/4]=remap?4:3;car[0x34/4]=addr(physics);body[0x5c/4]=!remap;body[0x64/4]=addr(ref);body[0x68/4]=addr(ride);return 0;
}
static uint32_t gcall0(CPU *c,uint32_t fn,uint32_t p){
 (void)c;CHECK(fn==FN_CAR_BODYPOS&&p==addr(car));bodpos_calls++;
 MRF32(p+0x50)=MRF32(p+0x60)-.5f;MRF32(p+0x54)=MRF32(p+0x64);MRF32(p+0x58)=MRF32(p+0x68);return 0;
}
static int authoritative_ready=1;
static int npi_contact_ready(uint32_t peer,int64_t at){(void)peer;(void)at;return authoritative_ready;}
#include "native_player_contact.inc"
int main(void){
 car[0x3c/4]=addr(body);body[0x130/4]=addr(car);
 MRF32(addr(car)+0x90)=1;MRF32(addr(car)+0xa4)=1;MRF32(addr(car)+0xb8)=1;
 MRF32(addr(me)+0x60)=100;
 Slot slot={0};slot.car=addr(car);slot.peer_id=200;slot.ghost=1;
 rc_peer *p=&g_s.peer[0];p->id=200;p->state=RC_PS_EST;p->player_contact=1;p->status=RC_ST_ROAM;p->last_snap_rx=now;
 rc_pose pose={0};pose.q[3]=1;
 CHECK(np_prepare_pose(0,&slot,&pose));CHECK(mode_calls==1&&bodpos_calls==1&&slot.com[0]==.5f);
 CHECK(np_native_ready(addr(car)));np_update_body(0,&slot);CHECK(solid&&!slot.ghost);
 authoritative_ready=0;np_update_body(0,&slot);CHECK(!solid);authoritative_ready=1;np_update_body(0,&slot);CHECK(solid);
    np_before_zero(addr(car));CHECK(!solid);np_update_body(0,&slot);CHECK(solid);
    np_before_zero(addr(me));CHECK(solid); /* local human is never disabled */
 CHECK(np_prepare_pose(0,&slot,&pose)&&mode_calls==1); /* no rebuild every frame */
 p->player_contact=0;np_update_body(0,&slot);CHECK(!solid&&slot.ghost);
 p->player_contact=1;slot.paused=1;np_update_body(0,&slot);CHECK(!solid);slot.paused=0;
 p->status=RC_ST_PAUSED;np_update_body(0,&slot);CHECK(!solid);
 p->status=RC_ST_ROAM;p->last_snap_rx=now-500001;np_update_body(0,&slot);CHECK(!solid);
 p->last_snap_rx=now+1;CHECK(!np_source_live(200,now));p->last_snap_rx=now;
 MRF32(addr(me)+0x60)=0;np_update_body(0,&slot);CHECK(!solid&&slot.ghost);
 MRF32(addr(me)+0x60)=100;np_update_body(0,&slot);CHECK(solid&&!slot.ghost);
 /* A large native displacement from impact is not a network teleport. */
 slot.pose_frames=1;slot.ghost=0;MRF32(addr(car)+0x60)=100;
 CHECK(np_prepare_pose(0,&slot,&pose)&&!slot.ghost);
 pose.p[0]=100;CHECK(np_prepare_pose(0,&slot,&pose)&&slot.ghost);
 p->lost=1;np_update_body(0,&slot);CHECK(!solid);p->lost=0;
 body[0x68/4]=0;CHECK(!np_native_ready(addr(car)));CHECK(!np_prepare_pose(0,&slot,&pose)&&!solid);
 body[0x68/4]=addr(ride);body[0x130/4]=addr(me);CHECK(!np_native_ready(addr(car)));body[0x130/4]=addr(car);
 p->status=RC_ST_EVENT;NC.peer=200;NC.bound=1;NC.control.phase=CC_RUNNING;NC.stream.seq_init=1;NC.stream.last_rx=now;
 CHECK(np_source_live(200,now));NC.stream.last_rx=now-500001;CHECK(!np_source_live(200,now));
 NC.stream.last_rx=now;NC.control.phase=CC_DONE;CHECK(!np_source_live(200,now));
 /* Reproduce native SetPhysics(3)->mode4 before a streamed cell exists.
  * Nonzero mode4 must not prevent a later full-physics promotion. */
 MR32(addr(car)+0x28)=0;MR32(addr(body)+0x130)=addr(car);MR8(addr(car)+0x5c0)=0;remap=1;
 CHECK(np_prepare_pose(0,&slot,&pose)&&MR32(addr(car)+0x28)==4&&!MR32(addr(body)+0x5c));
 unsigned before=mode_calls;CHECK(np_prepare_pose(0,&slot,&pose)&&mode_calls==before);
 MR8(addr(car)+0x5c0)=1;remap=0;
 CHECK(np_prepare_pose(0,&slot,&pose)&&mode_calls==before+1&&MR32(addr(car)+0x28)==3&&MR32(addr(body)+0x5c));
 CHECK(MR8(addr(car)+0x5c0)==1); /* native streaming flag was never forced */
 /* Native local bounds, asymmetric longitudinal centre and safe grid gaps.
  * Two cars within the old six-metre sphere are not necessarily overlapping. */
 uint32_t cars[2]={addr(car),addr(me)},bodies[2]={addr(body),addr(mebody)};
 for(int n=0;n<2;n++){
  MR32(cars[n]+0x3c)=bodies[n];MR32(bodies[n])=0x7a2478;MR32(bodies[n]+0x130)=cars[n];
  for(int j=0;j<3;j++){
   MRF32(cars[n]+0x60+4*j)=0;
   for(int k=0;k<3;k++)MRF32(cars[n]+0x90+16*j+4*k)=j==k?1.f:0.f;
  }
  float lo[3]={-2.5f,-1.05f,0},hi[3]={2.1f,1.05f,1.5f};
  for(int j=0;j<3;j++){MRF32(bodies[n]+0xe0+4*j)=lo[j];MRF32(bodies[n]+0xf0+4*j)=hi[j];}
 }
 CHECK(!np_spawn_clear(addr(car),addr(me))); /* coincident spawn stays ghost */
 MRF32(addr(me)+0x64)=2.7f;CHECK(np_spawn_clear(addr(car),addr(me))); /* two-wide grid */
 slot.ghost=1;p->status=RC_ST_ROAM;p->last_snap_rx=now;np_update_body(0,&slot);CHECK(solid&&!slot.ghost);
 slot.ghost=1;slot.paused=1;np_update_body(0,&slot);CHECK(!solid&&slot.ghost);slot.paused=0;
 MRF32(addr(me)+0x64)=2.3f;CHECK(!np_spawn_clear(addr(car),addr(me)));
 MRF32(addr(me)+0x64)=0;MRF32(addr(me)+0x60)=4.7f;CHECK(!np_spawn_clear(addr(car),addr(me)));
 MRF32(addr(me)+0x60)=5.1f;CHECK(np_spawn_clear(addr(car),addr(me)));
 /* Crossing cars overlap despite different headings. */
 MRF32(addr(me)+0x60)=0;MRF32(addr(me)+0x90)=0;MRF32(addr(me)+0x94)=1;
 MRF32(addr(me)+0xa0)=-1;MRF32(addr(me)+0xa4)=0;CHECK(!np_spawn_clear(addr(car),addr(me)));
 MRF32(addr(me)+0x60)=4.5f;CHECK(np_spawn_clear(addr(car),addr(me)));
 MR32(addr(mebody)+0xf0)=0x7fc00000;CHECK(!np_spawn_clear(addr(car),addr(me))); /* unknown bounds fail closed */
 MRF32(addr(mebody)+0xf0)=2.1f;MRF32(addr(me)+0x90)=10;CHECK(!np_spawn_clear(addr(car),addr(me)));
 printf("PASS player-contact adapter: %d checks; physics retention, native links, mutual capability, freshness, paused/teleport/spawn ghosts and race scope (mock engine only)\n",checks);
 return 0;
}
