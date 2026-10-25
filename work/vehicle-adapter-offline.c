/* Actual adapter includes; synthetic memory/engine calls only. No game process. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "coop_stream.h"
#include "vehicle_sync.h"
#include "native_world.h"
typedef struct {int unused;} CPU;
typedef struct {uint32_t car,world,peer_id;int st,hidden,ghost;float com[3],spin[4],steer;} Slot;
#define NSLOT 4
#define SL_ACTIVE 1
static Slot g_sl[NSLOT];
#define RIDE_SZ 0x8f0
#define G_CARTYPES 0x8a1ccc
#define G_PARTOWNERS 0x80000100
#define G_PROFILE_NAME 0x83a9e0
#define G_FLOW 0x8654a4
#define G_CARLOADER 0x8a1cc0
#define FN_MALLOC 1
#define FN_TYPE_BY_HASH 2
#define FN_CAR_SETRIDE 3
#define FN_LOADER_LOAD 4
#define FN_LOADER_SERVICE 5
#define FN_LOADER_LOADED 6
#define FN_LOADER_WAIT 7
#define FN_CAR_BODYPOS 8
#define GPTR(a) ((void*)(uintptr_t)(a))
#define MR32(a) (*(uint32_t*)GPTR(a))
#define MR16(a) (*(uint16_t*)GPTR(a))
#define MR8(a) (*(uint8_t*)GPTR(a))
#define MRF32(a) (*(float*)GPTR(a))
#define rlog(...) ((void)0)
static uint8_t types[2*0x890],parts[2][16],human[0x9e0],replica[0x9e0],human_record[0x940],replica_record[0x940];
static uint32_t owners[256],g_gmem=1,live_world=0x12340000;
static char profile[17]="Owner";
static int test_flow=6;
static int rim_dependency=1,replace_on_wait;
static int nrim_ready(CPU *c,uint32_t ride){(void)c;(void)ride;return rim_dependency;}
static int lock_depth,done,wheels_pending,invalidate_wait,waits,body_off,load_calls,retuned,scoped,unloads;
static rc_session g_s;
static rc_carcfg owner_cfg,remote_cfg;
static struct {int bound,host;uint32_t peer,world;cc_race control;cs_stream stream;uint32_t car[6];Slot slot[6];} NC;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t addr(void *p){return (uint32_t)(uintptr_t)p;}
static int ok(uint32_t a,uint32_t n){return a>65535&&a+n>=a;}
static uint32_t rd32(uint32_t a){
 if(a==G_CARTYPES)return addr(types);if(a==G_PARTOWNERS)return addr(owners);if(a==G_FLOW)return test_flow;
 return a>65535?MR32(a):0;
}
static uint32_t world(void){return live_world;}
static int roaming(void){return 1;}
static uint32_t player_car(void){return addr(human);}
static void cstr(uint32_t a,char *out,size_t n){CHECK(a==G_PROFILE_NAME);snprintf(out,n,"%s",profile);}
static void roam_plat_lock(void){lock_depth++;}
static void roam_plat_unlock(void){CHECK(lock_depth==1);lock_depth--;}
static int64_t roam_plat_now_us(void){return 1000000;}
static void gmem_init(CPU *c){(void)c;}
static uint32_t skin_of(uint32_t record){return record==addr(replica_record)?2:1;}
static int skin_textures(CPU *c,uint32_t skin){(void)c;return skin>0&&skin<=6;}
static uint32_t loader_entry(uint32_t handle){CHECK(handle==22);return 23;}
static int pending_unloads,flushes;
static void unload_later(uint32_t h){CHECK(h==22);unloads++;pending_unloads++;}
static void flush_unloads(CPU *c){(void)c;CHECK(!lock_depth);pending_unloads=0;flushes++;}
static void body_set(CPU *c,uint32_t car,int solid){(void)c;CHECK(car==addr(replica));CHECK(!solid);body_off++;}
static void rd3(uint32_t a,float *v){memcpy(v,GPTR(a),12);}
static int nc_same_world(void){return NC.bound;}
static void copy_ride(uint32_t dst,uint32_t src){
 memcpy(GPTR(dst),GPTR(src),RIDE_SZ);MR32(dst+0x57c)=dst+0x46c;MR32(dst+0x38c)=dst+0x2f0;MR32(dst+0x390)=dst+0x40;
}
int nv_owned_performance(uint32_t ride);
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t object,uint32_t argument){
 (void)c;CHECK(!lock_depth);
 if(fn==FN_TYPE_BY_HASH)return argument==100?addr(types):argument==200?addr(types+0x890):0;
 if(fn==0x413470){copy_ride(object,argument);return object;}
 if(fn==FN_LOADER_LOADED)return done&&!wheels_pending;
 if(fn==FN_LOADER_WAIT){waits++;if(invalidate_wait)wheels_pending=1;if(replace_on_wait)MR32(addr(replica)+0x2c)++;return 0;}
 if(fn==FN_CAR_SETRIDE){
  CHECK(object==addr(replica)||(object==addr(human)&&nv_owned_performance(rd32(object+0x14)+0x10)));uint32_t target=rd32(object+0x14)+0x10;
  copy_ride(target,argument);MR32(object+0x10)=target;MR32(object+0x20)=target+0x40;
  MR32(object+0xc)=addr(types)+MR32(target)*0x890;
  if(nv_owned_performance(target))scoped++;else{retuned++;MR8(target+0x340)^=1;}
  return 0;
 }
 CHECK(0);return 0;
}
static uint32_t gcall0(CPU *c,uint32_t fn,uint32_t car){
 (void)c;CHECK(fn==FN_CAR_BODYPOS&&!lock_depth);
 uint32_t ride=rd32(car+0x10);for(int k=0;k<3;k++)MRF32(car+0x50+4*k)=MRF32(car+0x60+4*k)-MRF32(ride+0x40+4*k);
 return 0;
}
static uint32_t rh_call(CPU *c,uint32_t fn,uint32_t object,uint32_t edx,int argc,const uint32_t *args){
 (void)c;(void)object;(void)edx;CHECK(!lock_depth);
 if(fn==FN_MALLOC){CHECK(argc==1);return addr(calloc(1,args[0]));}
 if(fn==FN_LOADER_LOAD){CHECK(argc==3);load_calls++;return 22;}
 CHECK(fn==FN_LOADER_SERVICE&&argc==2);return 0;
}
static void prepare(uint32_t ride,const rc_carcfg *cfg,uint32_t skin){
 memset(GPTR(ride),0,RIDE_SZ);MR32(ride)=cfg->type_hash==100?0:1;memcpy(GPTR(ride+4),cfg->ride_b,4);
 for(int k=0;k<3;k++)MRF32(ride+0x40+4*k)=k==0?0.2f+MR32(ride)*0.1f:k==2?0.3f:0;
 for(int k=0;k<4;k++)MRF32(ride+0x60+0x30*k)=.35f+MR32(ride)*.01f;
 for(int k=0;k<10;k++)MR32(ride+0x318+4*k)=(cfg->perf[k>>3]>>(7-(k&7)))&1;
 for(int k=0;k<68;k++)MR8(ride+0x340+k)=(cfg->perf[2+(k>>3)]>>(7-(k&7)))&1;
 memcpy(GPTR(ride+0x398),cfg->perf+11,41);
 for(int k=0;k<64;k++)MRF32(ride+0x46c+4*k)=20.f+(float)k+(float)cfg->perf[11];
 MR32(ride+0x56c)=64;MRF32(ride+0x570)=0;MRF32(ride+0x574)=7000;MRF32(ride+0x578)=63.f/7000.f;
 MR32(ride+0x57c)=ride+0x46c;MR32(ride+0x38c)=ride+0x2f0;MR32(ride+0x390)=ride+0x40;
 for(int i=0;i<cfg->n_parts;i++)MR32(ride+0x590+4*cfg->part[i].slot)=addr(parts[i]);
 for(int i=0;i<3;i++)MR32(ride+0x584+4*i)=skin*100+i;
}
static int build_ride(CPU *c,const rc_carcfg *cfg,uint32_t ride,uint32_t skin,char *why,int whylen){
 (void)c;(void)why;(void)whylen;CHECK(!pending_unloads);prepare(ride,cfg,skin);return 0;
}
static int local_cfg(uint8_t *out,int cap,char *name){strcpy(name,owner_cfg.type_name);return rc_carcfg_encode(&owner_cfg,out,cap);}
#include "native_vehicle_verify.inc"
#include "native_vehicle_load.inc"
#ifdef ROSTER_ADAPTER
static void nai_release(CPU *);
#else
static void nai_release(CPU *c){(void)c;}
#endif
#include "native_race_vehicle.inc"
#include "native_race_vehicle_frame.inc"
/* Exercise the actual experimental contact adapter as well, with no socket. */
static struct {
 uint32_t peer,generation,context;int owned;
 int enabled,host;uint32_t world,manager,observer,observer_peer,spawn_turn;int64_t observer_until;
 struct {int64_t last_rx;uint32_t world;} client;
 struct {int active;uint32_t id,car,type;float p[3];} host_car[RC_TRAFFIC_CARS];
} NW;
static struct {
 struct {uint32_t car,id;int valid;int64_t at;rc_pose written;} sample[RC_TRAFFIC_CARS];
 struct {rc_traffic_contact contact;int left;int64_t at;} pending[8];
 struct {uint32_t peer,world;uint16_t seq;uint64_t seen;float tokens;int64_t at;} replay[RC_TABLE];
 uint16_t seq;int64_t next_capture;uint32_t sent,accepted,rejected;
} NWC;
static uint32_t nw_type(uint32_t car){return rd32(rd32(car+0xc)+0xd0);}
static int nc_read_car(uint32_t car,rc_state *s,int64_t now){
 memset(s,0,sizeof *s);s->t_us=(uint32_t)now;rd3(car+0x60,s->p);rd3(car+0x70,s->v);rd3(car+0x110,s->w);return 1;
}
#include "native_traffic_contact.inc"
static uint8_t interest_world[0x11c],interest_car[4][0x9e0],interest_brain[4][0x7d4];
static unsigned nw_pool(uint32_t out[RC_TRAFFIC_CARS]){for(int i=0;i<4;i++)out[i]=addr(interest_car[i]);return 4;}
#include "native_traffic_interest.inc"
static unsigned eligible_calls;
static uint8_t __attribute__((thiscall)) eligible_no(void *m,uint32_t car){(void)m;(void)car;eligible_calls++;return 0;}
static uint8_t __attribute__((thiscall)) eligible_yes(void *m,uint32_t car){(void)m;(void)car;eligible_calls++;return 1;}
static uint8_t __attribute__((thiscall)) spawn_original(void *m,uint32_t car,const float *density){(void)m;(void)car;CHECK(density[0]==1);return 7;}
static void interest(void){
 live_world=addr(interest_world);NW.world=live_world;NW.manager=0x123456;MR32(live_world+0x118)=NW.manager;NW.enabled=NW.host=1;NW.context=0;
 g_sl[0].car=addr(replica);g_sl[0].world=live_world;g_sl[0].peer_id=20;g_sl[0].st=SL_ACTIVE;
 rc_peer *peer=&g_s.peer[0];peer->world_host=0;peer->status=RC_ST_ROAM;peer->last_snap_rx=1000000;
 MR32(addr(replica)+0x8fc)=12345;MR8(addr(replica)+0x550)=1;MRF32(addr(replica)+0x60)=500;MRF32(addr(human)+0x60)=0;
 MRF32(addr(human)+0x90)=MRF32(addr(replica)+0x90)=1;MRF32(addr(human)+0x94)=MRF32(addr(replica)+0x94)=0;
 nw_update_interest(1000000);CHECK(nw_observer_valid()&&NW.observer==addr(replica));
 MRF32(live_world+0x90)=77;MRF32(live_world+0x94)=88;MRF32(live_world+0x98)=99;MR32(live_world+0xe4)=42;
 nw_interest_scope outer,inner;nw_interest_begin(&outer);CHECK(outer.active&&MRF32(live_world+0x90)==500&&MR32(live_world+0xe4)==12345);
 nw_interest_begin(&inner);nw_interest_end(&inner);CHECK(MRF32(live_world+0x90)==500);nw_interest_end(&outer);
 CHECK(MRF32(live_world+0x90)==77&&MRF32(live_world+0x94)==88&&MRF32(live_world+0x98)==99&&MR32(live_world+0xe4)==42);
 NW.observer_until=999999;nw_interest_begin(&outer);CHECK(!outer.active&&MRF32(live_world+0x90)==77);
 nw_update_interest(1000000);
 for(int i=0;i<4;i++){
  uint32_t car=addr(interest_car[i]);MR8(car+0x550)=i<3;MR32(car+0x24)=2;MRF32(car+0x60)=-120.f-10.f*i;MRF32(car+0x64)=20;
  MR32(car+0x2c)=addr(interest_brain[i]);MR32(addr(interest_brain[i]))=0x785518;
 }
 NW.owned=0;eligible_calls=0;
 MR32(addr(interest_brain[0]))=0x7a5cb0;CHECK(!nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)&&!eligible_calls);
 MR32(addr(interest_brain[0]))=0x785518;MR32(addr(interest_car[0])+0x24)=0;
 CHECK(!nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)&&!eligible_calls);
 MR32(addr(interest_car[0])+0x24)=2;NW.owned=1;
 CHECK(!nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)&&!eligible_calls);NW.owned=0;
 MR32(addr(interest_car[0])+0x24)=4;MR32(addr(interest_brain[0]))=0x785428;
 CHECK(!nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)&&!eligible_calls);
 MR32(addr(interest_brain[0])+0x6fc)=addr(human);
 CHECK(nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)==1&&eligible_calls==1);
 MR32(addr(interest_car[0])+0x24)=2;MR32(addr(interest_brain[0]))=0x785518;
 CHECK(nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_no)==1);
 MRF32(addr(interest_car[0])+0x60)=20;CHECK(nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_no)==0);
 CHECK(nw_recycle((void*)(uintptr_t)NW.manager,addr(interest_car[0]),eligible_yes)==1);
 float density[3]={1,2,3};MRF32(addr(replica)+0x60)=10;
 CHECK(nw_spawn((void*)(uintptr_t)NW.manager,addr(interest_car[0]),density,spawn_original)==7);
 peer->lost=1;nw_update_interest(1000000);CHECK(!NW.observer&&!nw_observer_valid());peer->lost=0;
 peer->status=RC_ST_PAUSED;peer->last_snap_rx=1;peer->last_rx_us=1000000;nw_update_interest(1000000);CHECK(nw_observer_valid());
 NW.context=90;nw_update_interest(1000000);CHECK(!NW.observer);
 NC.bound=1;NC.host=1;NC.peer=peer->id;NC.control.phase=CC_RUNNING;NC.control.event=90;
 NC.car[1]=addr(replica);NC.stream.last_rx=1000000;peer->status=RC_ST_EVENT;
 nw_update_interest(1000000);CHECK(nw_observer_valid()&&NW.observer==addr(replica));
 nw_interest_begin(&outer);CHECK(outer.active);nw_interest_end(&outer);CHECK(MRF32(live_world+0x90)==77);
 NC.control.event=91;nw_update_interest(1000000);CHECK(!NW.observer);
 NC.control.event=90;NC.stream.last_rx=499999;nw_update_interest(1000000);CHECK(!NW.observer);
 NC.stream.last_rx=1000000;NC.control.phase=CC_DONE;nw_update_interest(1000000);CHECK(!NW.observer);
 NC.control.phase=CC_RUNNING;peer->lost=1;nw_update_interest(1000000);CHECK(!NW.observer);peer->lost=0;
 NC.bound=0;NW.context=0;
 puts("PASS actual traffic-interest adapter: live/paused/disconnected observers, nested native-cache restoration, expiry, shared-pool surplus recycling, nearby-car protection, native forwarding and event isolation; no native road placement executed");
}
static void deliver_hit(rc_traffic_contact *hit,uint32_t from){
 g_s.n_traffic_contact=1;g_s.traffic_contact_in[0].from=from;g_s.traffic_contact_in[0].contact=*hit;
 nw_host_contacts(1000000);
}
static unsigned hit_packets;static uint16_t hit_sequence;
static void hit_tx(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)u;(void)ip;(void)port;rc_msg m;CHECK(rc_decode(b,n,&m)==RC_OK&&m.h.type==RC_MSG_TRAFFIC_CONTACT);
 if(hit_packets)CHECK(hit_sequence==m.u.traffic_contact.seq);hit_sequence=m.u.traffic_contact.seq;hit_packets++;
}
static void contacts(void){
 memset(&NW,0,sizeof NW);memset(&NWC,0,sizeof NWC);memset(&g_s,0,sizeof g_s);
 g_s.conf.self_id=10;g_s.conf.traffic=g_s.conf.traffic_contact=g_s.conf.world_host=1;
 rc_peer *peer=&g_s.peer[0];peer->id=20;peer->state=RC_PS_EST;peer->traffic=peer->traffic_contact=1;
 peer->clock_valid=1;peer->clock_sample_at=1000000;peer->clock_offset_us=300000;peer->mo.n=1;peer->last_snap_rx=1000000;
 peer->mo.s[0].p[0]=9;peer->mo.s[0].p[1]=20;
 NW.generation=7;NW.host_car[0].id=1;NW.host_car[0].car=addr(replica);NW.host_car[0].type=100;NW.host_car[0].active=1;
 rd3(addr(replica)+0x60,NW.host_car[0].p);MR8(addr(replica)+0x550)=1;MR32(addr(replica)+0x24)=2;
 rc_traffic_contact hit={0};hit.to=10;hit.world=7;hit.id=1;hit.seq=10;hit.t_us=700000;hit.dv[0]=1;
 rd3(addr(replica)+0x60,hit.p);hit.player[0]=9;hit.player[1]=20;
 deliver_hit(&hit,20);CHECK(NWC.accepted==1&&MRF32(addr(replica)+0x70)==1);
 deliver_hit(&hit,20);CHECK(NWC.accepted==1&&MRF32(addr(replica)+0x70)==1);
 hit.seq=12;deliver_hit(&hit,20);hit.seq=11;deliver_hit(&hit,20);CHECK(NWC.accepted==3&&MRF32(addr(replica)+0x70)==3);
 deliver_hit(&hit,20);CHECK(NWC.accepted==3);
 hit.seq=13;hit.world=6;deliver_hit(&hit,20);CHECK(NWC.accepted==3);hit.world=7;
 hit.t_us=100000;deliver_hit(&hit,20);CHECK(NWC.accepted==3);hit.t_us=700000;
 hit.id=999;deliver_hit(&hit,20);CHECK(NWC.accepted==3);hit.id=1;
 deliver_hit(&hit,99);CHECK(NWC.accepted==3);
 hit.player[0]=1000;deliver_hit(&hit,20);CHECK(NWC.accepted==3);hit.player[0]=9;
 hit.dv[0]=12;deliver_hit(&hit,20);CHECK(NWC.accepted==4&&MRF32(addr(replica)+0x70)==15);
 hit.seq=14;deliver_hit(&hit,20);CHECK(NWC.accepted==4); /* token budget */
 memset(&NWC,0,sizeof NWC);g_s.conf.world_host=0;g_s.io.send=hit_tx;peer->world_host=1;NW.peer=20;NW.owned=1;
 NW.client.world=7;NW.client.last_rx=1000000;rc_pose pose={0};rd3(addr(replica)+0x60,pose.p);
 MRF32(addr(human)+0x60)=9;MRF32(addr(human)+0x64)=20;MRF32(addr(replica)+0x70)=2;
 nw_contact_sample(addr(replica),1,0,&pose,1000000);nw_capture_contact(addr(replica),1,0,1016000);
 CHECK(NWC.pending[0].left==3);nw_send_contacts(1016000);nw_send_contacts(1066000);nw_send_contacts(1116000);
 CHECK(hit_packets==3&&NWC.sent==3&&NWC.pending[0].left==0);
 puts("PASS actual contact adapter: clock offset, duplicate/reorder window, stale/world/identity/proximity rejection, token budget, pre-correction capture and three same-ID transmissions");
}
static void setup(unsigned run){
 CPU c={0};if(NV_RACE.started)nv_race_release(&c);memset(&NV_BARRIER,0,sizeof NV_BARRIER);nv_solo_start();
 memset(&g_s,0,sizeof g_s);g_s.conf.self_id=10;g_s.conf.coop=g_s.conf.vehicles=g_s.conf.vehicle_proof=1;
 memset(&NC,0,sizeof NC);NC.bound=NC.host=1;NC.peer=20;NC.control.run=run;NC.control.event=90;NC.control.phase=CC_LOADING;
 NC.car[0]=addr(human);NC.car[1]=addr(replica);NC.slot[1].car=addr(replica);
 strcpy(profile,"Owner");done=0;
 prepare(addr(human_record)+0x10,&owner_cfg,1);prepare(addr(replica_record)+0x10,&owner_cfg,2);
 for(int i=0;i<2;i++){
  uint32_t car=i?addr(replica):addr(human),rec=i?addr(replica_record):addr(human_record);memset(GPTR(car),0,0x9e0);
  MR32(car+0x14)=rec;MR32(car+0x10)=rec+0x10;MR32(car+0x20)=rec+0x50;MR32(car+0xc)=addr(types);MR32(car+0x2c)=123456;
  MRF32(car+0x94)=1;MRF32(car+0xa0)=-1;MRF32(car+0xb8)=1;MRF32(car+0x60)=10;MRF32(car+0x64)=20;
 }
 rc_peer *p=&g_s.peer[0];p->state=RC_PS_EST;p->id=20;p->vehicles=p->vehicle_proof=1;p->cfg_rev=p->cfg_rev_seen=1;
 p->cfg_len=rc_carcfg_encode(&remote_cfg,p->cfg,sizeof p->cfg);
}
static void announce(uint32_t physical){
 rc_coop_msg m={RC_COOP_VEHICLE,10,NC.control.run,90,rc_fnv1a_bytes(g_s.peer[0].cfg,g_s.peer[0].cfg_len)};
 CHECK(vs_receive(&NV_BARRIER,20,&m));m.op=RC_COOP_VEHICLE_PHYSICS;m.value=physical;CHECK(vs_receive(&NV_BARRIER,20,&m));
}
int main(void){
 MR32(addr(types)+0xd0)=100;MR32(addr(types)+0x890+0xd0)=200;owners[1]=0x12345678;
 MR32(addr(parts[0]))=0xabcd;MR32(addr(parts[1]))=0xbcde;parts[0][7]=parts[1][7]=1;
 owner_cfg.type_hash=100;strcpy(owner_cfg.type_name,"ONE");owner_cfg.ride_b[3]=1;owner_cfg.has_perf=1;owner_cfg.n_parts=2;
 owner_cfg.part[0].slot=10;owner_cfg.part[0].key=0xabcd;owner_cfg.part[0].owner=owners[1];
 owner_cfg.part[1].slot=80;owner_cfg.part[1].key=0xbcde;owner_cfg.part[1].owner=owners[1];memset(owner_cfg.perf+11,100,41);
 remote_cfg=owner_cfg;remote_cfg.type_hash=200;strcpy(remote_cfg.type_name,"TWO");remote_cfg.perf[0]=128;remote_cfg.perf[2]=128;remote_cfg.perf[11]=150;
 CPU cpu={0};setup(1);nv_publish_local();CHECK(nv_freeze_race(&cpu,20,1,90,0));CHECK(nv_cfg_pinned()&&!NV_BARRIER.initialized);
 uint32_t old_digest=NV_FROZEN.digest;owner_cfg.perf[11]=125;prepare(addr(human_record)+0x10,&owner_cfg,1);
 CHECK(nc_vehicle_frame(&cpu,10000)==0);CHECK(NV_FROZEN.finalized&&NV_FROZEN.digest!=old_digest&&NV_BARRIER.proof_required);
 uint8_t reference[RIDE_SZ];prepare(addr(reference),&remote_cfg,2);uint32_t proof;CHECK(nv_physics_signature(addr(reference),&proof));announce(proof);
 CHECK(nc_vehicle_frame(&cpu,20000)==0&&!NV_BARRIER.loaded&&NV_RACE.started&&!NV_RACE.applied);
 CHECK(rd32(rd32(addr(replica)+0xc)+0xd0)==100);done=1;wheels_pending=1;
 CHECK(nc_vehicle_frame(&cpu,25000)==0&&!NV_RACE.applied&&!NV_BARRIER.loaded);
 wheels_pending=0;invalidate_wait=1;
 CHECK(nc_vehicle_frame(&cpu,26000)==0&&!NV_RACE.applied&&!NV_BARRIER.loaded);
 invalidate_wait=wheels_pending=0;CHECK(nc_vehicle_frame(&cpu,30000)==0);
 CHECK(NV_BARRIER.loaded&&NV_RACE.applied&&scoped==1&&retuned==0&&waits==2);
 CHECK(rd32(rd32(addr(replica)+0xc)+0xd0)==200);CHECK(NC.slot[1].com[0]>.299f&&NC.slot[1].com[0]<.301f&&NC.slot[1].com[1]==0);
 rc_coop_msg ack={RC_COOP_VEHICLE_ACK,10,1,90,NV_FROZEN.digest};CHECK(vs_receive(&NV_BARRIER,20,&ack));CHECK(nc_vehicle_frame(&cpu,40000)==1);
 MR32(rd32(addr(replica)+0x10)+0x590+10*4)=0;CHECK(nc_vehicle_frame(&cpu,50000)==-1);MR32(rd32(addr(replica)+0x10)+0x590+10*4)=addr(parts[0]);
 strcpy(profile,"Changed");CHECK(nc_vehicle_frame(&cpu,60000)==-1);strcpy(profile,"Owner");
 nv_race_release(&cpu);CHECK(!nv_cfg_pinned()&&!NV_RACE.started&&unloads==1&&retuned==1);
 setup(2);CHECK(nv_freeze_race(&cpu,20,2,90,0));CHECK(nc_vehicle_frame(&cpu,10000)==0);announce(proof^1);done=1;
 CHECK(nc_vehicle_frame(&cpu,20000)==-1&&NV_BARRIER.fault&&!vs_ready(&NV_BARRIER));nv_race_release(&cpu);
 setup(3);CHECK(nv_freeze_race(&cpu,20,3,90,0));MR8(addr(human_record)+0x10+0x340)=1;NC.control.phase=CC_RUNNING;CHECK(nc_vehicle_frame(&cpu,10000)==-1);nv_race_release(&cpu);
 CHECK(lock_depth==0&&load_calls==2&&body_off>0);
 setup(4);unsigned scoped_before=scoped,retuned_before=retuned;
 nv_free_setride(&cpu,addr(replica),addr(human_record)+0x10);
 CHECK(scoped==scoped_before+1&&retuned==retuned_before);
 CHECK(!NV_FREE_ATTACH.car&&!NV_FREE_ATTACH.world&&!NV_FREE_ATTACH.record);
 CHECK(!nv_owned_performance(addr(replica_record)+0x10)&&!nv_owned_performance(addr(human_record)+0x10));
 gcall1(&cpu,FN_CAR_SETRIDE,addr(replica),addr(human_record)+0x10);
 CHECK(retuned==retuned_before+1);
 /* Native loading substitutes another car: recover the frozen human
  * configuration, wait for real resources, retain the handle while live. */
 setup(5);CHECK(nv_freeze_race(&cpu,20,5,90,0));
 prepare(addr(human_record)+0x10,&remote_cfg,1);MR32(addr(human)+0xc)=addr(types)+0x890;
 int own_off_before=body_off;CHECK(nc_vehicle_frame(&cpu,10000)==0&&NV_OWNER.started&&!NV_OWNER.applied&&!NV_FROZEN.finalized);
 CHECK(!NV_OWNER_CAR);done=1;CHECK(nc_vehicle_frame(&cpu,20000)==0&&NV_OWNER.applied&&NV_FROZEN.finalized);
 CHECK(nv_matches_cfg(&owner_cfg,rd32(addr(human)+0x10))&&body_off==own_off_before&&!NV_OWNER_CAR);
 nv_race_release(&cpu);CHECK(NV_OWNER.started&&NV_OWNER.handle);uint32_t prior_world=live_world;
 live_world++;nv_owner_cleanup(&cpu);CHECK(!NV_OWNER.started);live_world=prior_world;flush_unloads(&cpu);
 /* Exercise the real reset/begin path repeatedly with race-style deferred
  * releases: the next build must never reuse a still-retained buffer. */
 setup(6);done=1;nv_slot stock={0};
 uint32_t material[3]={0xdead,0xbeef,0};memcpy(GPTR(rd32(addr(replica)+0x10)+0x584),material,12);
 CHECK(nv_begin(&cpu,&stock,addr(replica),&remote_cfg,1,1,0)==0);
 CHECK(!memcmp(GPTR(stock.ride+0x584),material,12));nv_reset(&cpu,&stock);flush_unloads(&cpu);
 nv_slot churn={0};int before_flush=flushes;
 for(int i=0;i<1000;i++){
  CHECK(nv_begin(&cpu,&churn,addr(replica),&remote_cfg,2,100000+i,1)==0);
  CHECK(!pending_unloads&&nv_poll(&cpu,&churn,200000+i)==1);
  nv_reset(&cpu,&churn);CHECK(pending_unloads==1);
 }
 flush_unloads(&cpu);CHECK(!pending_unloads&&flushes>=before_flush+1000);
 /* Suspended tuning scene: pin the attached model, perform no native car
  * calls/unload, then release normally after the old world disappears. */
 CHECK(nv_begin(&cpu,&churn,addr(replica),&remote_cfg,2,300000,1)==0);
 CHECK(nv_poll(&cpu,&churn,310000)==1);unsigned parked_scoped=scoped;
 test_flow=3;nv_reset(&cpu,&churn);
 CHECK(churn.started&&churn.handle&&!pending_unloads&&scoped==parked_scoped);
 live_world++;nv_reset(&cpu,&churn);CHECK(!churn.started&&pending_unloads==1);
 live_world--;test_flow=6;flush_unloads(&cpu);
 /* Mock world teardown does not actually destroy/recreate this array. */
 gcall1(&cpu,FN_CAR_SETRIDE,addr(replica),churn.saved);
 /* The wheel brand header must precede native model construction. A
  * generic CarLoader ready bit alone cannot satisfy this dependency. */
 nv_slot delayed_rim={0};int rim_loads_before=load_calls;rim_dependency=0;
 CHECK(nv_begin(&cpu,&delayed_rim,addr(replica),&remote_cfg,2,400000,1)==0);
 CHECK(delayed_rim.started&&!delayed_rim.handle&&load_calls==rim_loads_before);
 CHECK(nv_poll(&cpu,&delayed_rim,410000)==0&&load_calls==rim_loads_before);
 rim_dependency=1;CHECK(nv_poll(&cpu,&delayed_rim,420000)==1&&load_calls==rim_loads_before+1);
 nv_reset(&cpu,&delayed_rim);flush_unloads(&cpu);
 /* Ordinary remote attachment, polling and stale rollback cannot target
  * the local human even when a game pool reuses its address. */
 nv_slot forbidden={0};CHECK(nv_begin(&cpu,&forbidden,addr(human),&remote_cfg,1,1,1)==-1);
 unsigned prior_scoped=scoped;nv_free_setride(&cpu,addr(human),addr(replica_record)+0x10);CHECK(scoped==prior_scoped);
 forbidden=churn;forbidden.applied=forbidden.started=1;forbidden.car=addr(human);forbidden.world=live_world;forbidden.record=addr(human_record);forbidden.brain=rd32(addr(human)+0x2c);forbidden.handle=22;
 CHECK(nv_poll(&cpu,&forbidden,300000)==-1);nv_reset(&cpu,&forbidden);CHECK(body_off>=own_off_before&&scoped==prior_scoped);
 CHECK(forbidden.started&&forbidden.handle==22);live_world++;nv_reset(&cpu,&forbidden);CHECK(!forbidden.started);live_world--;
 flush_unloads(&cpu);
 /* A new controller does not mean its old model was detached. */
 setup(7);done=1;nv_slot retained={0};
 CHECK(nv_begin(&cpu,&retained,addr(replica),&remote_cfg,2,1,1)==0);
 CHECK(nv_poll(&cpu,&retained,2)==1);MR32(addr(replica)+0x2c)++;
 int old_unloads=unloads;nv_reset(&cpu,&retained);
 CHECK(retained.started&&retained.applied&&unloads==old_unloads);
 CHECK(nv_begin(&cpu,&retained,addr(replica),&remote_cfg,2,3,1)==-1);
 live_world++;nv_reset(&cpu,&retained);CHECK(!retained.started&&unloads==old_unloads+1);
 flush_unloads(&cpu);
 setup(8);done=1;nv_slot changed={0};
 CHECK(nv_begin(&cpu,&changed,addr(replica),&remote_cfg,2,1,1)==0);
 unsigned old_scoped=scoped;replace_on_wait=1;
 CHECK(nv_poll(&cpu,&changed,2)==-1&&!changed.applied&&scoped==old_scoped);
 replace_on_wait=0;nv_reset(&cpu,&changed);flush_unloads(&cpu);
 contacts();interest();CHECK(!lock_depth);
 printf("PASS %u actual vehicle-adapter checks: pre-load freeze, owner race tuning, CFG pin, delayed loader, scope, parts/physics proof, model COM, profile/upgrade rejection, rollback; synthetic native backend only\n",checks);
 return 0;
}
