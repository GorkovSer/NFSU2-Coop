/* Actual native preparation and generated host publisher; mock engine only. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roam_core.h"
#include "traffic_sync.h"
#include "native_world.h"
typedef void CPU;
typedef struct {uint32_t car;} Slot;
typedef struct {int started;uint32_t brain;} nv_slot;
static unsigned checks,activations,replacements,resets,sends,physics_calls;static int solid,locked;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t car[2][0x9e0],brains[2][0x820];
static uint32_t address(void *p){return (uint32_t)(uintptr_t)p;}
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define MR8(a) (*(uint8_t*)(uintptr_t)(a))
static int ok(uint32_t p,unsigned n){return (p>=address(car)&&p+n>=p&&p+n<=address(car)+sizeof car)||(p>=address(brains)&&p+n>=p&&p+n<=address(brains)+sizeof brains);}
static uint32_t rd32(uint32_t p){return ok(p,4)?MR32(p):0;}
#define FN_CAR_ACTIVATE 1
#define FN_CAR_SETDRIVER 2
#define FN_CAR_DEACTIVATE 3
#define FN_CAR_SETPHYS 4
static int written,parked,sync_ok=1,release_test,parks;
static void body_set(CPU *c,uint32_t p,int on){(void)c;CHECK(ok(p,0x9e0));if(on)CHECK(written&&sync_ok&&!parked);solid=on;}
static int apply_pose_mode(CPU *c,Slot *s,const rc_pose *p,float dt,int retain){(void)c;(void)s;(void)p;(void)dt;CHECK(!solid&&retain);written=1;return sync_ok;}
static void soft_park(CPU *c,uint32_t p){(void)c;(void)p;CHECK(!solid);parked=1;}
static void nv_reset(CPU *c,nv_slot *s){(void)c;resets++;if(release_test){CHECK(!solid&&MR8(address(car[1])+0x550));solid=1;}s->started=0;}
static uint32_t gcall0(CPU *c,int fn,uint32_t p){
 (void)c;CHECK(!solid);
 if(fn==FN_CAR_DEACTIVATE){parks++;MR8(p+0x550)=0;MR32(p+0x24)=0;MR32(p+0x2c)=0;return 0;}
 CHECK(fn==FN_CAR_ACTIVATE);activations++;MR8(p+0x550)=1;MR32(p+0x24)=4;MR32(p+0x2c)=address(brains[1]);solid=1;return 0;
}
static uint32_t gcall1(CPU *c,int fn,uint32_t p,uint32_t mode){
 if(fn==FN_CAR_SETPHYS){CHECK(mode==2&&!solid);physics_calls++;MR32(p+0x28)=MR8(p+0x5c0)?2:4;MR32(p+0x34)=address(brains[0]);return 0;}
 (void)c;CHECK(fn==FN_CAR_SETDRIVER&&mode==4&&!solid);replacements++;MR32(p+0x24)=mode;MR32(p+0x2c)=address(brains[1]);return 0;
}
#include "native_traffic_replica.inc"
static rc_session g_s;static rc_traffic_frame received;
static struct {int owned;struct {uint32_t car;}replica[RC_TRAFFIC_CARS];int64_t next_send;uint32_t generation,context,next_id;uint16_t seq;struct {uint32_t car,type,id;int active;float p[3];}host_car[RC_TRAFFIC_CARS];} NW;
static int same_context=1;static unsigned pool_count=2;
static int nw_same(void){return NW.owned&&same_context;}
static unsigned nw_pool(uint32_t out[RC_TRAFFIC_CARS]){for(unsigned i=0;i<pool_count;i++)out[i]=address(car[i]);return pool_count;}
#include "native-traffic-ownership-extracted.inc"
static nv_slot NW_MODELS[RC_TRAFFIC_CARS];
static void ni_forget_pending(void){}
static void rlog(const char *s,...){(void)s;}
#include "native-traffic-release-extracted.inc"
static void roam_plat_lock(void){CHECK(!locked);locked=1;}
static void roam_plat_unlock(void){CHECK(locked);locked=0;}
static uint32_t nw_type(uint32_t p){return p==address(car[0])?100:200;}
static int nc_read_car(uint32_t p,rc_state *s,int64_t now){(void)p;memset(s,0,sizeof *s);s->t_us=(uint32_t)now;s->q[3]=1;return 1;}
static void sent(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)u;(void)ip;(void)port;rc_msg m;CHECK(rc_decode(b,n,&m)==RC_OK&&m.h.type==RC_MSG_TRAFFIC);received=m.u.traffic;sends++;
}
#include "native-traffic-publish-extracted.inc"
int main(void){
 MR32(address(brains[0]))=0x785518;MR32(address(brains[1]))=0x785428;
 /* Real ownership predicate: unassigned pool cars, stale attachment and
  * the exact missing-goal state in both reported dumps. */
 uint32_t heavy=address(brains[0]),basic=address(brains[1]),p0=address(car[0]),p1=address(car[1]);
 MR32(p0+0x2c)=heavy;MR32(p1+0x2c)=basic;NW.owned=1;
 CHECK(nw_brain_owned(heavy)&&nw_brain_owned(basic));
 pool_count=0;NW.replica[0].car=p0;MR32(p0+0x2c)=basic;
 CHECK(!nw_brain_owned(heavy)&&nw_brain_owned(basic));
 NW.owned=0;CHECK(nw_brain_owned(basic));
 MR32(basic+0x6fc)=p1;CHECK(!nw_brain_owned(basic));
 CHECK(!nw_brain_owned(heavy)&&!nw_brain_owned(0));
 NW.owned=1;pool_count=2;same_context=0;CHECK(!nw_brain_owned(basic));
 same_context=1;CHECK(nw_brain_owned(basic));
 memset(&NW,0,sizeof NW);MR32(basic+0x6fc)=0;
 /* A car constructed outside a loaded cell falls back to mode4. Once the
  * cell is ready it must retry, without replacing a valid impact mode3. */
 MR32(p0+0x28)=0;MR8(p0+0x5c0)=0;solid=1;
 CHECK(nw_prepare_physics(0,p0)&&rd32(p0+0x28)==4&&physics_calls==1&&!solid);
 CHECK(nw_prepare_physics(0,p0)&&physics_calls==1);
 MR8(p0+0x5c0)=1;
 CHECK(nw_prepare_physics(0,p0)&&rd32(p0+0x28)==2&&physics_calls==2);
 CHECK(nw_prepare_physics(0,p0)&&physics_calls==2&&MR8(p0+0x5c0)==1);
 MR32(p0+0x28)=3;CHECK(nw_prepare_physics(0,p0)&&physics_calls==2);
 MR32(p0+0x34)=0;CHECK(!nw_prepare_physics(0,p0));
 for(int mode=0;mode<=5;mode++)for(int active=0;active<=1;active++)for(int started=0;started<=1;started++){
  memset(car,0,sizeof car);uint32_t p=address(car[0]);MR32(p+0x24)=mode;MR32(p+0x2c)=address(brains[mode==2?0:1]);MR8(p+0x550)=(uint8_t)active;
  nv_slot model={started,rd32(p+0x2c)};solid=1;activations=replacements=resets=0;
  CHECK(nw_prepare_replica(0,&model,p));CHECK(activations==!active);CHECK(nw_traffic_driver(rd32(p+0x24)));
  CHECK(resets==((!active||!nw_traffic_driver(mode))&&started));
  /* The real loader now freezes this already activated controller. */
  model.started=1;model.brain=rd32(p+0x2c);unsigned before=activations+replacements;
  CHECK(nw_prepare_replica(0,&model,p)&&model.brain==rd32(p+0x2c)&&before==activations+replacements);
  nw_park_replica(0,p);CHECK(!solid&&!MR8(p+0x550)&&rd32(p+0x24)==4&&rd32(rd32(p+0x2c))==0x785428);
 }
 rc_config cfg={0};cfg.self_id=100;cfg.port=47210;cfg.room=1234;cfg.traffic=cfg.world_host=1;rc_io io={0,sent,0,0};rc_session_init(&g_s,&cfg,&io);
 rc_peer *peer=&g_s.peer[0];peer->id=200;peer->state=RC_PS_EST;peer->traffic=1;peer->ip=0x7f000001;peer->port=47210;
 uint32_t pool[2]={address(car[0]),address(car[1])};memset(&NW,0,sizeof NW);NW.generation=77;
 for(int mode=0;mode<=5;mode++){
  MR8(pool[0]+0x550)=MR8(pool[1]+0x550)=1;MR32(pool[0]+0x24)=mode;MR32(pool[1]+0x24)=4;
  nw_publish(1000000+100000*mode,pool,2);CHECK(received.n==(nw_traffic_driver(mode)?2:1));
  ts_client client;ts_init(&client,200,100,0);CHECK(ts_receive(&client,100,&received,1000000+100000*mode,0)&&client.active);
  if(mode==4)CHECK(received.n==2); /* old host filter sent zero stock cars */
 }
 Slot visible={pool[0]};rc_pose pose={0};written=parked=0;solid=1;
 CHECK(!nw_apply_visible(0,&visible,0,.016f)&&!solid&&parked&&!written);
 written=parked=0;sync_ok=0;CHECK(!nw_apply_visible(0,&visible,&pose,.016f)&&!solid&&written);
 written=parked=0;sync_ok=1;MR8(pool[0]+0x550)=1;MR8(pool[0]+0x5c0)=1;MR32(pool[0]+0x554)=heavy;
 MR32(heavy)=pool[0];MR32(heavy+4)=rd32(pool[0]+0x10);MR32(heavy+8)=rd32(pool[0]+0xc);
 CHECK(nw_apply_visible(0,&visible,&pose,.016f)&&solid&&written&&!parked);
 for(int field=0;field<3;field++){
  uint32_t saved=MR32(heavy+4*field);MR32(heavy+4*field)^=4;
  CHECK(!nw_apply_visible(0,&visible,&pose,.016f)&&!solid);
  MR32(heavy+4*field)=saved;
 }
 /* Native cell/render/activation loss must not expose an invisible collider. */
 for(int active=0;active<2;active++)for(int cell=0;cell<2;cell++)for(int render=0;render<2;render++){
  MR8(pool[0]+0x550)=active;MR8(pool[0]+0x5c0)=cell;MR32(pool[0]+0x554)=render?heavy:0;
  written=parked=0;solid=1;
  CHECK(nw_apply_visible(0,&visible,&pose,.016f)==(active&&cell&&render));
  CHECK(solid==(active&&cell&&render)&&written);
 }
 /* Even inactive, unassigned pool entries are withdrawn before rebuild. */
 MR8(pool[0]+0x550)=MR8(pool[1]+0x550)=0;solid=1;nw_withdraw_pool(0,pool,2);CHECK(!solid);
 /* Mid-rebuild failure: only one mapping survived, but both native pool
  * entries must be parked, including a body recreated by ride restoration. */
 NW.owned=1;NW.replica[0].car=pool[0];NW.replica[1].car=0;
 MR8(pool[0]+0x550)=MR8(pool[1]+0x550)=1;NW_MODELS[1].started=1;
 solid=1;parks=0;release_test=1;nw_release(0);release_test=0;
 CHECK(parks==2&&!solid&&!NW.owned&&!NW_MODELS[1].started);
 CHECK(!MR8(pool[0]+0x550)&&!MR8(pool[1]+0x550));
 CHECK(sends==6);printf("PASS traffic replicas: %u checks, controller2/4 host datagrams accepted by real client reducer; native activation before controller freeze; no per-frame replacement; fresh visible pose before collision, failed physical sync and parked-body withdrawal; mock engine\n",checks);return 0;
}
