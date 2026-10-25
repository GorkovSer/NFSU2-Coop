/* Local-only scripted guest. No second game or human is simulated by proof.
 * Mirrors host motion at +4m; tests engine integration, not actual co-op play. */
#include <stdlib.h>
#define main old_free_roam_main
#include "native-fake-peer.c"
#undef main
#include "coop_race.h"
int main(int argc,char **argv){
 int seconds=argc>1?atoi(argv[1]):600;
 static rc_session s;rc_config cfg={0};cfg.self_id=0x54455333;cfg.port=48761;cfg.room=rc_fnv1a("career-development");
 strcpy(cfg.name,"CoopTestGuest");cfg.platform=RC_PLAT_TOOL;cfg.test_mode=1;cfg.max_players=2;cfg.rate_hz=20;cfg.coop=1;
 rc_io io={0};rc_session_init(&s,&cfg,&io);roam_plat_cfg pc={48761,1,0,NULL};if(roam_plat_start(&pc,&s))return 1;
 rc_add_peer(&s,0x7f000001,48760,1,roam_plat_now_us());
 cc_race race;cc_init(&race,cfg.self_id,0,0);rc_race_frame host_frame={0};
 uint16_t car_rev=0,seq=0,echoed_seq=0;int echoed_valid=0,previous=-1;int64_t begin=roam_plat_now_us(),next_log=begin,next_frame=0;
 while(roam_plat_now_us()-begin<(int64_t)seconds*1000000){
  roam_win_poll();int64_t now=roam_plat_now_us();int connected=0,clock_valid=0;int32_t offset=0;
  for(int i=0;i<RC_TABLE;i++){
   rc_peer *p=&s.peer[i];if(p->state!=RC_PS_EST||p->lost||!p->coop)continue;
   connected=1;race.peer=p->id;clock_valid=p->clock_valid&&now-p->clock_sample_at<10000000;offset=p->clock_offset_us;
   if(p->cfg_len&&p->cfg_rev!=car_rev){rc_set_cfg(&s,p->cfg,p->cfg_len,now);car_rev=p->cfg_rev;}
   if(race.phase<CC_LOADING||race.phase>CC_RUNNING){
    if(p->mo.n&&(p->status==RC_ST_ROAM||p->status==RC_ST_PAUSED)){
     rc_state st=p->mo.s[p->mo.n-1];st.t_us=(uint32_t)now;st.p[0]+=4;rc_set_status(&s,RC_ST_ROAM);rc_push_state(&s,&st,now);
    }else rc_set_status(&s,RC_ST_MENUS);
   }
  }
  uint32_t from;rc_coop_msg msg;
  while(rc_take_coop(&s,&from,&msg))if(msg.op!=RC_COOP_GO||clock_valid)cc_receive(&race,from,&msg,now,offset);
  uint32_t actions=cc_take_actions(&race);
  if(actions&CC_INVITATION){memset(&host_frame,0,sizeof host_frame);seq=0;echoed_valid=0;next_frame=0;cc_accept(&race,race.event,now);}
  if(actions&CC_LOAD_GAME)rc_set_status(&s,RC_ST_LOADING);
  rc_race_frame incoming;
  if(rc_take_race(&s,race.peer,&incoming)&&incoming.run==race.run&&incoming.event==race.event){host_frame=incoming;if(race.phase==CC_LOADING)cc_loaded(&race,now);}
  cc_step(&race,now,connected);
  if(clock_valid&&host_frame.run==race.run&&race.phase>=CC_LOADING&&race.phase<=CC_RUNNING&&now>=next_frame&&
     (!echoed_valid||host_frame.seq!=echoed_seq)){
   rc_race_frame out={0};out.to=race.peer;out.run=race.run;out.event=race.event;out.racers=host_frame.racers;out.n=1;out.seq=seq++;
   int found=0;for(int i=0;i<host_frame.n;i++)if(host_frame.car[i].slot==0){out.car[0].state=host_frame.car[i].state;found=1;}
   /* This is a delayed host sample, not a measurement taken by a second
    * game now. Retain its sampling time, mapped into our QPC clock domain.
    * Restamping a held position with 'now' made Hermite tangents disagree
    * with position deltas, causing periodic backwards motion and lurches. */
   out.car[0].slot=1;out.car[0].state.t_us+=(uint32_t)offset;out.car[0].state.p[0]+=4;
   if(found){rc_send_race(&s,&out);echoed_seq=host_frame.seq;echoed_valid=1;}
   rc_set_status(&s,RC_ST_EVENT);next_frame=now+50000;
  }
  if(cc_next_packet(&race,now,&msg))rc_send_coop(&s,&msg);
  if(previous!=race.phase){printf("guest phase=%d run=%08x event=%08x reason=%u\n",race.phase,race.run,race.event,race.cancel_reason);fflush(stdout);previous=race.phase;}
  if(now>=next_log){printf("peer=%d clock=%d rtt/offset=%d tx=%u rx=%u bad=%u phase=%d roster=%u\n",connected,clock_valid,offset,s.tx,s.rx,s.rx_bad,race.phase,host_frame.racers);fflush(stdout);next_log=now+5000000;}
  Sleep(5);
 }
 roam_plat_stop(0);return 0;
}
