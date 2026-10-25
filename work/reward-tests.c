#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "reward_sync.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t seed=729138;
static uint32_t random32(void){seed=seed*1664525u+1013904223u;return seed;}
static void edges(void){
 rw_reward a,b;rw_init(&a,10,20,1,90,100,1);rw_init(&b,20,10,1,90,200,0);
 uint32_t after=0;CHECK(!rw_credit(&a,100,50,&after));CHECK(!rw_authorize(&a,1));CHECK(!rw_offer(&b,200));
 CHECK(!rw_offer(&a,RW_MAX_PRIZE+1));CHECK(rw_offer(&a,200));CHECK(!rw_offer(&a,300));
 rc_coop_msg m;CHECK(rw_next(&a,0,&m));CHECK(!rw_receive(&b,99,&m));rc_coop_msg bad=m;bad.run=2;CHECK(!rw_receive(&b,10,&bad));
 bad=m;bad.to=99;CHECK(!rw_receive(&b,10,&bad));bad=m;bad.event=91;CHECK(!rw_receive(&b,10,&bad));
 CHECK(rw_receive(&b,10,&m));CHECK(rw_ready(&b));CHECK(!rw_ready(&a));CHECK(rw_next(&b,0,&m));
 bad=m;bad.value=199;CHECK(!rw_receive(&a,20,&bad));CHECK(rw_receive(&a,20,&m));CHECK(rw_ready(&a));
 CHECK(rw_authorize(&a,1));CHECK(rw_authorize(&b,1));
 CHECK(rw_credit(&a,999,100,&after)==-1);CHECK(!a.paid);
 CHECK(rw_credit(&a,100,RW_MAX_BALANCE-100,&after)==-1);CHECK(!a.paid);
 CHECK(rw_credit(&a,100,50,&after)==1&&after==250);CHECK(!rw_credit(&a,100,after,&after));
 CHECK(rw_credit(&b,200,100000,&after)==1&&after==100200);
 CHECK(rw_authorize(&a,0)==-1);CHECK(!rw_credit(&a,100,250,&after));
 puts("reward edges: host-only terms, identity/run/event/profile guards, frozen amount, overflow, duplicate credit, and different starting balances passed");
}
typedef struct {rc_coop_msg m;int to;int64_t at;int used;} packet;
static packet queue[512];
static void simulations(void){
 for(unsigned run=1;run<=500;run++){
  rw_reward s[2];rw_init(&s[0],10,20,run,90,100,1);rw_init(&s[1],20,10,run,90,200,0);
  uint32_t prize=run%7?random32()%20001:0,bank[2]={5000,900000},baseline[2]={5000,900000};
  int won=run%5!=0;CHECK(rw_offer(&s[0],prize));for(int i=0;i<512;i++)queue[i].used=0;
  int agreed=0;
  for(int64_t now=0;now<15000000;now+=10000){
   for(int k=0;k<2;k++){
    rc_coop_msg m;if(rw_next(&s[k],now,&m)&&random32()%100>=35){
     rc_msg raw={0},decoded;raw.h.type=RC_MSG_COOP;raw.h.sender=k?20:10;raw.u.coop=m;
     uint8_t b[RC_MAX_DGRAM];int n=rc_encode(&raw,b,sizeof b);CHECK(n==40);CHECK(rc_decode(b,n,&decoded)==RC_OK);
     for(int d=0;d<2;d++){int at=-1;for(int j=0;j<512;j++)if(!queue[j].used){at=j;break;}CHECK(at>=0);queue[at]=(packet){decoded.u.coop,1-k,now+(random32()%350000),1};}
    }
   }
   for(int j=511;j>=0;j--)if(queue[j].used&&queue[j].at<=now){packet p=queue[j];queue[j].used=0;CHECK(rw_receive(&s[p.to],p.to?10:20,&p.m)==1);}
   if(rw_ready(&s[0])&&rw_ready(&s[1])){agreed=1;break;}
  }
  CHECK(agreed&&s[0].amount==s[1].amount);
  /* Private spending/income after agreement is never overwritten by host. */
  bank[0]-=43;baseline[0]-=43;bank[1]+=17;baseline[1]+=17;
  for(int duplicate=0;duplicate<100;duplicate++)for(int k=0;k<2;k++){
   CHECK(rw_authorize(&s[k],won));uint32_t after;
   int credit=rw_credit(&s[k],k?200:100,bank[k],&after);CHECK(credit==(duplicate==0));if(credit)bank[k]=after;
  }
  CHECK(bank[0]-baseline[0]==(won?prize:0));CHECK(bank[1]-baseline[1]==(won?prize:0));
 }
 puts("reward simulations: 500 races, 35% loss/reordering/duplicates, zero prizes, team losses, separate purchases/bonuses, and 100 repeated result deliveries passed");
}
int main(void){edges();simulations();printf("PASS %u reward checks\n",checks);return 0;}
