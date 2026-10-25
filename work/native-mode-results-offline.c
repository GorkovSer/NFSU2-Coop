/* Actual native mode adapter, synthetic memory/calls; no game code runs. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "coop_stream.h"
#include "reward_sync.h"
typedef struct {int unused;} CPU;
static uint8_t flags[2],series[0x3500];
static void *mode_ptr(uint32_t a){
 if(a>=0x85ba64&&a<0x85ba64+sizeof series)return series+a-0x85ba64;
 return a>=0x89e7d8&&a<=0x89e7d9?flags+a-0x89e7d8:(void*)(uintptr_t)a;
}
#define MR32(a) (*(uint32_t*)mode_ptr(a))
#define MR16(a) (*(uint16_t*)mode_ptr(a))
#define MR8(a) (*(uint8_t*)mode_ptr(a))
#define MRF32(a) (*(float*)(uintptr_t)(a))
#define G_CAREER_RACE 0x863470u
#define G_RACE 0x890118u
#define G_FLOW 0x8654a4u
#define FN_CAR_SETPHYS 1u
static unsigned checks;static int64_t now=1000000;static int locked,paused,flushes,boards,finishes,defer_index=-1;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint8_t catalog[2][0x88],board[5][0x20];
static int downhill;static uint32_t attempt;
static uint8_t event[0x88],race[0xc44],car[4][0x9e0],record[4][0x1f0],manager[4][0x9c],style[4][0x54];
static uint32_t address(void *p){return (uint32_t)(uintptr_t)p;}
static int ok(uint32_t p,unsigned n){
 if(p>=0x85ba64&&p+n>=p&&p+n<=0x85ba64+sizeof series)return 1;
 const void *b[]={event,race,car,record,manager,style,catalog,board};unsigned z[]={sizeof event,sizeof race,sizeof car,sizeof record,sizeof manager,sizeof style,sizeof catalog,sizeof board};
 for(int k=0;k<8;k++)if(p>=address((void*)b[k])&&p+n>=p&&p+n<=address((void*)b[k])+z[k])return 1;return 0;
}
static uint32_t live_world=900,native_track=4001;static int native_flow=6,is_url,cancel_key,releases,result_builds;
static uint32_t rd32(uint32_t p){
 if(p==0x85ad40+0x872c)return attempt;
 if(p==0x8383e0)return address(catalog);if(p==0x8383e4)return 2;
 if(p==0x864630)return downhill?address(board[0]):0;
 if(p==0x89e7c4)return downhill?1:3;
 if(p==G_FLOW)return native_flow;if(p==G_RACE)return address(race);
 if(p==G_CAREER_RACE)return address(event);if(p==0x89e7a0)return native_track;
 if(p>=0x89e7a4&&p<=0x89e7bc)return p==0x89e7bc?2:0;
 return ok(p,4)?MR32(p):0;
}
static uint32_t world(void){return live_world;}
static int nc_event_mode(uint32_t e){return is_url?RM_URL:MR8(e+0xf);}
static int nc_same_world(void);
static int nc_pause_held(void){return paused;}
static void roam_plat_lock(void){CHECK(!locked);locked=1;}
static void roam_plat_unlock(void){CHECK(locked);locked=0;}
static int64_t roam_plat_now_us(void){return now;}
static void rlog(const char *s,...){(void)s;}
static uint32_t nr_profile(void){return 123;}
static rw_reward NR_REWARD;static rc_session g_s;
static struct {int valid,finalized;uint32_t run;} NV_FROZEN;
static struct {int enabled,bound,host,result_applied,loaded_noted;int64_t next_send;uint16_t seq;int32_t clock_offset;uint32_t race,world,peer,next_run,car[6],record[6],brain[6],slot[6],old_phys[6];cc_race control;cs_stream stream;} NC;
static void nv_race_release(CPU *c){(void)c;CHECK(!NC.bound);releases++;NV_FROZEN.valid=NV_FROZEN.finalized=0;}
static void body_set(CPU *c,uint32_t car,int on){(void)c;CHECK(!NC.bound&&ok(car,0x9e0));if(on)CHECK(MR32(car+0x28));}
static uint32_t gcall1(CPU *c,uint32_t fn,uint32_t car,uint32_t mode){(void)c;CHECK(!NC.bound&&fn==FN_CAR_SETPHYS);MR32(car+0x28)=mode;return 0;}
int nc_cancel_key(void){return cancel_key;}
static void nc_unbind(CPU *);
static int nc_same_world(void){return NC.bound&&NC.race==address(race)&&NC.world==world();}
static void nc_original_finish(void){}
static void rh_call(CPU *c,uint32_t fn,uint32_t a,uint32_t b,unsigned n,uint32_t *args){
 (void)c;(void)a;(void)b;CHECK(!locked&&fn==(uint32_t)(uintptr_t)nc_original_finish&&n==5);
 CHECK(args[0]==NC.race&&args[1]<4);finishes++;if((int)args[1]==defer_index)return;
 uint32_t rec=MR32(NC.race+0xbd0+args[1]*4);MR8(rec+0xc)=(uint8_t)args[2];MR16(rec+0x10)=(uint16_t)args[4];
}
void ni_style_finalize(uint32_t m){
 CHECK(!locked&&ok(m,0x9c));flushes++;uint32_t st=MR32(m);MRF32(st+0x2c)+=MRF32(st+0x24);MRF32(st+0x24)=0;MR32(st+0x14)=3;
}
void ni_style_board(void){CHECK(!locked);boards++;}
void ni_race_results(uint32_t r){CHECK(r==NC.race);for(int k=0;k<NC.stream.racers;k++)CHECK(MR16(NC.record[k]+0x10));result_builds++;}
#include "native_race_modes.inc"
#ifdef MODE_GATE_TEST
static unsigned credits;
static int nr_reward_result(int won){
 CHECK(rw_authorize(&NR_REWARD,won)==1);uint32_t after;
 int n=rw_credit(&NR_REWARD,123,500,&after);CHECK(n>=0);if(n){credits++;CHECK(after==500+(won?2500:0));}return NR_REWARD.paid;
}
#include "native-mode-gate-extracted.inc"
#endif
static void nc_unbind(CPU *c){NC.bound=0;nv_race_release(c);nm_reset();memset(&NR_REWARD,0,sizeof NR_REWARD);}
static rm_packet last;static void sent(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)u;(void)ip;(void)port;rc_msg m;CHECK(rc_decode(b,n,&m)==RC_OK);if(m.h.type==RC_MSG_OUTCOME)last=m.u.outcome;
}
static void setup(int mode,int host){
 memset(&g_s,0,sizeof g_s);memset(&NC,0,sizeof NC);memset(&NR_REWARD,0,sizeof NR_REWARD);nm_reset();
 memset(event,0,sizeof event);memset(race,0,sizeof race);memset(record,0,sizeof record);memset(manager,0,sizeof manager);memset(style,0,sizeof style);
 downhill=0;attempt=0;memset(catalog,0,sizeof catalog);memset(board,0,sizeof board);
 now=1000000;locked=paused=flushes=boards=finishes=cancel_key=releases=result_builds=0;live_world=900;native_flow=6;native_track=4001;is_url=mode==RM_URL;memset(series,0,sizeof series);NV_FROZEN.valid=1;NV_FROZEN.finalized=1;NV_FROZEN.run=77;MR8(address(event)+0xf)=(uint8_t)mode;MR32(address(event)+8)=88;
 NC.enabled=NC.bound=1;NC.host=host;NC.peer=host?200:100;NC.world=world();NC.race=address(race);NC.stream.racers=4;MR32(NC.race+0x24)=4;
 cc_init(&NC.control,host?100:200,NC.peer,host);NC.control.phase=CC_LOADING;NC.control.run=77;NC.control.event=88;
 for(int k=0;k<4;k++){NC.car[k]=address(car[k]);NC.record[k]=address(record[k]);MR32(NC.race+0xbd0+4*k)=NC.record[k];MR32(NC.record[k])=NC.car[k];NC.old_phys[k]=3;MR16(NC.car[k]+0xa)=(uint16_t)(k==(host?0:1)?0:k<2?100:99+k);MR32(NC.record[k]+0x1d4)=address(manager[k]);MR32(address(manager[k]))=address(style[k]);MRF32(address(style[k])+0x2c)=(float)(1000+k*1000);}
 g_s.conf.self_id=host?100:200;g_s.conf.coop=1;g_s.conf.extensions=RC_EXT_RACE_MODES|RC_EXT_RANK_TABLE|RC_EXT_RANK_CLOCK|RC_EXT_PAUSE_CLOCK|(host?RC_EXT_CONTACT_HOST:0);g_s.conf.extension_nonce=host?111:222;g_s.io.send=sent;
 rc_peer *p=&g_s.peer[0];p->id=NC.peer;p->state=RC_PS_EST;p->coop=1;p->core_ver=4;p->extension_seen=1;p->extension_at=now;p->extensions.nonce=host?222:111;p->extensions.echo=g_s.conf.extension_nonce;p->extensions.features=RC_EXT_RACE_MODES|RC_EXT_RANK_TABLE|RC_EXT_RANK_CLOCK|RC_EXT_PAUSE_CLOCK|(host?0:RC_EXT_CONTACT_HOST);
 if(is_url){MR8(address(event)+0x7e)=3;MR32(0x85ba64+0x34e8)=3;MR32(0x85ba64+0x34e4)=0x85ba64+0x54;MR32(0x85ba64+0x54)=native_track;MR32(0x85ba64+0x54+8)=2;}
 NR_REWARD.initialized=1;NR_REWARD.profile=123;NR_REWARD.run=77;NR_REWARD.event=88;
}
static rm_packet packet(int op,uint32_t serial){
 rm_packet p={0};p.op=(uint8_t)op;p.mode=(uint8_t)NM.outcome.mode;p.round=(uint8_t)NM.outcome.round;p.rounds=(uint8_t)NM.outcome.rounds;p.racers=(uint8_t)NM.outcome.racers;p.run=NM.outcome.run;p.event=88;p.layout=NM.outcome.layout;p.serial=serial;p.to=g_s.conf.self_id;p.source=NM.destination;p.destination=NM.source;return p;
}
static void enqueue(rm_packet p){
 if(p.final)for(int k=0;k<p.racers;k++)p.elapsed[k]=p.car[k].wall;
 /* Delivery cannot precede the sender's finish on the common epoch. */
 if(NC.host&&NC.control.release_at)for(int k=0;k<p.racers;k++){
  int64_t arrival=NC.control.release_at+(int64_t)p.car[k].wall*250;
  if(arrival>now)now=arrival;
 }
 g_s.peer[0].extension_at=now;
 CHECK(g_s.n_outcome<RM_INBOX);g_s.outcome_in[g_s.n_outcome].from=NC.peer;g_s.outcome_in[g_s.n_outcome++].message=p;
}
static void next_world(int round){
 live_world++;for(int k=0;k<4;k++)MR16(address(record[k])+0x10)=0;native_track=4001u+(uint32_t)round;uint32_t stage=0x85ba64+0x54+0xd24*(uint32_t)round;
 MR32(0x85ba64+0x34e4)=stage;MR32(stage)=native_track;MR32(stage+4)=0;MR32(stage+8)=2;MR32(address(race))=3;
}
static void rebind(void){
 NR_REWARD.initialized=1;NR_REWARD.run=NC.control.run;NR_REWARD.event=88;NR_REWARD.profile=123;
 NC.bound=1;NC.world=world();NC.race=address(race);NC.stream.racers=4;MR32(NC.race+0x24)=4;
 for(int k=0;k<4;k++){
  NC.car[k]=address(car[k]);NC.record[k]=address(record[k]);NC.old_phys[k]=3;
  memset(record[k],0,sizeof record[k]);MR32(NC.record[k])=NC.car[k];MR32(NC.race+0xbd0+4*k)=NC.record[k];
  MR32(NC.record[k]+0x1d4)=address(manager[k]);MR32(address(manager[k]))=address(style[k]);
  MR16(NC.car[k]+0xa)=(uint16_t)(k==(NC.host?0:1)?0:k<2?100:99+k);
 }
}
static int fixture_finish(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t);
static void url_tests(CPU *c){
 setup(RM_URL,1);
 for(int round=0;round<3;round++){
  CHECK(!nm_frame(c,now));CHECK(NM.outcome.round==round&&NV_FROZEN.valid);
  enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(c,now));NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=100000;
  CHECK(fixture_finish(NC.race,0,1,0,RM_FINISH));MR32(NC.race+0x14)=120000;CHECK(fixture_finish(NC.race,2,3,0,RM_FINISH));MR32(NC.race+0x14)=130000;CHECK(fixture_finish(NC.race,3,4,0,RM_FINISH));
  rm_packet owner=packet(RM_OWNER,2);owner.car[1]=(rm_value){110000,0,RM_FINISH,110000};enqueue(owner);CHECK(nm_frame(c,now));
  CHECK(NM.outcome.final&&NM.outcome.rounds_committed==round+1&&!NM.applied);
  enqueue(packet(RM_BOARD_ACK,3));CHECK(nm_frame(c,now));
  if(round<2){
   CHECK(NC.control.phase==CC_RUNNING&&NM.applied&&!NC.bound&&NV_FROZEN.valid&&!NR_REWARD.authorized);
   CHECK(finishes==4*(round+1)&&result_builds==round+1&&releases==round+1);
   CHECK(nm_can_continue(0x85ba64)==1&&nm_can_continue(0x12345678)==-1);
   CHECK(nm_between(c,now));native_flow=4;CHECK(nm_between(c,now));native_flow=6;
   next_world(round+1);if(!round)live_world--;CHECK(!nm_between(c,now));CHECK(NC.control.phase==CC_LOADING&&NC.control.run==(uint32_t)(78+round));
   CHECK(NM.outcome.round==round+1&&!NM.outcome.final&&NV_FROZEN.run==NC.control.run&&!NV_FROZEN.finalized);
   CHECK(NM.outcome.points[0]==(uint32_t)(8*(round+1)));rebind();
  }else{
   CHECK(NC.control.phase==CC_DONE&&NC.control.winner==g_s.conf.self_id&&NM.outcome.points[0]==24);
   CHECK(!NM.applied&&nm_result_ready());NR_REWARD.authorized=1;CHECK(nm_apply_result(c)==1);
   CHECK(!NC.bound&&!NV_FROZEN.valid&&result_builds==3&&finishes==12&&nm_career_win(0x85ba64)==1&&!nm_can_continue(0x85ba64));
   CHECK(!nm_between(c,now));
  }
 }
 setup(RM_URL,0);CHECK(!nm_frame(c,now));enqueue(packet(RM_TERMS,1));CHECK(nm_frame(c,now));NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=110000;
 CHECK(fixture_finish(NC.race,1,2,0,RM_FINISH));rm_packet final=packet(RM_BOARD,2);final.final=1;
 for(int k=0;k<4;k++)final.car[k]=(rm_value){(uint32_t)(100000+k*10000),0,RM_FINISH,(uint32_t)(100000+k*10000)};
 enqueue(final);CHECK(nm_frame(c,now));CHECK(NM.applied&&!NC.bound&&NM.old_ack_valid);
 CHECK(nm_between(c,now)&&last.op==RM_BOARD_ACK&&last.run==77);
 next_world(1);CHECK(!nm_between(c,now));CHECK(NM.old_ack_valid&&NM.old_ack.run==77&&NM.outcome.run==78);rebind();
 enqueue(final);CHECK(!nm_frame(c,now)&&!NM.outcome.fault&&NM.old_ack_valid);
 enqueue(packet(RM_TERMS,1));CHECK(nm_frame(c,now)&&!NM.old_ack_valid);
 MR16(NC.car[2]+0xa)=105;CHECK(nm_frame(c,now)==-1);
 setup(RM_URL,1);CHECK(!nm_frame(c,now));MR32(0x85ba64+0x34e8)=4;CHECK(nm_frame(c,now)==-1);
 /* An interrupted stage wait cancels the active series and releases CFG. */
 setup(RM_URL,1);CHECK(!nm_frame(c,now));NM.applied=1;NM.between_since=now;NC.bound=0;NC.control.phase=CC_RUNNING;
 cancel_key=1;CHECK(!nm_between(c,now)&&!NM.active&&!NV_FROZEN.valid&&NC.control.phase==CC_CANCELLED);
}
static void downhill_tests(CPU *c){
 for(int host=0;host<2;host++){
  setup(RM_DRIFT,host);downhill=1;native_track=0x104e;MR16(address(event)+0x18)=0x104e;NC.stream.racers=2;MR32(NC.race+0x24)=2;
  catalog[0][0xf]=5;catalog[0][0x7c]=1;catalog[0][0x42]=2;catalog[0][0x40]=7;catalog[0][0x44]=99;
  uint8_t original[0x88];memcpy(original,event,0x88);
  CHECK(nm_prepare_event(address(event),77));attempt++;CHECK(!memcmp(original,event,0x88));
  uint32_t copy=nm_downhill_event(address(event));CHECK(copy==address(NMD.copy)&&MR8(copy+0x7c)==1&&MR8(copy+0x40)==7&&MR8(address(event)+0x7c)==0);
  CHECK(nm_downhill_grid());attempt++;CHECK(nm_downhill_event(address(event))==address(event)&&!nm_downhill_grid());attempt--;
  for(int k=0;k<5;k++){
   MR32(address(board[k]))=k==4?0x864630:address(board[k+1]);MR32(address(board[k])+8)=k==0?0:k==1?100:99+k;
   MRF32(address(board[k])+0x10)=k<2?0.f:(float)(k*1000);
  }
  CHECK(!nm_frame(c,now)&&NM.active&&NM.downhill&&NM.outcome.racers==5);
  enqueue(packet(host?RM_TERMS_ACK:RM_TERMS,1));CHECK(nm_frame(c,now));NC.control.phase=CC_RUNNING;
  MR32(NC.race+0x14)=100000;MRF32(address(style[host?0:1])+0x2c)=host?3500.f:5000.f;
  CHECK(nm_frame(c,now));CHECK(!NM.finishing); /* virtual opponents do not start finish deadline */
  CHECK(fixture_finish(NC.race,host?0:1,1,0,RM_FINISH));
  if(host){
   rm_packet owner=packet(RM_OWNER,2);owner.car[1]=(rm_value){100000,5000,RM_FINISH,100000};enqueue(owner);CHECK(nm_frame(c,now)&&NM.outcome.final);
   CHECK(NM.outcome.car[2].score==2000&&NM.outcome.car[3].score==3000&&NM.outcome.car[4].score==4000);
   enqueue(packet(RM_BOARD_ACK,3));CHECK(nm_frame(c,now)&&NC.control.phase==CC_DONE&&NC.control.winner==NC.peer);
  }else{
   rm_packet final=packet(RM_BOARD,2);final.final=1;
   final.car[0]=(rm_value){100000,3500,RM_FINISH,100000};final.car[1]=(rm_value){100000,5000,RM_FINISH,100000};
   for(int k=2;k<5;k++)final.car[k]=(rm_value){0,(uint32_t)k*1000,RM_FINISH,0};enqueue(final);CHECK(nm_frame(c,now)&&NM.outcome.final);
   NC.control.phase=CC_DONE;NC.control.winner=g_s.conf.self_id;
  }
  CHECK(nm_result_ready());NR_REWARD.authorized=1;CHECK(nm_apply_result(c)==1&&finishes==1&&MR8(NC.record[host?0:1]+0xc)==1);
  CHECK(MRF32(address(board[4])+0x10)==4000&&MRF32(address(board[2])+0x10)==2000);
  CHECK(nm_career_win(0x85ba64)==1); /* teammate beats preserved table score */
  nm_reset();NV_FROZEN.valid=0;CHECK(nm_downhill_event(address(event))==copy); /* in-flight native attempt remains consistent */
  attempt++;CHECK(nm_downhill_event(address(event))==address(event));attempt--;NM.active=1;NM.downhill=1;
  MR32(address(board[4])+8)=100;uint32_t entries[3];CHECK(!nmd_entries(entries));
 }
 setup(RM_DRIFT,1);native_track=0x104e;MR16(address(event)+0x18)=0x104e;CHECK(!nm_prepare_event(address(event),77));
}
/* Legacy fixtures use a no-pause clock; dedicated clock tests inject drift. */
static int fixture_finish(uint32_t race,uint32_t index,uint32_t position,uint32_t flag,uint32_t completion){
 if(!NC.control.release_at)NC.control.release_at=1000000;
 uint32_t wall=MR32(race+0x14);now=NC.control.release_at+(int64_t)wall*250;
 g_s.peer[0].extension_at=now;
 if(NC.host){if(!NM.clock.active)CHECK(rk_step(&NM.clock,0,0));CHECK(rk_step(&NM.clock,wall,paused));}
 return nm_finish(race,index,position,flag,completion);
}
static void ranked_tests(CPU *c){
 setup(RM_RANK,1);g_s.peer[0].extension_at=now;
 CHECK(nm_frame(c,now-500000)==0&&NM.active&&!NM.outcome.fault);
 setup(RM_RANK,1);g_s.peer[0].extension_at=now-1500001;
 CHECK(nm_frame(c,now)==0&&!NM.active);
 g_s.peer[0].extension_at=now;CHECK(nm_frame(c,now)==0&&NM.active);
 for(int host=0;host<2;host++){
  setup(RM_RANK,host);MR8(address(event)+0xf)=0;CHECK(nm_mode(address(event))==RM_RANK);
  CHECK(!nm_frame(c,now));enqueue(packet(host?RM_TERMS_ACK:RM_TERMS,1));CHECK(nm_frame(c,now));
  NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=host?100000:110000;
  CHECK(fixture_finish(NC.race,host?0:1,host?1:2,0,RM_FINISH));
  CHECK(!finishes&&!NM.outcome.final); /* first human must not finish the other */
  if(host){
   MR32(NC.race+0x14)=120000;CHECK(fixture_finish(NC.race,2,3,0,RM_FINISH));
   MR32(NC.race+0x14)=140000;CHECK(fixture_finish(NC.race,3,4,0,RM_FINISH));
   CHECK(nm_frame(c,now)&&!NM.outcome.final&&!finishes);
   rm_packet owner=packet(RM_OWNER,2);owner.car[1]=(rm_value){110000,0,RM_FINISH,110000};enqueue(owner);
   CHECK(nm_frame(c,now)&&NM.outcome.final&&!finishes);
   enqueue(packet(RM_BOARD_ACK,3));CHECK(nm_frame(c,now)&&NC.control.phase==CC_DONE);
  }else{
   rm_packet final=packet(RM_BOARD,2);final.final=1;
   for(int k=0;k<4;k++)final.car[k]=(rm_value){(uint32_t)(k==3?140000:100000+k*10000),0,RM_FINISH,(uint32_t)(k==3?140000:100000+k*10000)};
   enqueue(final);CHECK(nm_frame(c,now)&&NM.outcome.final&&!finishes);
   NC.control.phase=CC_DONE;NC.control.winner=NC.peer;
  }
  NR_REWARD.authorized=1;CHECK(nm_result_ready()&&nm_apply_result(c)==1);
  CHECK(finishes==4&&result_builds==1&&NM.applied&&nm_career_win(0x85ba64)==1);
  for(int k=0;k<4;k++){
   CHECK(MR8(NC.record[k]+0xa)==k+1&&MR8(NC.record[k]+0xc)==k+1);
   CHECK(MR16(NC.record[k]+0x10)==RM_FINISH);
   CHECK(MR32(NC.record[k]+0x198)==(uint32_t)(k==3?140000:100000+k*10000));
  }
  CHECK(nm_apply_result(c)==1&&finishes==4);
 }
 setup(RM_RANK,1);g_s.peer[0].extensions.features&=~RC_EXT_RANK_TABLE;CHECK(nm_frame(c,now)==-1);
 /* No AI crosses the finish: the second real human completes the field.
  * Actual adapter must close remaining bots, agree the board and only then
  * run native completion, without a one-minute bot timeout. */
 setup(RM_RANK,1);CHECK(!nm_frame(c,now));enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(c,now));
 NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=10000;
 CHECK(fixture_finish(NC.race,0,1,0,RM_FINISH));
 rm_packet last_human=packet(RM_OWNER,2);last_human.car[1]=(rm_value){15000,0,RM_FINISH,16000};enqueue(last_human);
 MR32(NC.race+0x14)=20000;now=NC.control.release_at+20000LL*250;g_s.peer[0].extension_at=now;
 CHECK(nm_frame(c,now)&&NM.outcome.final&&!finishes);
 CHECK(NM.outcome.car[2].status==RM_TIMEOUT&&NM.outcome.car[3].status==RM_TIMEOUT);
 CHECK(NM.outcome.rank[0]==1&&NM.outcome.rank[1]==2);
 enqueue(packet(RM_BOARD_ACK,3));CHECK(nm_frame(c,now)&&NC.control.phase==CC_DONE);
 NR_REWARD.authorized=1;CHECK(nm_apply_result(c)==1&&finishes==4);
 setup(RM_RANK,1);g_s.peer[0].extensions.features&=~RC_EXT_PAUSE_CLOCK;
 uint32_t old_nonce=0;CHECK(!nm_peer(NC.peer,now,&old_nonce)&&!old_nonce);
 puts("PASS ranked native board: late human finish retained, host/guest full identical native ranks/times, one commit and capability refusal");
}
static void shared_clock_native_test(CPU *c){
 for(int mode=RM_DRAG;mode<=RM_RANK;mode++){
  setup(mode,1);CHECK(!nm_frame(c,now));enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(c,now));
  NC.control.phase=CC_RUNNING;NC.control.release_at=now;CHECK(nm_frame(c,now));
  paused=1;now=NC.control.release_at+40000LL*250;g_s.peer[0].extension_at=now;CHECK(nm_frame(c,now));
  paused=0;now=NC.control.release_at+60000LL*250;g_s.peer[0].extension_at=now;CHECK(nm_frame(c,now));
  for(int k=0;k<4;k++)if(k!=1){
   uint32_t wall=120000u+k*10000u;now=NC.control.release_at+(int64_t)wall*250;
   g_s.peer[0].extension_at=now;MR32(NC.race+0x14)=100000u+k*10000u;
   CHECK(nm_finish(NC.race,k,k+1,0,RM_FINISH));
  }
  rm_packet p=packet(RM_OWNER,2);p.car[1]=(rm_value){90000,mode==RM_DRIFT?1000u:0u,RM_FINISH,124000};enqueue(p);
  CHECK(nm_frame(c,now)&&NM.outcome.final);
  CHECK(NM.outcome.elapsed[0]==100000&&NM.outcome.elapsed[1]==104000);
  if(mode!=RM_DRIFT)CHECK(NM.outcome.rank[0]==1&&NM.outcome.rank[1]==2);
  CHECK(NM.outcome.car[1].ticks==90000&&NM.outcome.car[1].wall==124000);
 }
 puts("PASS actual shared clock adapter: one pause ledger for Drag/Drift/ranked, inverted native owner timers retained only as diagnostics");
}
int main(void){

 CPU c={0};
 ranked_tests(&c);
 shared_clock_native_test(&c);
 for(int mode=RM_DRAG;mode<=RM_DRIFT;mode++){
  setup(mode,1);CHECK(nm_frame(&c,now)==0);CHECK(!NM.outcome.peer_agreed&&!nm_result_ready());
  enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(&c,now)==1);NC.control.phase=CC_RUNNING;
  MR32(NC.race+0x14)=100000;
  CHECK(fixture_finish(NC.race,0,1,0,RM_FINISH));CHECK(NM.outcome.car[0].status==RM_FINISH);
  CHECK(fixture_finish(NC.race,0,1,0,RM_FINISH));CHECK(flushes==(mode==RM_DRIFT?1:0));
  CHECK(fixture_finish(NC.race,2,3,0,RM_BLOWN));CHECK(fixture_finish(NC.race,3,4,0,RM_TOTALLED));
  CHECK(fixture_finish(NC.race,1,2,0,RM_FINISH));CHECK(!NM.outcome.car[1].status); /* remote simulation cannot finish its owner */
  rm_packet owner=packet(RM_OWNER,2);owner.car[1]=(rm_value){110000,mode==RM_DRIFT?5000u:0u,RM_FINISH,110000};enqueue(owner);
  CHECK(nm_frame(&c,now)==1&&NM.outcome.final&&NC.control.phase==CC_RUNNING);CHECK(last.op==RM_BOARD&&last.final);
  if(mode==RM_DRIFT)CHECK(MRF32(address(style[1])+0x2c)==5000);
  rm_packet final=last;
  enqueue(packet(RM_BOARD_ACK,3));CHECK(nm_frame(&c,now)==1&&NC.control.phase==CC_DONE);
  CHECK(NC.control.winner==(mode==RM_DRIFT?NC.peer:g_s.conf.self_id));
  CHECK(nm_result_ready());CHECK(nm_career_win(0x85ad40+0xd24)==-1);NR_REWARD.authorized=1;
  CHECK(nm_apply_result(&c)==1&&finishes==1&&MR8(NC.record[0]+0xc)==1);
  CHECK(nm_apply_result(&c)==1&&finishes==1);CHECK(nm_career_win(0x85ad40+0xd24)==1);CHECK(nm_career_win(0x12345678)==-1);
  if(mode==RM_DRIFT)CHECK(boards>=2&&MRF32(address(style[1])+0x2c)==5000);
  /* Same final board on the real guest, before the co-op result arrives. */
  setup(mode,0);CHECK(nm_frame(&c,now)==0);enqueue(packet(RM_TERMS,1));CHECK(nm_frame(&c,now)==1);NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=110000;
  MRF32(address(style[1])+0x2c)=5000;CHECK(fixture_finish(NC.race,1,2,0,RM_FINISH));
  final.to=g_s.conf.self_id;final.source=NM.destination;final.destination=NM.source;final.serial=2;enqueue(final);CHECK(nm_frame(&c,now)==1&&NM.outcome.final);
  NC.control.phase=CC_DONE;NC.control.winner=mode==RM_DRIFT?g_s.conf.self_id:NC.peer;NR_REWARD.authorized=1;
  CHECK(nm_apply_result(&c)==1&&MR8(NC.record[1]+0xc)==1&&finishes==1);
 }
 setup(RM_DRAG,1);CHECK(!nm_frame(&c,now));enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(&c,now));NC.control.phase=CC_RUNNING;
 MR32(NC.race+0x14)=1000;CHECK(fixture_finish(NC.race,0,1,0,RM_FINISH));CHECK(nm_frame(&c,now));
 paused=1;MR32(NC.race+0x14)=250000;CHECK(nm_frame(&c,now));CHECK(!NM.outcome.car[2].status);
 paused=0;CHECK(nm_frame(&c,now));CHECK(NM.outcome.car[2].status==RM_TIMEOUT&&!NM.outcome.car[1].status);
 setup(RM_DRAG,1);CHECK(!nm_frame(&c,now));enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(&c,now));NC.control.phase=CC_RUNNING;
 CHECK(fixture_finish(NC.race,0,1,0,7)&&NC.control.phase==CC_CANCELLED);
 setup(RM_DRIFT,1);CHECK(!nm_frame(&c,now));enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(&c,now));g_s.peer[0].extensions.nonce++;CHECK(nm_frame(&c,now)==-1);
 setup(RM_DRIFT,1);CHECK(!nm_frame(&c,now));MRF32(address(style[0])+0x2c)=rc_bitsf(0x7fc00000);enqueue(packet(RM_TERMS_ACK,1));CHECK(nm_frame(&c,now));NC.control.phase=CC_RUNNING;CHECK(nm_frame(&c,now)==-1);
 /* Deferred local finish and partially committed opponent roster. */
 for(int ranked=0;ranked<2;ranked++){
  setup(ranked?RM_RANK:RM_DRAG,1);NM.active=1;NM.outcome.mode=ranked?RM_RANK:RM_DRAG;
  NM.outcome.final=1;NM.outcome.racers=4;
  for(int k=0;k<4;k++){NM.outcome.rank[k]=k+1;NM.outcome.car[k].status=RM_FINISH;NM.outcome.elapsed[k]=100+k;}
  NC.control.phase=CC_DONE;NC.control.winner=g_s.conf.self_id;defer_index=ranked?2:0;
  CHECK(nm_apply_result(&c)==0&&!NM.applied&&!NM.outcome.fault);
  int first_calls=finishes;CHECK(nm_apply_result(&c)==0&&finishes==first_calls+1&&!NM.applied);
  defer_index=-1;CHECK(nm_apply_result(&c)==1&&NM.applied);
  first_calls=finishes;CHECK(nm_apply_result(&c)==1&&finishes==first_calls);
 }
 url_tests(&c);downhill_tests(&c);
