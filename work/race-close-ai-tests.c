#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "race_outcome.h"
static int checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
 for(int mode=RM_URL;mode<=RM_RANK;mode++){
  rm_outcome s;CHECK(rm_init(&s,1,2,3,mode,0,1,4,1));s.peer_agreed=1;
  rm_value a={10000,mode==RM_DRIFT?30:0,RM_FINISH,11000};
  CHECK(rm_set(&s,0,&a));CHECK(!rm_close_ai(&s,20000,22000)&&!s.car[2].status);
  /* Remote owner is normally imported by rm_receive. */
  s.car[1]=(rm_value){15000,mode==RM_DRIFT?40:0,RM_FINISH,16000};
  a=(rm_value){12000,mode==RM_DRIFT?100:0,RM_FINISH,13000};CHECK(rm_set(&s,2,&a));
  CHECK(rm_close_ai(&s,20000,22000));CHECK(!memcmp(&a,&s.car[2],sizeof a));
  CHECK(s.car[3].status==RM_TIMEOUT&&s.car[3].wall==22000&&!s.car[3].score);
  rm_value frozen=s.car[3];CHECK(rm_close_ai(&s,25000,27000));CHECK(!memcmp(&frozen,&s.car[3],sizeof frozen));
  uint32_t elapsed[6]={11000,16000,13000,22000};CHECK(rm_timing(&s,elapsed)&&rm_resolve(&s));
  CHECK(s.rank[3]==4);CHECK(!rm_close_ai(&s,30000,33000));
  s.final=0;s.host=0;s.car[3].status=0;CHECK(!rm_close_ai(&s,30000,33000)&&!s.car[3].status);
 }
 printf("PASS AI closure: %d checks; all modes, both-human barrier, real finished bots preserved, unfinished DNF, immutable closure, host authority and final board\n",checks);
}
