/* Actual player and traffic adapters with synthetic objects/math only. */
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
#define JUMP_M 50.f
#define GPTR(a) ((void*)(uintptr_t)(a))
#define MR32(a) (*(uint32_t*)GPTR(a))
#define MR8(a) (*(uint8_t*)GPTR(a))
#define MRF32(a) (*(float*)GPTR(a))
static uint8_t cars[2][0x9e0],bodies[2][0x1a0],physical[2][0x30],info[2][0x200],rides[2][0x8f0],pair[0x20];
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int64_t time_now=1000000;static uint32_t live_world=900;static int paused,city=1,lock_depth,solves,refreshes,setters,bad_refresh;
static int replace_contact_body;
static uint32_t expected_class;static Slot g_sl[NSLOT];static rc_session g_s;
static struct {int bound,host;uint32_t peer,car[6];cc_race control;cs_stream stream;} NC;
static struct {
 int enabled,host,owned;uint32_t world,context,generation,peer;
 struct {uint32_t world;int64_t last_rx;} client;
 struct {int active;uint32_t car,id,type;} host_car[16];
 struct {uint32_t car,id;} replica[16];
} NW;
static uint32_t addr(const void *p){return (uint32_t)(uintptr_t)p;}
static int ok(uint32_t a,uint32_t n){
 const void *starts[]={cars,bodies,physical,info,rides,pair};unsigned sizes[]={sizeof cars,sizeof bodies,sizeof physical,sizeof info,sizeof rides,sizeof pair};
 if(a+n<a)return 0;
 for(int k=0;k<6;k++)if(a>=addr(starts[k])&&a+n<=addr(starts[k])+sizes[k])return 1;
 return 0;
}
static uint32_t rd32(uint32_t a){return a==G_FLOW?6:ok(a,4)?MR32(a):0;}
static void rd3(uint32_t a,float *v){CHECK(ok(a,12));memcpy(v,GPTR(a),12);}
static uint32_t world(void){return live_world;}
static uint32_t player_car(void){return addr(cars[0]);}
static int nc_same_world(void){return NC.bound;}
static int roaming(void){return city;}
static int nc_pause_raw(int original){return original||paused;}
static int clock_step;
static int64_t roam_plat_now_us(void){time_now+=clock_step;return time_now;}
static uint32_t roam_plat_random(void){return 123456;}
static void roam_plat_lock(void){CHECK(!lock_depth);lock_depth=1;}
static void roam_plat_unlock(void){CHECK(lock_depth);lock_depth=0;}
static void rlog(const char *s,...){(void)s;}
static float nw_len2(const float *v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
static float nw_dist2(const float *a,const float *b){float v[3];for(int k=0;k<3;k++)v[k]=a[k]-b[k];return nw_len2(v);}
static uint32_t nw_type(uint32_t car){(void)car;return 1;}
static int nc_read_car(uint32_t car,rc_state *s,int64_t now){(void)now;memset(s,0,sizeof *s);rd3(car+0x60,s->p);return 1;}
static void body_set(CPU *c,uint32_t car,int on){(void)c;CHECK(ok(car,0x9e0));MR8(rd32(car+0x3c)+0x17)=!on;}
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t self,uint32_t arg){
 (void)c;CHECK(!lock_depth&&fn==0x5938b0&&self==addr(bodies[0])&&arg==addr(bodies[1]));
 if(replace_contact_body)MR32(addr(cars[0])+0x3c)=addr(bodies[1]);return 0;
}
void ni_apply(uint32_t body,const float *j,const float *r){(void)body;(void)j;(void)r;CHECK(0);}
static uint32_t gcall0(CPU *c,uint32_t fn,uint32_t body){
 (void)c;CHECK(!lock_depth&&fn==0x593670&&ok(body,0x134));uint32_t pi=rd32(body+0x68);refreshes++;
 uint32_t car=rd32(body+0x130);
 for(int k=0;k<3;k++){
  MRF32(pi+0xa0+4*k)=bad_refresh?300:MRF32(pi+0x70+4*k)*MRF32(pi+0x168);
  MRF32(pi+0xc0+4*k)=MRF32(pi+0x80+4*k); /* mock unit inertia */
  MRF32(car+0x70+4*k)=MRF32(pi+0xa0+4*k);MRF32(car+0x110+4*k)=MRF32(pi+0xc0+4*k);
 }
 if(bad_refresh)bad_refresh--;
 return 0;
}
void ni_set_state(uint32_t pi,const float *p,const float *m,const float *v,const float *w){
 CHECK(!lock_depth&&ok(pi,0x1f0));setters++;
 memcpy(GPTR(pi+0x20),p,12);memcpy(GPTR(pi+0x30),m,64);
 for(int k=0;k<3;k++){MRF32(pi+0x70+4*k)=v[k]*MRF32(pi+0x164);MRF32(pi+0x80+4*k)=w[k];MRF32(pi+0xa0+4*k)=v[k];MRF32(pi+0xc0+4*k)=w[k];}
}
static rc_msg sent[128];static int nsent;
static void send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)u;CHECK(ip==0x7f000001&&port>=47210&&port<=47211&&nsent<128);CHECK(rc_decode(b,n,&sent[nsent++])==RC_OK);
}
#include "native_physics_signature.inc"
#include "native_contact_impulse.inc"
#include "native_player_impulse.inc"
static void setup(int host){
 memset(&g_s,0,sizeof g_s);memset(&NPI,0,sizeof NPI);memset(&NIP,0,sizeof NIP);memset(&NC,0,sizeof NC);memset(&NW,0,sizeof NW);
 memset(cars,0,sizeof cars);memset(bodies,0,sizeof bodies);memset(info,0,sizeof info);memset(rides,0,sizeof rides);memset(g_sl,0,sizeof g_sl);memset(pair,0,sizeof pair);
 time_now=1000000;clock_step=0;live_world=900;paused=solves=refreshes=setters=bad_refresh=nsent=lock_depth=0;city=1;expected_class=1;
 for(int k=0;k<2;k++){
  uint32_t car=addr(cars[k]),body=addr(bodies[k]),pi=addr(info[k]),ride=addr(rides[k]);
  MR32(car+0x3c)=body;MR32(car+0x34)=addr(physical[k]);MR32(car+0x28)=k?3:1;MR32(car+0x10)=ride;MR32(car+0x38)=k?0:1;
  MR32(body)=0x7a2478;MR32(body+0x130)=car;MR32(body+0x5c)=1;MR32(body+0x64)=addr(physical[k])+8;MR32(body+0x68)=pi;
  MRF32(pi+0x164)=2;MRF32(pi+0x168)=.5f;MRF32(pi+0x170)=MRF32(pi+0x184)=MRF32(pi+0x198)=1;
  MRF32(car+0x90)=MRF32(car+0xa4)=MRF32(car+0xb8)=1;
  MR32(ride+0x56c)=64;MR32(ride+0x57c)=ride+0x46c;MR32(ride+0x38c)=ride+0x2f0;MR32(ride+0x390)=ride+0x40;
  for(int j=0;j<4;j++)MRF32(ride+0x60+0x30*j)=.3f;
 }
 MR32(addr(pair)+0xc)=addr(bodies[0]);MR32(addr(pair)+0x10)=addr(bodies[1]);
 g_s.conf.self_id=host?100:200;g_s.conf.extension_nonce=host?111:222;g_s.conf.world_host=host;g_s.conf.player_contact=1;
 g_s.conf.port=host?47210:47211;g_s.conf.platform=RC_PLAT_WIN;g_s.conf.rate_hz=20;g_s.conf.snap_states=1;strcpy(g_s.conf.name,host?"Host":"Guest");
 g_s.conf.extensions=RC_EXT_PLAYER_IMPULSE|(host?RC_EXT_CONTACT_HOST:0);g_s.conf.test_mode=1;g_s.conf.max_players=2;g_s.status=RC_ST_ROAM;g_s.io.send=send_packet;
 rc_peer *p=&g_s.peer[0];p->id=host?200:100;p->state=RC_PS_EST;p->status=RC_ST_ROAM;p->core_ver=4;p->player_contact=1;
 p->ip=0x7f000001;p->port=host?47211:47210;p->clock_valid=1;p->clock_rtt_us=1000;p->clock_sample_at=time_now;
 p->extension_seen=1;p->extension_at=time_now;p->extensions=(rc_extensions){g_s.conf.self_id,host?222:111,g_s.conf.extension_nonce,RC_EXT_PLAYER_IMPULSE|(host?0:RC_EXT_CONTACT_HOST),1};
 p->last_snap_rx=time_now;p->mo.n=1;
 g_sl[0]=(Slot){addr(cars[1]),world(),p->id,SL_ACTIVE};
 uint32_t hash;CHECK(npi_physics(addr(cars[0]),&hash));
 p->contact_seen=1;p->contact_at=time_now;p->contact_scene=(rc_contact_scene){g_s.conf.self_id,p->extensions.nonce,g_s.conf.extension_nonce,1,host?202:101,0,0,0,hash,1,1};
 npi_publish(time_now); /* creates the local actual token */
 p->contact_scene.peer_world=NPI.token;CHECK(rc_player_contact_ready(&g_s,p,time_now));
 CHECK(npi_sync_proxy(0,addr(cars[1])));CHECK(npi_contact_ready(p->id,time_now));
}
static void __attribute__((thiscall)) solve_player(void *object){
 CHECK(object==pair&&!lock_depth);CHECK(rd32(addr(cars[1])+0x38)==expected_class);solves++;
 float before[6];uint32_t pi=npi_before(addr(bodies[1]),before);CHECK(pi==addr(info[1]));
 MRF32(pi+0x70)+=10;MRF32(pi+0x88)+=2;npi_after(addr(bodies[1]),pi,before);
 pi=npi_before(addr(bodies[1]),before);MRF32(pi+0x74)+=4;npi_after(addr(bodies[1]),pi,before);
}
static void receive(const rc_player_impulse *hit,uint32_t from){
 rc_msg m={0};m.h.type=RC_MSG_PLAYER_IMPULSE;m.h.sender=from;m.u.player_impulse=*hit;uint8_t bytes[RC_MAX_DGRAM];
 int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==108);rc_receive(&g_s,0x7f000001,g_s.peer[0].port,bytes,n,time_now);
}
static unsigned noise=0xabc987;
static unsigned random32(void){noise^=noise<<13;noise^=noise>>17;noise^=noise<<5;return noise;}
static rc_session sim_session[2];static __typeof__(NPI) sim_player[2];
static uint8_t sim_cars[2][sizeof cars],sim_info[2][sizeof info];
static struct {rc_msg msg;int role;int64_t at;} deliveries[256];
static void simulate_loss(void){
 unsigned applied=0,confirmed=0;
 for(int run=0;run<500;run++){
  CPU cpu={0};int nd=0;
  setup(1);ni_solve(pair,solve_player);CHECK(NPI.captured==1);
  uint32_t host_token=NPI.token;
  sim_session[0]=g_s;sim_player[0]=NPI;memcpy(sim_cars[0],cars,sizeof cars);memcpy(sim_info[0],info,sizeof info);
  setup(0);NPI.token=202;g_s.contact_scene.world=202;g_s.contact_scene.peer_world=host_token;
  g_s.peer[0].contact_scene.world=host_token;g_s.peer[0].contact_scene.peer_world=202;
  sim_session[1]=g_s;sim_player[1]=NPI;memcpy(sim_cars[1],cars,sizeof cars);memcpy(sim_info[1],info,sizeof info);
  solves=0;
  /* DATA may live500ms, then a late ACK can wait another500ms for a pose. */
  for(time_now=1000000;time_now<=2100000;time_now+=10000)for(int role=0;role<2;role++){
   g_s=sim_session[role];NPI=sim_player[role];memcpy(cars,sim_cars[role],sizeof cars);memcpy(info,sim_info[role],sizeof info);
   g_sl[0].peer_id=role?100:200;nsent=0;
   for(int k=0;k<nd;){
    if(deliveries[k].role!=role||deliveries[k].at>time_now){k++;continue;}
    rc_msg m=deliveries[k].msg;deliveries[k]=deliveries[--nd];uint8_t bytes[RC_MAX_DGRAM];int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n>0);
    rc_receive(&g_s,0x7f000001,g_s.peer[0].port,bytes,n,time_now);
   }
   for(int k=0;k<3;k++)MRF32(addr(cars[0])+0x60+4*k)+=MRF32(addr(cars[0])+0x70+4*k)*.01f;
   npi_sync_proxy(&cpu,addr(cars[1]));npi_frame(&cpu,time_now);
   rc_state st={0};st.t_us=(uint32_t)time_now;st.q[3]=1;rd3(addr(cars[0])+0x60,st.p);rd3(addr(cars[0])+0x70,st.v);rd3(addr(cars[0])+0x110,st.w);
   rc_push_state(&g_s,&st,time_now);rc_tick(&g_s,time_now);
   for(int k=0;k<nsent;k++){
    if(random32()%100<35)continue;
    int copies=random32()%10==0?2:1;
    while(copies--){CHECK(nd<256);deliveries[nd].msg=sent[k];deliveries[nd].role=!role;deliveries[nd++].at=time_now+10000+random32()%90000;}
   }
   sim_session[role]=g_s;sim_player[role]=NPI;memcpy(sim_cars[role],cars,sizeof cars);memcpy(sim_info[role],info,sizeof info);
  }
  if(sim_player[1].applied>1||sim_player[0].pending[0].active)fprintf(stderr,"run=%d applied=%u pending=%d ack=%lld source=%lld state=%d last=%lld dropped=%u token=%u peer=%u\n",run,sim_player[1].applied,sim_player[0].pending[0].active,(long long)sim_player[0].pending[0].after_ack,(long long)sim_session[0].peer[0].last_snap_rx,sim_session[0].peer[0].state,(long long)sim_session[0].peer[0].last_rx_us,sim_player[0].dropped,sim_player[0].token,sim_player[0].peer);
  CHECK(sim_player[1].applied<=1&&!sim_player[0].pending[0].active);
  if(sim_player[1].applied)applied++;
  if(!sim_player[0].dropped){CHECK(sim_player[1].applied==1);confirmed++;}
 }
 CHECK(applied>450&&confirmed>425);
 printf("player loss:500 sessions/35%% loss, reordering/duplicates; applied %u, confirmed %u, no repeated application\n",applied,confirmed);
}
int main(void){
 CPU cpu={0};setup(1);uint32_t proxy=addr(cars[1]);
 ni_solve(pair,solve_player);CHECK(solves==1&&NPI.captured==1&&rd32(proxy+0x38)==0);
 ni_solve(pair,solve_player);CHECK(solves==2); /* independent sequenced contacts may be in flight */
 rc_player_impulse hit=NPI.pending[0].hit;CHECK(hit.dp[0]==10&&hit.dp[1]==4&&hit.dl[2]==2);
 __typeof__(NPI) host_state=NPI;rc_session host_session=g_s;
 npi_frame(&cpu,time_now);CHECK(nsent==1&&sent[0].h.type==RC_MSG_PLAYER_IMPULSE);
 time_now+=80000;npi_frame(&cpu,time_now);CHECK(nsent==2);
 setup(0);NPI.token=hit.guest_world;g_s.contact_scene.world=NPI.token;
 g_s.peer[0].contact_scene.world=hit.host_world;g_s.peer[0].contact_scene.peer_world=NPI.token;
 g_s.contact_scene.peer_world=hit.host_world;
 receive(&hit,100);npi_frame(&cpu,time_now);CHECK(NPI.applied==1&&refreshes==1&&MRF32(addr(info[0])+0x70)==10&&MRF32(addr(info[0])+0x74)==4);
 CHECK(MRF32(addr(info[0])+0xa0)==5&&MRF32(addr(info[0])+0x88)==2);rc_player_impulse ack=sent[0].u.player_impulse;
 receive(&hit,100);npi_frame(&cpu,time_now);CHECK(NPI.applied==1&&NPI.repeated==1&&refreshes==1);
 ni_solve(pair,solve_player);CHECK(!solves); /* no second client solver */
 setup(1);NPI=host_state;g_s=host_session;receive(&ack,200);npi_frame(&cpu,time_now);CHECK(NPI.pending[0].active==2);
 ni_solve(pair,solve_player);CHECK(solves==1);
 time_now++;g_s.peer[0].last_snap_rx=time_now;g_s.peer[0].mo.s[g_s.peer[0].mo.n-1].t_us=(uint32_t)time_now;
 npi_frame(&cpu,time_now);CHECK(!NPI.pending[0].active);
 setup(1);g_s.peer[0].contact_scene.physics^=1;ni_solve(pair,solve_player);CHECK(!solves&&!NPI.captured);
 setup(1);MRF32(addr(info[1])+0x164)=3;ni_solve(pair,solve_player);CHECK(!solves); /* actual proxy mass mismatch */
 setup(1);time_now+=100001;ni_solve(pair,solve_player);CHECK(!solves); /* proxy sync lease */
 setup(1);uint32_t token=NPI.token;paused=1;npi_frame(&cpu,time_now);CHECK(!NPI.active&&NPI.token!=token);token=NPI.token;
 paused=0;npi_frame(&cpu,++time_now);CHECK(NPI.active&&NPI.token!=token);
 setup(1);token=NPI.token;MRF32(addr(cars[0])+0x60)=100;npi_frame(&cpu,time_now);CHECK(NPI.token!=token);
 setup(1);token=NPI.token;time_now+=300000;npi_frame(&cpu,time_now);CHECK(NPI.token!=token);
 setup(1);token=NPI.token;city=0;NC.bound=1;NC.host=1;NC.peer=200;NC.control.phase=CC_RUNNING;NC.control.run=7;NC.control.event=8;
 npi_frame(&cpu,time_now);CHECK(NPI.token!=token&&NPI.run==7&&NPI.event==8&&NPI.active);
 setup(1);MRF32(addr(cars[1])+0x60)=7;MRF32(addr(cars[1])+0x70)=3;CHECK(npi_sync_proxy(&cpu,addr(cars[1])));
 CHECK(MRF32(addr(info[1])+0x20)==7&&MRF32(addr(info[1])+0x70)==6&&MRF32(addr(info[1])+0xa0)==3);
 MR32(addr(bodies[1])+0x5c)=0;CHECK(npi_sync_proxy(&cpu,addr(cars[1]))); /* kinematic traffic stays solid */
 MR32(addr(bodies[1])+0x5c)=1;MRF32(addr(cars[1])+0x90)=2;CHECK(!npi_sync_proxy(&cpu,addr(cars[1]))&&!npi_contact_ready(200,time_now));
 setup(0);NPI.token=hit.guest_world;g_s.contact_scene.world=NPI.token;g_s.contact_scene.peer_world=hit.host_world;
 g_s.peer[0].contact_scene.world=hit.host_world;g_s.peer[0].contact_scene.peer_world=NPI.token;
 bad_refresh=1;receive(&hit,100);npi_frame(&cpu,time_now);CHECK(!NPI.applied&&refreshes==2&&MRF32(addr(info[0])+0x70)==0&&MRF32(addr(info[0])+0xa0)==0);
 receive(&hit,100);npi_frame(&cpu,time_now);CHECK(NPI.applied==1); /* failed apply not marked consumed */
 setup(0);NPI.token=hit.guest_world;g_s.contact_scene.world=NPI.token;g_s.contact_scene.peer_world=hit.host_world;
 g_s.peer[0].contact_scene.world=hit.host_world;g_s.peer[0].contact_scene.peer_world=NPI.token;
 receive(&hit,100);CHECK(g_s.n_player_impulse==1);MRF32(addr(cars[0])+0x60)=100;npi_frame(&cpu,time_now);CHECK(!NPI.applied);
 setup(1);ni_solve(pair,solve_player);g_s.conf.player_contact=0;npi_frame(&cpu,time_now);CHECK(!NPI.pending[0].active&&!g_s.contact_scene.active);
 setup(1);int64_t frame_start=time_now;time_now+=10;
 CHECK(npi_sync_proxy(&cpu,addr(cars[1])));
 npi_frame(&cpu,frame_start);
 CHECK(NPI.frame_at==time_now&&npi_contact_ready(200,time_now));
 CHECK(NPI.synchronized[0].reason==0);
 setup(1);token=NPI.token;ni_solve(pair,solve_player);CHECK(NPI.pending[0].active==1);
 MR32(addr(cars[0])+0x38)=2;
 CHECK(npi_contact_ready(200,time_now)); /* native state changed between frame hooks */
 MR32(addr(cars[0])+0x38)=17;CHECK(!npi_contact_ready(200,time_now));
 MR32(addr(cars[0])+0x38)=2;time_now+=20;npi_frame(&cpu,time_now-20);
 CHECK(NPI.token==token&&NPI.contact_class==2&&NPI.pending[0].active==1&&nsent==1);
 /* Real clocks advance between frame start, scene publication and send.
  * The earlier fixed-clock mocks hid a permanently refused DATA/ACK path. */
 setup(1);ni_solve(pair,solve_player);clock_step=1;
 npi_frame(&cpu,time_now);clock_step=0;
 CHECK(nsent==1&&sent[0].h.type==RC_MSG_PLAYER_IMPULSE);
 CHECK(time_now>g_s.contact_frame_at);
 CHECK(!rc_send_player_impulse(&g_s,&NPI.pending[0].hit,g_s.contact_frame_at-1));
 setup(0);NPI.token=hit.guest_world;g_s.contact_scene.world=NPI.token;
 g_s.peer[0].contact_scene.world=hit.host_world;g_s.peer[0].contact_scene.peer_world=NPI.token;
 g_s.contact_scene.peer_world=hit.host_world;int64_t before_ack=time_now;
 clock_step=1;npi_ack(100,&hit,before_ack-1);clock_step=0;
 CHECK(nsent==1&&sent[0].u.player_impulse.op==RC_IMPULSE_ACK&&time_now>before_ack);
 setup(0);NPI.token=hit.guest_world;g_s.contact_scene.world=NPI.token;
 g_s.peer[0].contact_scene.world=hit.host_world;g_s.peer[0].contact_scene.peer_world=NPI.token;
 g_s.contact_scene.peer_world=hit.host_world;replace_contact_body=1;
 receive(&hit,100);npi_frame(&cpu,time_now);
 CHECK(!NPI.applied&&!refreshes&&MRF32(addr(info[0])+0x70)==0);
 replace_contact_body=0;
 simulate_loss();
 CHECK(!lock_depth);
 ni_forget_pending();ni_frame(&cpu,time_now); /* disabled traffic path stays inert */
 printf("PASS actual player impulse adapter: %u checks; host-only solver, accumulated native momentum, class restoration, ACK/replay, mass/World/pause/frame/sync gates, native proxy-state setter (mock engine only)\n",checks);return 0;
}