#ifdef MODE_GATE_TEST
 for(int mode=RM_DRAG;mode<=RM_DRIFT;mode++){
  setup(mode,0);credits=0;CHECK(!nm_frame(&c,now));enqueue(packet(RM_TERMS,1));CHECK(nm_frame(&c,now));
  rw_init(&NR_REWARD,200,100,77,88,123,0);NR_REWARD.terms=1;NR_REWARD.amount=2500;
  NC.control.phase=CC_RUNNING;MR32(NC.race+0x14)=110000;
  CHECK(fixture_finish(NC.race,1,2,0,RM_FINISH));
  /* Control/result overtakes the final mode board. No native finish or credit. */
  NC.control.phase=CC_DONE;NC.control.winner=NC.peer;nc_apply_result(&c);
  CHECK(!credits&&!finishes&&!NC.result_applied&&!NR_REWARD.authorized);
  rm_packet final=packet(RM_BOARD,2);final.final=1;
  for(int k=0;k<4;k++)final.car[k]=(rm_value){100000u+k*10000u,mode==RM_DRIFT?10000u-k*1000u:0,RM_FINISH,100000u+k*10000u};
  final.car[1].score=NM.outcome.car[1].score;
  enqueue(final);nc_apply_result(&c);
  CHECK(credits==1&&finishes==1&&NC.result_applied&&NR_REWARD.paid);
  for(int k=0;k<100;k++)nc_apply_result(&c);
  CHECK(credits==1&&finishes==1);
 }
 puts("PASS actual generated nc_apply_result: reordered final-board/control cannot pay early; one native finish and one credit despite 100 repeated deliveries");
#endif
 printf("PASS actual native mode results: %u checks; mode barrier, native score flush, immutable owner finish, proxy rejection, board/ACK, both profiles' result, scoped career outcome, pause-aware timeout and nonce/score refusal (mock engine only)\n",checks);return 0;
}
