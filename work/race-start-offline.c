#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define G_CAREER_RACE 1
#define G_RACE 2
#define CC_LOADING 3
typedef void CPU;
static struct{int enabled;uint32_t race;struct{uint32_t event,run;int phase;}control;}NC;
static uint32_t flow=100,event=200,state=4,hash=123;
static uint32_t race=300,race_state=2,live_world=400;static int intros;
static uint32_t world(void){return live_world;}
static int nc_same_world(void){return live_world==400;}
static void nc_original_state(void){}
static unsigned originals,transitions,loads,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
#define MR32(p) rd32(p)
static int ok(uint32_t p,unsigned n){(void)n;return p&&(p==flow||p==event||p==race);}
static uint32_t rd32(uint32_t p){return p==G_RACE?race:p==race?race_state:p==0x8900d8?flow:p==G_CAREER_RACE?event:p==flow?state:p==event+8?hash:0;}
static void nc_original_start(void){}
static uint32_t rh_call(CPU*c,uint32_t fn,uint32_t self,uint32_t edx,int n,const uint32_t*a){
 (void)c;(void)edx;
 if(fn==(uint32_t)(uintptr_t)nc_original_state){CHECK(n==2&&a[0]==race&&a[1]==3);intros++;return 0;}
 if(fn==(uint32_t)(uintptr_t)nc_original_start){CHECK(!self&&!n);originals++;return 0;}
 CHECK(fn==0x601020&&self==flow&&state==0&&n==2&&a[0]==16&&!a[1]);loads++;return 0;
}
static uint32_t gcall1(CPU*c,uint32_t fn,uint32_t self,uint32_t mode){
 (void)c;CHECK(fn==0x603630&&self==flow&&state==4&&!mode);transitions++;state=0;return 0;
}
#define rlog(...) ((void)0)
#include "native_race_start.inc"
#include "native_race_cinematic.inc"
int main(void){
 NC.control.event=123;nc_engine_start(0);CHECK(originals==1&&transitions==1&&loads==1);
 /* A marker/menu start already in a supported native transition is preserved. */
 for(unsigned s=0;s<9;s++)if(s!=4){state=s;nc_engine_start(0);CHECK(transitions==1&&loads==1&&state==s);}
 state=4;hash=456;nc_engine_start(0);CHECK(transitions==1&&loads==1);
 hash=123;flow=0;nc_engine_start(0);CHECK(transitions==1&&loads==1);
 flow=100;event=0;nc_engine_start(0);CHECK(transitions==1&&loads==1);
 CHECK(originals==12);
 event=200;NC.enabled=1;NC.race=race;NC.control.phase=CC_LOADING;NC.control.run=7;
 CHECK(!ncp_bind_ready(race));CHECK(ncp_hold(race,3));CHECK(ncp_bind_ready(race));
 CHECK(!ncp_frame(0,0)&&!intros);CHECK(ncp_hold(race,3));
 CHECK(!ncp_frame(0,1)&&intros==1);CHECK(!ncp_hold(race,3));
 CHECK(!ncp_frame(0,1)&&intros==1);race_state=3;CHECK(ncp_frame(0,1));
 race_state=2;NC.control.run++;CHECK(!ncp_bind_ready(race));CHECK(ncp_hold(race,3));
 live_world++;CHECK(!ncp_current(race));NC.enabled=0;CHECK(!ncp_hold(race,3));
 printf("PASS race start: %u checks; city deferred loader, native menu paths retained, invalid scope refused; mock engine\n",checks);
}
