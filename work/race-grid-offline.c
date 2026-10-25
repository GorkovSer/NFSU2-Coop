/* Runs the actual grid adapter against mock native bodies, never NFS. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
typedef void CPU;
static uint8_t cars[2][0x9e0],race[0xc44],record[2][32],body[0x134],info[0x200];
static struct {int host;uint32_t world,race,car[2],record[2];cc_race control;} NC;
static int live=1,ready=1,checks,moves,roads,sections;
#define A(p) ((uint32_t)(uintptr_t)(p))
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define MRF32(a) (*(float*)(uintptr_t)(a))
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
#define FN_ROADTRACK 1
#define FN_CAR_SECTION 2
#define rlog(...) ((void)0)
static int ok(uint32_t p,unsigned n){return p>65535&&p+n>p;}
static uint32_t rd32(uint32_t p){return p?MR32(p):0;}
static uint32_t player_car(void){return A(cars[1]);}
static int nc_same_world(void){return live;}
static int ni_ready(uint32_t car){CHECK(car==player_car());return ready;}
static void ni_set_state(uint32_t physical,const float *p,const float *m,const float *v,const float *w){
 CHECK(physical==A(info));CHECK(p[0]==20&&p[1]==33&&p[2]==2);
 CHECK(m[0]==0&&m[1]==1&&m[4]==-1&&m[10]==1&&m[15]==1);
 CHECK(!v[0]&&!v[1]&&!v[2]&&!w[0]&&!w[1]&&!w[2]);moves++;
}
static void gcall1(CPU *c,int fn,uint32_t obj,uint32_t arg){(void)c;CHECK(fn==1&&obj==player_car()+0x8d8&&arg==player_car()+0x60);roads++;}
static void gcall0(CPU *c,int fn,uint32_t obj){(void)c;CHECK(fn==2&&obj==player_car());sections++;}
#include "native_race_grid.inc"
static void setup(void){
 memset(cars,0,sizeof cars);memset(&NC,0,sizeof NC);live=ready=1;
 NC.world=99;NC.race=A(race);NC.control.run=42;NC.control.phase=CC_LOADING;MR32(NC.race)=3;
 for(int i=0;i<2;i++){
  NC.car[i]=A(cars[i]);NC.record[i]=A(record[i]);MR32(NC.record[i])=NC.car[i];
  MRF32(NC.car[i]+0x90)=1;MRF32(NC.car[i]+0xa4)=1;MRF32(NC.car[i]+0xb8)=1;
  MR32(NC.car[i]+0x2c)=0x1234+i;MR32(NC.car[i]+0x10)=0x2345+i;
 }
 MRF32(NC.car[0]+0x50)=20;MRF32(NC.car[0]+0x54)=30;MRF32(NC.car[0]+0x58)=2;
 MRF32(NC.car[0]+0x90)=0;MRF32(NC.car[0]+0x94)=1;
 MRF32(NC.car[0]+0xa0)=-1;MRF32(NC.car[0]+0xa4)=0;
 MRF32(NC.car[1]+0x60)=3; /* own COM +3 along local forward */
 MR32(NC.car[1]+0x3c)=A(body);MR32(A(body)+0x68)=A(info);
 nc_grid_capture();
}
int main(void){
 setup();CHECK(NG.captured&&!NG.placed);
 /* Incoming host pose moves the replaced bot; saved grid must survive. */
 MRF32(NC.car[0]+0x50)=999;
 ready=0;CHECK(nc_grid_frame(0)==0&&!moves);ready=1;
 CHECK(nc_grid_frame(0)==1&&moves==1&&roads==1&&sections==1);
 CHECK(MRF32(NC.car[1]+0x50)==20&&MRF32(NC.car[1]+0x54)==30);
 CHECK(rd32(NC.car[1]+0x2c)==0x1235&&rd32(NC.car[1]+0x10)==0x2346);
 CHECK(nc_grid_frame(0)==1&&moves==1);
 NC.control.phase=CC_RUNNING;CHECK(nc_grid_frame(0)==1&&moves==1);
 setup();NC.control.phase=CC_RUNNING;CHECK(nc_grid_frame(0)==-1&&moves==1);
 setup();MR32(NC.race)=4;CHECK(nc_grid_frame(0)==-1&&moves==1);
 setup();NC.control.run++;CHECK(nc_grid_frame(0)==-1&&moves==1);
 setup();live=0;CHECK(nc_grid_frame(0)==-1&&moves==1);
 setup();MR32(NC.record[1])=NC.car[0];CHECK(nc_grid_frame(0)==-1&&moves==1);
 setup();MRF32(NC.car[0]+0x90)=2;nc_grid_capture();CHECK(!NG.captured&&nc_grid_frame(0)==-1);
 setup();MRF32(NC.car[0]+0x50)=10000;nc_grid_capture();CHECK(!NG.captured);
 setup();NC.host=1;nc_grid_capture();CHECK(nc_grid_frame(0)==1&&moves==1);
 printf("PASS grid adapter: %d checks; native bot slot, own COM/model/controller, once-only, loading/identity/basis guards; mock only\n",checks);
}
