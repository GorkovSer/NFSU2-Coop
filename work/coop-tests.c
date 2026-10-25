#include "coop_race.h"
#include "coop_stream.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static uint32_t rng=11235813;
static uint32_t random32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void wire_tests(void){
 uint8_t bytes[RC_MAX_DGRAM],bad[RC_MAX_DGRAM];
 for(unsigned op=1;op<RC_COOP__N;op++){
  rc_msg m={0},decoded;m.h.type=RC_MSG_COOP;m.h.sender=100;m.u.coop=(rc_coop_msg){(uint8_t)op,200,123,0xca207584,77};
  int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==40);CHECK(rc_decode(bytes,n,&decoded)==RC_OK);
  CHECK(decoded.u.coop.op==op&&decoded.u.coop.to==200&&decoded.u.coop.run==123&&decoded.u.coop.event==0xca207584&&decoded.u.coop.value==77);
  for(int size=0;size<n;size++)CHECK(rc_decode(bytes,size,&decoded)!=RC_OK);
  memcpy(bad,bytes,n);bad[n-1]^=1;CHECK(rc_decode(bad,n,&decoded)==RC_E_CRC);
  m.u.coop.run=0;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 }
 puts("wire: all controls, truncated packets, CRC, invalid run passed");
}
static rc_session sessions[2];
static struct {int to,n;uint16_t source;uint8_t data[RC_MAX_DGRAM];} network[256];
static int network_n;
static void send_packet(void *user,uint32_t ip,uint16_t port,const uint8_t *data,int n){
 (void)ip;int from=(int)(intptr_t)user;CHECK(port==(from?47210:47211));CHECK(network_n<256);
 network[network_n].to=from^1;network[network_n].source=(uint16_t)(from?47211:47210);network[network_n].n=n;
 memcpy(network[network_n++].data,data,n);
}
static void pump(int64_t now){for(int k=0;k<network_n;k++)rc_receive(&sessions[network[k].to],0x7f000001,network[k].source,network[k].data,network[k].n,now);network_n=0;}
static void connect_pair(int guest_coop){
 network_n=0;
 for(int i=0;i<2;i++){
  rc_config cfg={0};cfg.self_id=i?200:100;cfg.port=i?47211:47210;cfg.room=1234;strcpy(cfg.name,i?"Laptop":"Host");
  cfg.platform=RC_PLAT_WIN;cfg.test_mode=1;cfg.max_players=2;cfg.rate_hz=20;cfg.coop=i?guest_coop:1;
  rc_io io={(void*)(intptr_t)i,send_packet,0,0};rc_session_init(&sessions[i],&cfg,&io);
 }
 /* Only the guest lists an address, matching the LAN host setup in use. */
 CHECK(rc_add_peer(&sessions[1],0x7f000001,47210,1,1)>=0);
 for(int64_t now=1;now<7000000;now+=50000){rc_tick(&sessions[0],now);rc_tick(&sessions[1],now);pump(now);}
 CHECK(rc_established(&sessions[0])==1&&rc_established(&sessions[1])==1);
}
static void session_tests(void){
 connect_pair(1);rc_coop_msg m={RC_COOP_INVITE,200,77,88,0},received;uint32_t from;
 CHECK(rc_send_coop(&sessions[0],&m)==1);pump(7100000);
 CHECK(rc_take_coop(&sessions[1],&from,&received)==1&&from==100&&received.run==77);
 CHECK(!rc_take_coop(&sessions[1],&from,&received));
 for(int k=0;k<10;k++)CHECK(rc_send_coop(&sessions[0],&m)==1);
 pump(7200000);CHECK(sessions[1].n_coop_in==1);CHECK(rc_take_coop(&sessions[1],&from,&received));
 rc_msg raw={0};uint8_t bytes[RC_MAX_DGRAM];raw.h.type=RC_MSG_COOP;raw.h.sender=100;raw.u.coop=m;
 int n=rc_encode(&raw,bytes,sizeof bytes);CHECK(n>0);
 rc_receive(&sessions[1],0x7f000002,47210,bytes,n,7300000);CHECK(!sessions[1].n_coop_in);
 raw.u.coop.to=999;n=rc_encode(&raw,bytes,sizeof bytes);rc_receive(&sessions[1],0x7f000001,47210,bytes,n,7350000);CHECK(!sessions[1].n_coop_in);
 connect_pair(0);CHECK(!rc_send_coop(&sessions[0],&m));
 raw.u.coop=m;n=rc_encode(&raw,bytes,sizeof bytes);rc_receive(&sessions[1],0x7f000001,47210,bytes,n,7400000);CHECK(!sessions[1].n_coop_in);
 connect_pair(1);m.op=RC_COOP_VEHICLE;CHECK(!rc_send_coop(&sessions[0],&m));
 raw.u.coop=m;n=rc_encode(&raw,bytes,sizeof bytes);rc_receive(&sessions[1],0x7f000001,47210,bytes,n,7500000);CHECK(!sessions[1].n_coop_in);
 sessions[0].conf.vehicles=1;sessions[1].conf.vehicles=1;
 for(int64_t t=7600000;t<11000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(rc_send_coop(&sessions[0],&m));pump(11000000);CHECK(rc_take_coop(&sessions[1],&from,&received)&&received.op==RC_COOP_VEHICLE);
 m.op=RC_COOP_PRIZE;m.value=200;CHECK(!rc_send_coop(&sessions[0],&m));
 raw.u.coop=m;n=rc_encode(&raw,bytes,sizeof bytes);rc_receive(&sessions[1],0x7f000001,47210,bytes,n,11100000);CHECK(!sessions[1].n_coop_in);
 sessions[0].conf.rewards=1;sessions[1].conf.rewards=1;
 for(int64_t t=11200000;t<14600000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(rc_send_coop(&sessions[0],&m));pump(14600000);CHECK(rc_take_coop(&sessions[1],&from,&received)&&received.op==RC_COOP_PRIZE&&received.value==200);
 m.op=RC_COOP_VEHICLE_PHYSICS;m.value=0;CHECK(!rc_send_coop(&sessions[0],&m));
 sessions[0].conf.vehicle_proof=sessions[1].conf.vehicle_proof=1;
 for(int64_t now=14700000;now<22000000;now+=50000){rc_tick(&sessions[0],now);rc_tick(&sessions[1],now);pump(now);}
 CHECK(rc_send_coop(&sessions[0],&m));pump(22100000);
 CHECK(rc_take_coop(&sessions[1],&from,&received)&&received.op==RC_COOP_VEHICLE_PHYSICS&&received.value==0);
 puts("sessions: real handshake, one configured LAN endpoint, duplicate collapse, address/recipient/capability checks passed");
}
static void contact_capability_tests(void){
 connect_pair(1);
 CHECK(!sessions[0].peer[0].player_contact&&!sessions[1].peer[0].player_contact);
 sessions[0].conf.player_contact=1;
 for(int64_t t=7100000;t<10500000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(!sessions[0].peer[0].player_contact&&sessions[1].peer[0].player_contact);
 sessions[1].conf.player_contact=1;
 for(int64_t t=10600000;t<14000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(sessions[0].peer[0].player_contact&&sessions[1].peer[0].player_contact);
 sessions[0].conf.player_contact=0;
 for(int64_t t=14100000;t<17500000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(sessions[0].peer[0].player_contact&&!sessions[1].peer[0].player_contact);
 puts("player contact: HELLO capability absent by default, one-sided/two-sided enable and withdrawal passed");
}
static struct {int to;int64_t due;rc_coop_msg msg;} delayed[256];
static int delayed_n;
static void simulate(unsigned seed,int64_t base,int64_t guest_offset){
 rng=seed;cc_race pair[2];cc_init(&pair[0],100,200,1);cc_init(&pair[1],200,100,0);delayed_n=0;
 CHECK(cc_offer(&pair[0],seed+1,0xca207584,base));
 int releases[2]={0},loads[2]={0},result_actions[2]={0},drop_streak[2]={0},claims=0,confirmed=0;
 int64_t ready_at[2]={0},released_at[2]={0};
 for(int64_t time=0;time<30000000;time+=10000){
  int64_t host_now=base+time;
  for(int k=0;k<delayed_n;){
   if(delayed[k].due>time){k++;continue;}
   int to=delayed[k].to;rc_coop_msg msg=delayed[k].msg;delayed[k]=delayed[--delayed_n];
   int64_t offset=to?guest_offset:-guest_offset;
   cc_receive(&pair[to],to?100:200,&msg,host_now+(to?guest_offset:0),offset);
  }
  for(int i=0;i<2;i++){
   int64_t local=host_now+(i?guest_offset:0);uint32_t actions=cc_take_actions(&pair[i]);
   if(actions&CC_INVITATION)CHECK(cc_accept(&pair[i],0xca207584,local));
   if(actions&CC_LOAD_GAME){loads[i]++;ready_at[i]=time+100000+(random32()%2500000);}
   if(actions&CC_RELEASE_GAME){releases[i]++;released_at[i]=time;}
   if(actions&CC_RESULT)result_actions[i]++;
   CHECK(!(actions&CC_CANCEL));
   if(ready_at[i]&&time>=ready_at[i]&&pair[i].phase==CC_LOADING)cc_loaded(&pair[i],local);
   cc_step(&pair[i],local,1);
   rc_coop_msg msg;
   if(cc_next_packet(&pair[i],local,&msg)){
    int drop=(random32()%4)==0&&drop_streak[i]<2;
    if(drop){drop_streak[i]++;continue;}drop_streak[i]=0;
    int copies=1+((random32()%3)==0);
    for(int c=0;c<copies;c++){
     CHECK(delayed_n<256);delayed[delayed_n].to=i^1;delayed[delayed_n].due=time+(random32()%150000);
     delayed[delayed_n++].msg=msg;
    }
   }
  }
  if(releases[0]&&releases[1]&&!claims){cc_claim_finish(&pair[1],1);claims=1;}
  if(claims&&!confirmed&&time-released_at[0]>1500000){
   CHECK(pair[0].phase==CC_RUNNING); /* remote winner claims cannot award progress */
   CHECK(cc_confirm_finish(&pair[0],200,1,host_now));confirmed=1;
  }
  if(pair[0].result_acked&&pair[1].phase==CC_DONE&&result_actions[0]&&result_actions[1])break;
 }
 CHECK(loads[0]==1&&loads[1]==1&&releases[0]==1&&releases[1]==1);
 CHECK(llabs(released_at[0]-released_at[1])<=10000);
 CHECK(pair[0].winner==200&&pair[1].winner==200&&pair[0].result_acked);
 CHECK(result_actions[0]==1&&result_actions[1]==1);
 rc_coop_msg old={RC_COOP_INVITE,200,seed+1,0xca207584,0};
 cc_receive(&pair[1],100,&old,base+40000000,guest_offset);CHECK(pair[1].phase==CC_DONE);
}
static void edge_tests(void){
 cc_race host,guest;cc_init(&host,100,200,1);cc_init(&guest,200,100,0);
 CHECK(cc_offer(&host,12,88,1));cc_step(&host,30000001,1);CHECK(host.phase==CC_CANCELLED&&host.cancel_reason==CC_TIMEOUT);
 CHECK(!cc_offer(&host,12,88,40000000));CHECK(cc_offer(&host,13,88,40000000));
 cc_step(&host,40000001,0);CHECK(host.phase==CC_CANCELLED&&host.cancel_reason==CC_DISCONNECTED);
 rc_coop_msg invite={RC_COOP_INVITE,200,17,88,0};cc_receive(&guest,100,&invite,1,0);
 CHECK(!cc_accept(&guest,99,2));CHECK(guest.cancel_reason==CC_MISMATCH);
 invite.run=18;cc_receive(&guest,100,&invite,3,0);CHECK(cc_accept(&guest,88,4));
 rc_coop_msg load={RC_COOP_LOAD,200,18,88,0};cc_receive(&guest,100,&load,5,0);CHECK(guest.phase==CC_LOADING);
 cc_step(&guest,120000005,1);CHECK(guest.phase==CC_CANCELLED&&guest.cancel_reason==CC_TIMEOUT);
 cc_init(&host,100,200,1);host.phase=CC_RUNNING;host.run=1;host.event=88;
 rc_coop_msg fake={RC_COOP_RESULT,100,1,88,200};cc_receive(&host,200,&fake,1,0);CHECK(host.phase==CC_RUNNING);
 CHECK(!cc_confirm_finish(&host,999,1,2));CHECK(cc_confirm_finish(&host,100,2,3));
 CHECK(!cc_confirm_finish(&host,200,2,4));CHECK(cc_confirm_finish(&host,200,3,5));CHECK(host.phase==CC_DONE&&host.winner==0);
 cc_init(&host,100,200,1);host.phase=CC_COUNTDOWN;host.release_at=100;cc_step(&host,100,1);CHECK(host.phase==CC_CANCELLED);
 puts("edge cases: invitation/loading timeouts, disconnect, replay, mismatched event, result authority and missing start acknowledgement passed");
}
static void stream_tests(void){
 cs_stream host,guest;cs_init(&host,100,200,17,88,1,6);cs_init(&guest,200,100,17,88,0,6);
 rc_msg raw={0},dec;uint8_t b[RC_MAX_DGRAM];raw.h.type=RC_MSG_RACE;raw.h.sender=100;
 rc_race_frame *f=&raw.u.race;f->to=200;f->run=17;f->event=88;f->racers=6;f->n=5;f->seq=65535;
 for(int i=0;i<5;i++){f->car[i].slot=(uint8_t)(i?i+1:0);f->car[i].state.q[3]=1;f->car[i].state.p[0]=(float)i*10;f->car[i].state.t_us=900000;}
 int n=rc_encode(&raw,b,sizeof b);CHECK(n==320);CHECK(rc_decode(b,n,&dec)==RC_OK);
 uint8_t corrupt[RC_MAX_DGRAM];
 const int offsets[]={16,17,18,19,36,39,92,44};
 for(unsigned k=0;k<sizeof offsets/sizeof offsets[0];k++){
  memcpy(corrupt,b,n);
  if(k==7){uint32_t nan=0x7fc00000u;memcpy(corrupt+44,&nan,4);}
  else corrupt[offsets[k]]=(uint8_t)(k==0?2:k==1?7:k==2?1:k==4?6:k==6?0:1);
  uint32_t crc=rc_crc32(corrupt,(uint32_t)n-4);for(int j=0;j<4;j++)corrupt[n-4+j]=(uint8_t)(crc>>(8*j));
  CHECK(rc_decode(corrupt,n,&dec)!=RC_OK);
 }
 for(int size=0;size<n;size++)CHECK(rc_decode(b,size,&dec)!=RC_OK);
 CHECK(rc_decode(b,n,&dec)==RC_OK);CHECK(cs_receive(&guest,100,&dec.u.race,1000000));CHECK(!cs_receive(&guest,100,&dec.u.race,1000001));
 f->seq=0;CHECK(cs_receive(&guest,100,f,1050000));f->seq=65534;CHECK(!cs_receive(&guest,100,f,1050001));
 f->seq=1;f->run=16;CHECK(!cs_receive(&guest,100,f,1050001));f->run=17;
 f->car[4].slot=1;CHECK(!cs_receive(&guest,100,f,1050001));f->car[4].slot=5;
 CHECK(!cs_receive(&guest,999,f,1050001));CHECK(cs_receive(&guest,100,f,1100000));
 rc_pose pose;CHECK(cs_pose(&guest,2,1150000,50000,&pose));CHECK(pose.p[0]>9.9f&&pose.p[0]<10.1f);CHECK(!cs_pose(&guest,2,3200000,50000,&pose));
 f->to=100;f->n=1;f->seq=1;f->car[0].slot=1;f->car[0].position=1;f->car[0].completion=1;
 CHECK(cs_receive(&host,200,f,1000000));CHECK(host.position[1]==0&&host.completion[1]==0);
 f->seq=2;f->car[0].slot=2;CHECK(!cs_receive(&host,200,f,1050000));
 f->car[0].slot=1;f->n=2;f->car[1].slot=1;CHECK(rc_encode(&raw,b,sizeof b)<0);
 connect_pair(1);f->n=1;f->car[0].slot=0;f->to=200;CHECK(rc_send_race(&sessions[0],f));pump(7100000);
 rc_race_frame got;CHECK(rc_take_race(&sessions[1],100,&got)&&got.run==17);CHECK(!rc_take_race(&sessions[1],100,&got));
 puts("race stream: bounded wire format, roster authority, old-run/reorder rejection, sequence wrap, interpolation and timeout passed");
}
static void clock_tests(void){
 connect_pair(1);rc_peer *p=0;for(int i=0;i<RC_TABLE;i++)if(sessions[0].peer[i].state==RC_PS_EST)p=&sessions[0].peer[i];CHECK(p!=0);
 rc_msg raw={0};raw.h.type=RC_MSG_PONG;raw.h.sender=200;uint8_t b[RC_MAX_DGRAM];
 int64_t now=0x100010000LL;uint32_t t0=(uint32_t)(now-100000);p->last_ping_tx=now-100000;p->clock_valid=0;
 raw.u.ping.t0=t0;raw.u.ping.t1=t0+50000+456789000u;raw.u.ping.t2=raw.u.ping.t1;
 int n=rc_encode(&raw,b,sizeof b);rc_receive(&sessions[0],0x7f000001,47211,b,n,now);
 CHECK(p->clock_valid&&p->clock_offset_us==-456789000&&p->clock_rtt_us==100000);
 int32_t offset=p->clock_offset_us;raw.u.ping.t0--;n=rc_encode(&raw,b,sizeof b);
 rc_receive(&sessions[0],0x7f000001,47211,b,n,now+1);CHECK(p->clock_offset_us==offset&&p->clock_sample_at==now);
 /* Slightly slower valid samples must renew before consumers' 10s expiry. */
 int64_t first=now;
 for(int seconds=1;seconds<=30;seconds++){
  int64_t sample=first+(int64_t)seconds*1000000;
  p->last_ping_tx=sample-110000;raw.u.ping.t0=(uint32_t)p->last_ping_tx;
  raw.u.ping.t1=raw.u.ping.t0+55000+456789000u;raw.u.ping.t2=raw.u.ping.t1;
  n=rc_encode(&raw,b,sizeof b);rc_receive(&sessions[0],0x7f000001,47211,b,n,sample);
  CHECK(p->clock_valid&&sample-p->clock_sample_at<10000000&&p->clock_offset_us==offset);
  if(seconds==8)CHECK(p->clock_sample_at==sample&&p->clock_rtt_us==110000);
 }
 puts("clock: offset/wrap, unrelated PONG rejection and proactive lease renewal passed");
}
int main(void){
 wire_tests();session_tests();edge_tests();
 stream_tests();clock_tests();contact_capability_tests();
 for(unsigned seed=1;seed<=500;seed++)simulate(seed,seed%2?0xffff0000LL:1000000LL,seed%3?456789000LL:-12345000LL);
 printf("PASS %u checks; 500 two-client races with bounded packet loss, jitter, duplication, clock offset and timestamp wrap\n",checks);return 0;
}
