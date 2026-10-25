/* Actual adapter, synthetic engine globals. No native function execution. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "coop_stream.h"
typedef void CPU;
typedef struct {int ghost;} Slot;
static struct {int enabled,bound,host,clock_valid;uint32_t peer,world,car[6];int32_t clock_offset;int64_t next_send;cc_race control;cs_stream stream;Slot slot[6];} NC;
static rc_session g_s;
static int64_t now=10000000;
static uint32_t pause_flag,manager[8],flow[4],clock_data[0x120/4],wall_tick=9999;
static int same=1,focused=1,native_pause,locks,traffic_resumes,bodies,checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t addr(void *p){return (uint32_t)(uintptr_t)p;}
static uint32_t *ref(uint32_t p){if(p==0x86676c)return &pause_flag;if(p==0x8651ac)return &wall_tick;if(p>=0x89cf48&&p<0x89d068)return &clock_data[(p-0x89cf48)/4];return (void*)(uintptr_t)p;}
#define MR32(a) (*ref(a))
static uint32_t rd32(uint32_t p){if(p==0x8650b8)return addr(manager);if(p==0x8900d8)return addr(flow);return MR32(p);}
static int ok(uint32_t p,unsigned n){return (p==addr(manager)&&n<=sizeof manager)||(p==addr(flow)&&n<=sizeof flow);}
static int nc_same_world(void){return same;}
static void roam_plat_lock(void){locks++;}
static void roam_plat_unlock(void){locks--;CHECK(locks>=0);}
static int64_t roam_plat_now_us(void){return now;}
static int nc_window_active(void){return focused;}
static int nc_original_world_paused(uint32_t w){CHECK(w==NC.world);return native_pause;}
static void nw_pause_resume(int64_t t){CHECK(t==now);traffic_resumes++;}
static int nc_remote_car(uint32_t car){
 for(int i=0;i<NC.stream.racers;i++)if(car==NC.car[i])return i!=(NC.host?0:1)&&(!NC.host||i==1);
 return 0;
}
static void body_set(CPU *c,uint32_t car,int solid){(void)c;CHECK(nc_remote_car(car));CHECK(!solid);bodies++;}
#define rlog(...) ((void)0)
#include "native_race_pause.inc"
int main(void){
 CHECK(!nc_pause_gate(123,0)&&nc_pause_gate(123,1));
 NC.enabled=NC.bound=NC.host=NC.clock_valid=1;NC.peer=200;NC.world=123;NC.control.phase=CC_RUNNING;NC.control.run=77;NC.control.event=88;
 NC.car[0]=1000;NC.car[1]=1001;NC.stream.racers=2;NC.stream.seq_init=1;NC.stream.seq=100;NC.stream.motion[0].n=2;NC.next_send=999;
 g_s.conf.self_id=100;g_s.conf.race_pause=1;
 rc_peer *p=&g_s.peer[0];p->id=200;p->state=RC_PS_EST;p->race_pause=1;p->clock_valid=1;p->clock_sample_at=p->last_rx_us=now;
 CHECK(nc_pause_frame(now)&&g_s.pause.active);
 g_s.pause.phase=CP_RUNNING;g_s.pause.remote_seen=1;g_s.pause.remote_flags=0;g_s.pause.peer_at=now;
 CHECK(!nc_pause_frame(now));CHECK(traffic_resumes==1&&bodies==1&&NC.next_send==0);
 CHECK(NC.stream.motion[0].n==0&&NC.stream.seq==100&&NC.stream.last_rx==now&&NC.slot[1].ghost);
 CHECK(!NC.slot[0].ghost); /* local human was never disabled */
 NC.stream.racers=4;NC.car[2]=1002;NC.car[3]=1003;
 for(int host=0;host<=1;host++){
  NC.host=host;memset(NC.slot,0,sizeof NC.slot);bodies=0;nc_pause_was_held=1;
  g_s.pause.host=host;g_s.pause.phase=CP_RUNNING;g_s.pause.blocked=0;g_s.pause.local_flags=0;g_s.pause.remote_flags=0;
  g_s.pause.host_at=now;g_s.pause.need_sync=0;
  CHECK(!nc_pause_frame(now));CHECK(bodies==(host?1:3));
  CHECK(!NC.slot[host?0:1].ghost&&NC.slot[host?1:0].ghost);
  CHECK(NC.slot[2].ghost==!host&&NC.slot[3].ghost==!host);
 }
 NC.host=1;traffic_resumes=1;
 CHECK(!nc_pause_gate(NC.world,0));
 focused=0;CHECK(nc_pause_gate(NC.world,0)&&nc_pause_held());focused=1;
 uint32_t saved=999;CHECK(!nc_pause_clock_enter(1,&saved)&&saved==999);
 clock_data[0x54/4]=555;
 CHECK(nc_pause_clock_enter(0x89cf48,&saved)&&saved==0&&pause_flag==1);
 CHECK(clock_data[0x54/4]==wall_tick);
 CHECK(!nc_pause_raw(0)); /* our borrowed flag must not become a local pause request */
 uint32_t inner;CHECK(nc_pause_clock_enter(0x89cf48,&inner)&&inner==1);
 nc_pause_clock_leave(inner);CHECK(pause_flag==1);nc_pause_clock_leave(saved);CHECK(pause_flag==0);
 pause_flag=7;CHECK(nc_pause_clock_enter(0x89cf48,&saved)&&saved==7);nc_pause_clock_leave(saved);CHECK(pause_flag==7);
 pause_flag=0;manager[7]=1;CHECK(nc_pause_raw(0));manager[7]=0;flow[0]=1;CHECK(nc_pause_raw(0));flow[0]=0;
 clock_data[0x54/4]=0x7fffffff;CHECK(nc_pause_clock_enter(0x89cf48,&saved));nc_pause_clock_leave(saved);CHECK(clock_data[0x54/4]==0x7fffffff);
 native_pause=1;CHECK(nc_pause_raw(nc_original_world_paused(NC.world)));native_pause=0;
 rc_race_frame f={0};f.n=1;NC.clock_offset=123456;f.car[0].state.t_us=(uint32_t)now-123456;
 CHECK(nc_pause_accept_frame(&f,now));f.car[0].state.t_us-=600000;CHECK(!nc_pause_accept_frame(&f,now));
 f.car[0].state.t_us=(uint32_t)now-123456+300000;CHECK(!nc_pause_accept_frame(&f,now));
 same=0;CHECK(!nc_pause_frame(now)&&!g_s.pause.active);CHECK(traffic_resumes==1);
 CHECK(!nc_pause_clock_enter(0x89cf48,&saved));CHECK(!locks);
 printf("PASS native race pause: %d checks; query scope, frame intent, nested clock-flag restoration, no pause feedback, native manager/menu conditions, motion/traffic re-anchor, stale source and world exit guards (mock only)\n",checks);return 0;
}
