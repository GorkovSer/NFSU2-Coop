/* The build driver includes the ACTUAL generated nc_unbind, with a synthetic
 * native backend. No game executable code is called. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {int unused;} CPU;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static struct {
 int bound,host,loaded_noted,result_applied,next_send,seq;
 struct {int racers;} stream;
 uint32_t car[6],brain[6],record[6],slot[6],old_phys[6];
} NC;
static int same_world,release_calls,reward_calls,physics_calls,body_calls;
static uint32_t modes[7];static int enabled[7],valid[7];
enum {FN_CAR_SETPHYS=1};
static int nc_same_world(void){return same_world;}
static int ok(uint32_t car,int size){CHECK(size==0x9e0);return valid[car/0x100];}
static uint32_t rd32(uint32_t address){return modes[(address-0x28)/0x100];}
static void nm_reset(void){CHECK(!NC.bound);}
static void nr_reward_solo_start(void){CHECK(!NC.bound);reward_calls++;}
static void nv_race_release(CPU *c){(void)c;CHECK(!NC.bound);release_calls++;}
static void body_set(CPU *c,uint32_t car,int on){
 (void)c;CHECK(!on||modes[car/0x100]!=0);enabled[car/0x100]=on;body_calls++;
}
static void gcall1(CPU *c,int fn,uint32_t car,uint32_t mode){
 (void)c;CHECK(fn==FN_CAR_SETPHYS);CHECK(!enabled[car/0x100]);physics_calls++;
 /* Emulate the real network-ownership guard: restoration fails if bound. */
 if(!NC.bound)modes[car/0x100]=mode;
}
#include "native-unbind-extracted.inc"
int main(void){
 CPU cpu={0};
 for(int host=0;host<2;host++)for(int racers=2;racers<=6;racers++)
 for(int live=0;live<2;live++)for(int zero=0;zero<2;zero++){
  memset(&NC,0,sizeof NC);memset(modes,0,sizeof modes);
  memset(enabled,0,sizeof enabled);memset(valid,1,sizeof valid);
  release_calls=reward_calls=physics_calls=body_calls=0;
  NC.bound=1;NC.host=host;NC.stream.racers=racers;same_world=live;
  NC.loaded_noted=NC.result_applied=NC.next_send=NC.seq=99;
  for(int k=0;k<racers;k++){
   NC.car[k]=(k+1)*0x100;NC.brain[k]=NC.record[k]=NC.slot[k]=99;
   NC.old_phys[k]=zero?0:3;modes[k+1]=1;
  }
  nc_unbind(&cpu);
  CHECK(!NC.bound&&!NC.loaded_noted&&!NC.result_applied&&!NC.next_send&&!NC.seq);
  CHECK(release_calls==1&&reward_calls==1);
  CHECK(physics_calls==(live?racers-1:0)&&body_calls==2*physics_calls);
  for(int k=0;k<racers;k++){
   CHECK(!NC.car[k]&&!NC.brain[k]&&!NC.record[k]&&!NC.slot[k]);
   int restored=live&&k!=(host?0:1);
   CHECK(modes[k+1]==(restored?(zero?0:3):1));
   CHECK(enabled[k+1]==(restored&&!zero));
  }
 }
 printf("PASS generated nc_unbind: %u checks; host/guest, 2..6 racers, world loss, zero/nonzero restoration, disable-before-physics and release-before-ownership guards (mock engine)\n",checks);
 return 0;
}
