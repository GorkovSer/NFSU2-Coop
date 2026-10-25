#include <stdio.h>
#include <stdlib.h>
#include "roam_core.h"
#include "race_clock.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t rng=12345;
static uint32_t random32(void){rng=rng*1664525u+1013904223u;return rng;}
int main(void){
 for(int mode=RM_URL;mode<=RM_RANK;mode++){
  rk_clock clock={0};CHECK(rk_step(&clock,0,1));CHECK(rk_step(&clock,8000,0));
  CHECK(rk_step(&clock,40000,1));CHECK(rk_step(&clock,60000,0));CHECK(rk_step(&clock,140000,0));
  rm_outcome h,g;CHECK(rm_init(&h,77,88,99,mode,0,1,4,1));CHECK(rm_init(&g,77,88,99,mode,0,1,4,0));
  h.peer_agreed=g.agreed=1;
  rm_value v[4]={{100000,mode==RM_DRIFT?1000u:0u,RM_FINISH,120000},
   {90000,mode==RM_DRIFT?1000u:0u,RM_FINISH,124000}, /* faster LOCAL clock, later real finish */
   {110000,mode==RM_DRIFT?900u:0u,RM_FINISH,130000},{120000,0,RM_TIMEOUT,140000}};
  CHECK(rm_set(&g,1,&v[1]));for(int k=0;k<4;k++)if(k!=1)CHECK(rm_set(&h,k,&v[k]));
  rm_packet p={0};p.op=RM_OWNER;p.mode=mode;p.rounds=1;p.racers=4;p.to=100;p.source=222;p.destination=111;
  p.run=77;p.event=88;p.layout=99;p.serial=1;p.car[1]=v[1];CHECK(rm_receive(&h,&p));
  CHECK(!rm_resolve(&h));uint32_t elapsed[RM_CARS]={0};
  for(int k=0;k<4;k++)CHECK(rk_elapsed(&clock,v[k].wall,&elapsed[k]));
  CHECK(elapsed[0]==92000&&elapsed[1]==96000);CHECK(rm_timing(&h,elapsed)&&rm_resolve(&h));
  CHECK(h.rank[0]==1&&h.rank[1]==2&&rm_team_winner(&h)==1);
  CHECK(rm_next(&h,1000000,&p));p.to=200;p.source=111;p.destination=222;
  CHECK(p.op==RM_BOARD&&p.final&&rm_receive(&g,&p));CHECK(!memcmp(h.rank,g.rank,sizeof h.rank));
  CHECK(!memcmp(h.elapsed,g.elapsed,sizeof h.elapsed));CHECK(g.car[1].ticks==90000&&g.car[1].wall==124000);
  p.serial++;p.car[1].wall++;p.elapsed[1]++;CHECK(!rm_receive(&g,&p)&&g.fault);
 }
 /* Randomized ledger versus a per-tick reference, independently of race rules. */
 for(int run=0;run<500;run++){
  rk_clock c={0};uint32_t reference[10001],active=0;int held=1;CHECK(rk_step(&c,0,held));reference[0]=0;
  for(uint32_t t=1;t<=10000;t++){
   if(!held)active++;reference[t]=active;
   if(random32()%97==0){held=!held;CHECK(rk_step(&c,t,held));}
  }
  CHECK(rk_step(&c,10000,held));
  for(int k=0;k<200;k++){uint32_t t=random32()%10001,out;CHECK(rk_elapsed(&c,t,&out)&&out==reference[t]);}
 }
 rk_clock c={0};uint32_t out;CHECK(rk_step(&c,0,0));CHECK(!rk_elapsed(&c,1,&out));
 CHECK(rk_step(&c,20,1));CHECK(rk_elapsed(&c,20,&out)&&out==20);CHECK(!rk_step(&c,19,0)&&c.fault);
 memset(&c,0,sizeof c);CHECK(rk_step(&c,0,0));
 for(unsigned i=0;i<RK_SPANS;i++){CHECK(rk_step(&c,2*i+1,1));CHECK(rk_step(&c,2*i+2,0));}
 CHECK(rk_step(&c,2*RK_SPANS+1,1));CHECK(!rk_step(&c,2*RK_SPANS+2,0)&&c.fault);
 printf("PASS shared race clock: %u checks, all result modes, inverted local timers, identical pause-excluding final board, immutable owner stamps, 500 random ledgers and bounded overflow; no game\n",checks);
 return 0;
}
