/* Loopback-only protocol peer. Mirrors the real game's transmitted car with
 * a lateral offset, exercising actual receive/load/update paths. No input/UI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "roam_core.h"
#include "roam_plat.h"
void roam_win_poll(void);
const char *roam_opt(const char *n){
 if(!strcmp(n,"NFSU2_ROAM_NETTHREAD"))return "0";
 if(!strcmp(n,"NFSU2_ROAM_HARNESS"))return "1";
 return NULL;
}
uint32_t roam_win_mxcsr_default(void){return 0x1f80;}
void roam_log(const char *fmt,...){va_list args;va_start(args,fmt);vprintf(fmt,args);va_end(args);fflush(stdout);}
void roam_game_event(int ev,int peer,const char *text){printf("event %d peer %d: %s\n",ev,peer,text);fflush(stdout);}
int main(int argc,char **argv){
 int seconds=argc>1?atoi(argv[1]):90;
 static rc_session s;rc_config cfg={0};cfg.self_id=0x54455332;cfg.port=48762;cfg.room=rc_fnv1a("local-development");
 strcpy(cfg.name,"TestTwo");cfg.platform=RC_PLAT_TOOL;cfg.test_mode=1;cfg.max_players=2;cfg.rate_hz=20;
 rc_io io={0};rc_session_init(&s,&cfg,&io);roam_plat_cfg pc={48762,1,0,NULL};
 if(roam_plat_start(&pc,&s))return 1;
 rc_add_peer(&s,0x7f000001,48761,1,roam_plat_now_us());
 int64_t start=roam_plat_now_us(),next_log=start,until=start+(int64_t)seconds*1000000;
 uint16_t car_rev=0;
 while(roam_plat_now_us()<until){
  roam_win_poll();int64_t now=roam_plat_now_us();
  roam_plat_lock();
  for(int i=0;i<RC_TABLE;i++){
   rc_peer *p=&s.peer[i];if(p->state!=RC_PS_EST)continue;
   if(p->cfg_len&&p->cfg_rev!=car_rev){rc_set_cfg(&s,p->cfg,p->cfg_len,now);car_rev=p->cfg_rev;printf("car copied: %d bytes\n",p->cfg_len);fflush(stdout);}
   if(p->mo.n>0&&(p->status==RC_ST_ROAM||p->status==RC_ST_PAUSED)){
    rc_state st=p->mo.s[p->mo.n-1];st.t_us=(uint32_t)now;st.p[0]+=4.f;
    rc_set_status(&s,RC_ST_ROAM);rc_push_state(&s,&st,now);
   }else rc_set_status(&s,RC_ST_MENUS);
  }
  if(now>=next_log){printf("peers=%d cfg=%d tx=%u rx=%u bad=%u policy=%u\n",rc_established(&s),s.cfg_len,s.tx,s.rx,s.rx_bad,s.rx_policy);fflush(stdout);next_log=now+5000000;}
  roam_plat_unlock();Sleep(5);
 }
 roam_plat_stop(0);puts("peer stopped cleanly");return 0;
}
