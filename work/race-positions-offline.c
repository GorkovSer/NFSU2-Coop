#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "coop_stream.h"
static uint8_t race[0xc44],rec[6][0x20];
static struct {int host,bound;uint32_t race,record[6],car[6];cc_race control;cs_stream stream;} NC;
static int live=1,checks;
#define MR8(a) (*(uint8_t*)(uintptr_t)(a))
#define MR16(a) (*(uint16_t*)(uintptr_t)(a))
#define MR32(a) (*(uint32_t*)(uintptr_t)(a))
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t rd32(uint32_t a){return MR32(a);}
static int ok(uint32_t a,unsigned n){return a&&n<=0x20;}
static int nc_same_world(void){return live;}
static int64_t roam_plat_now_us(void){return 10000000;}
static int cs_stale_test(const cs_stream *s,int64_t t){return !s->seq_init||t-s->last_rx>2000000;}
#define cs_stale cs_stale_test
#include "native_race_positions.inc"
int main(void){
 NC.race=(uint32_t)(uintptr_t)race;NC.bound=1;NC.control.phase=CC_RUNNING;
 NC.stream.racers=4;NC.stream.seq_init=1;NC.stream.last_rx=10000000;
 for(int i=0;i<4;i++){
  NC.record[i]=(uint32_t)(uintptr_t)rec[i];NC.car[i]=100+i;
  MR32(NC.record[i])=NC.car[i];MR8(NC.record[i]+0xa)=i+1;
  MR32(NC.race+0xbd0+4*i)=NC.record[i];
 }
 NC.stream.position[0]=3;NC.stream.position[2]=1;NC.stream.position[3]=4;
 nc_positions_frame();int expected[]={3,2,1,4};
 for(int i=0;i<4;i++){CHECK(MR8(NC.record[i]+0xa)==expected[i]);CHECK(rd32(NC.race+0xbd0+4*(expected[i]-1))==NC.record[i]);CHECK(!MR16(NC.record[i]+0x10));}
 uint8_t saved[sizeof race];memcpy(saved,race,sizeof race);
 NC.stream.position[3]=1;nc_positions_frame();CHECK(!memcmp(saved,race,sizeof race));
 NC.stream.position[3]=0;nc_positions_frame();CHECK(!memcmp(saved,race,sizeof race));NC.stream.position[3]=4;
 NC.host=1;nc_positions_frame();CHECK(!memcmp(saved,race,sizeof race));NC.host=0;
 NC.stream.last_rx=1;nc_positions_frame();CHECK(!memcmp(saved,race,sizeof race));NC.stream.last_rx=10000000;
 MR32(NC.race+0xbd0)=999;nc_positions_frame();CHECK(rd32(NC.race+0xbd0)==999);
 printf("PASS live rank adapter: %d checks; authoritative permutation, inferred own rank, board identity, no fabricated completion, duplicate/missing/stale/host refusal; mock only\n",checks);
}
