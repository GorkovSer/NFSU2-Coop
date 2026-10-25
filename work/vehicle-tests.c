#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "vehicle_sync.h"
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t random_state=0x19479031;
static uint32_t random32(void){random_state=random_state*1664525u+1013904223u;return random_state;}
static void edges(void){
 vs_barrier a,b;vs_init(&a,10,20,1,90,0xaabbccdd);vs_init(&b,20,10,1,90,0x12345678);
 rc_coop_msg m;CHECK(vs_next(&b,0,&m));CHECK(!vs_loaded(&a,m.value));
 rc_coop_msg bad=m;bad.run++;CHECK(!vs_receive(&a,20,&bad));bad=m;bad.to=11;CHECK(!vs_receive(&a,20,&bad));CHECK(!vs_receive(&a,21,&m));
 CHECK(vs_receive(&a,20,&m));CHECK(!vs_ready(&a));CHECK(!vs_loaded(&a,0x99999999));CHECK(vs_loaded(&a,0x12345678));CHECK(!vs_ready(&a));
 bad=m;bad.op=RC_COOP_VEHICLE_ACK;bad.value=0x99999999;CHECK(!vs_receive(&a,20,&bad));bad.value=a.own;CHECK(vs_receive(&a,20,&bad));CHECK(vs_ready(&a));
 m.value++;CHECK(vs_receive(&a,20,&m)==-1);CHECK(!vs_ready(&a));CHECK(!vs_next(&a,1000000,&m));
 vs_init(&a,10,20,2,90,0);vs_init(&b,20,10,2,90,0);CHECK(vs_next(&b,0,&m));CHECK(vs_receive(&a,20,&m));CHECK(vs_loaded(&a,0));
 bad=m;bad.run=1;CHECK(!vs_receive(&a,20,&bad));
 puts("vehicle barrier: recipient/identity/run checks, wrong digest, no premature readiness, frozen equipment mismatch, digest zero and previous-run rejection passed");
}
typedef struct {rc_coop_msg msg;int from,to;int64_t when;int used;} packet;
static packet queue[512];
static void simulations(void){
 for(unsigned run=1;run<=1000;run++){
  int proof=run>500;
  vs_barrier side[2];uint32_t digest[2]={random32(),random32()};
  uint32_t physics[2]={random32(),random32()};
  vs_init(&side[0],10,20,run,90,digest[0]);vs_init(&side[1],20,10,run,90,digest[1]);
  if(proof)for(int s=0;s<2;s++)CHECK(vs_expect_physics(&side[s],physics[s]));
  for(int i=0;i<512;i++)queue[i].used=0;
  int ready=0;
  for(int64_t now=0;now<15000000;now+=10000){
   for(int s=0;s<2;s++){
    rc_coop_msg m;if(vs_next(&side[s],now,&m)){
     rc_msg wire={0},decoded;wire.h.type=RC_MSG_COOP;wire.h.sender=s?20:10;wire.h.seq=(uint16_t)(now/10000);wire.u.coop=m;
     uint8_t bytes[RC_MAX_DGRAM];int n=rc_encode(&wire,bytes,sizeof bytes);CHECK(n>0);CHECK(rc_decode(bytes,n,&decoded)==RC_OK);m=decoded.u.coop;
     if(random32()%100>=35)for(int duplicate=0;duplicate<(random32()%4==0?2:1);duplicate++){
      int pos=-1;for(int i=0;i<512;i++)if(!queue[i].used){pos=i;break;}CHECK(pos>=0);
      queue[pos]=(packet){m,s,1-s,now+(random32()%400000),1};
     }
    }
    /* Native loader completion is independent and may be delayed. A stale
     * cached car can never satisfy the digest announced by its owner. */
    if(side[s].remote_seen&&(!proof||side[s].physics_seen)&&now>(s?1300000:2700000)){
     CHECK(!vs_loaded(&side[s],digest[1-s]^1));
     if(proof){CHECK(!vs_loaded(&side[s],digest[1-s]));CHECK(vs_loaded_physics(&side[s],digest[1-s],physics[1-s]));}
     else CHECK(vs_loaded(&side[s],digest[1-s]));
    }
   }
   for(int i=511;i>=0;i--)if(queue[i].used&&queue[i].when<=now){packet p=queue[i];queue[i].used=0;CHECK(vs_receive(&side[p.to],p.from?20:10,&p.msg)==1);}
   CHECK(!side[0].fault&&!side[1].fault);
   if(vs_ready(&side[0])&&vs_ready(&side[1])){CHECK(now>2700000);ready=1;break;}
  }
  CHECK(ready);
 }
 puts("vehicle barrier: 1000 real encode/decode exchanges (500 physics-proof), 35% packet loss, jitter/reorder, duplicates and different loader times passed");
}
static void proof_edges(void){
 vs_barrier s;vs_init(&s,10,20,1,90,0);CHECK(vs_expect_physics(&s,0));CHECK(!vs_expect_physics(&s,1));
 rc_coop_msg m={RC_COOP_VEHICLE_PHYSICS,10,1,90,0};CHECK(!vs_receive(&s,99,&m));CHECK(vs_receive(&s,20,&m));
 CHECK(!vs_loaded(&s,0));CHECK(!vs_loaded_physics(&s,0,0));m.op=RC_COOP_VEHICLE;CHECK(vs_receive(&s,20,&m));
 CHECK(vs_loaded_physics(&s,0,0));CHECK(!vs_ready(&s));m.op=RC_COOP_VEHICLE_ACK;CHECK(vs_receive(&s,20,&m));CHECK(vs_ready(&s));
 m.op=RC_COOP_VEHICLE_PHYSICS;m.value=1;CHECK(vs_receive(&s,20,&m)==-1);CHECK(!vs_ready(&s));
 vs_init(&s,10,20,2,90,9);CHECK(vs_expect_physics(&s,5));m.run=2;m.value=7;CHECK(vs_receive(&s,20,&m));
 m.op=RC_COOP_VEHICLE;m.value=8;CHECK(vs_receive(&s,20,&m));CHECK(!vs_loaded_physics(&s,8,6)&&s.fault);
 puts("physics proof: no readiness without both proofs, zero digests, identity checks, frozen proof and calculated-physics mismatch rejection passed");
}
int main(void){edges();proof_edges();simulations();printf("PASS %u vehicle checks\n",checks);return 0;}
