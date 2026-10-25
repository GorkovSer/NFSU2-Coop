#include "traffic_sync.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void fill(rc_msg *m,int n,uint32_t time){
 memset(m,0,sizeof *m);m->h.type=RC_MSG_TRAFFIC;m->h.sender=10;
 rc_traffic_frame *f=&m->u.traffic;f->to=20;f->world=7;f->seq=1;f->n=(uint8_t)n;f->t_us=time;
 for(int i=0;i<n;i++){f->car[i].id=(uint32_t)i+1;f->car[i].type=100+(uint32_t)i;f->car[i].state.t_us=time;f->car[i].state.q[3]=1;f->car[i].state.p[0]=(float)i*10;}
}
static void wire(void){
 rc_msg m,d;uint8_t b[RC_MAX_DGRAM];
 for(int count=0;count<=16;count++){
  fill(&m,count,1000000);int n=rc_encode(&m,b,sizeof b);CHECK(n==44+60*count);CHECK(n<=1200);
  CHECK(rc_decode(b,n,&d)==RC_OK);CHECK(d.u.traffic.n==count&&d.u.traffic.t_us==1000000);
  for(int cut=0;cut<n;cut++)CHECK(rc_decode(b,cut,&d)!=RC_OK);
 }
 fill(&m,2,1000000);m.u.traffic.car[1].id=1;CHECK(rc_encode(&m,b,sizeof b)<0);
 fill(&m,1,1000000);m.u.traffic.car[0].type=0;CHECK(rc_encode(&m,b,sizeof b)<0);
 fill(&m,0,1000000);m.u.traffic.world=0;CHECK(rc_encode(&m,b,sizeof b)<0);
 puts("traffic wire: 0..16 cars, bounded datagrams, all truncated lengths, duplicate IDs and missing type/world rejected");
}
static rc_session pair[2];
static struct {int to,n;uint8_t b[RC_MAX_DGRAM];} net[256];static int nn;
static void tx(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)ip;(void)port;CHECK(nn<256);net[nn].to=((int)(intptr_t)u)^1;net[nn].n=n;memcpy(net[nn++].b,b,n);
}
static void pump(int64_t now){for(int i=0;i<nn;i++){int t=net[i].to;rc_receive(&pair[t],0x7f000001,(uint16_t)(t?47210:47211),net[i].b,net[i].n,now);}nn=0;}
static void sessions(void){
 for(int i=0;i<2;i++){
  rc_config c={0};c.self_id=i?20:10;c.port=(uint16_t)(i?47211:47210);c.room=123;c.platform=3;c.test_mode=1;c.max_players=2;c.rate_hz=20;c.traffic=1;c.world_host=!i;strcpy(c.name,i?"guest":"host");
  rc_io io={(void*)(intptr_t)i,tx,0,0};rc_session_init(&pair[i],&c,&io);
 }
 CHECK(rc_add_peer(&pair[1],0x7f000001,47210,1,1)>=0);
 for(int64_t now=1;now<7000000;now+=50000){rc_tick(&pair[0],now);rc_tick(&pair[1],now);pump(now);}
 CHECK(rc_established(&pair[0])==1&&rc_established(&pair[1])==1);
 rc_msg m;fill(&m,16,7100000);CHECK(rc_send_traffic(&pair[0],&m.u.traffic));pump(7100000);
 rc_traffic_frame f;CHECK(rc_take_traffic(&pair[1],10,&f)&&f.n==16);CHECK(!rc_take_traffic(&pair[1],10,&f));
 m.u.traffic.to=10;CHECK(!rc_send_traffic(&pair[1],&m.u.traffic));
 m.h.sender=20;uint8_t b[RC_MAX_DGRAM];int n=rc_encode(&m,b,sizeof b);rc_receive(&pair[0],0x7f000001,47211,b,n,7200000);CHECK(!rc_take_traffic(&pair[0],20,&f));
 for(int i=0;i<RC_TABLE;i++)if(pair[1].peer[i].state==RC_PS_EST){CHECK(pair[1].peer[i].world_host&&pair[1].peer[i].traffic);CHECK(pair[1].peer[i].clock_valid);}
 puts("traffic sessions: capability negotiation, host-only send/receive and clock sampling without co-op enabled passed");
 rc_traffic_contact hit={0},received;hit.to=10;hit.world=7;hit.id=1;hit.seq=65535;hit.dv[0]=1;hit.t_us=7100000;
 CHECK(!rc_send_traffic_contact(&pair[1],&hit));
 for(int i=0;i<2;i++)pair[i].conf.traffic_contact=1;
 for(int64_t now=7200000;now<14500000;now+=50000){rc_tick(&pair[0],now);rc_tick(&pair[1],now);pump(now);}
 uint32_t from;CHECK(rc_send_traffic_contact(&pair[1],&hit));CHECK(rc_send_traffic_contact(&pair[1],&hit));pump(14600000);
 CHECK(rc_take_traffic_contact(&pair[0],&from,&received)&&from==20&&received.seq==65535);CHECK(!rc_take_traffic_contact(&pair[0],&from,&received));
 CHECK(!rc_send_traffic_contact(&pair[0],&hit));
 rc_msg raw={0};raw.h.type=RC_MSG_TRAFFIC_CONTACT;raw.h.sender=20;raw.u.traffic_contact=hit;
 uint8_t wire[RC_MAX_DGRAM];raw.u.traffic_contact.to=999;int len=rc_encode(&raw,wire,sizeof wire);CHECK(len>0);
 rc_receive(&pair[0],0x7f000001,47211,wire,len,14600001);CHECK(!rc_take_traffic_contact(&pair[0],&from,&received));
 raw.u.traffic_contact.to=10;len=rc_encode(&raw,wire,sizeof wire);pair[0].conf.traffic_contact=0;
 rc_receive(&pair[0],0x7f000001,47211,wire,len,14600002);CHECK(!rc_take_traffic_contact(&pair[0],&from,&received));
 puts("traffic contact sessions: negotiated client-to-host only, wrong recipient and disabled capability rejected, queued duplicates collapsed");
}
static void contact_wire(void){
 rc_msg m={0},d;m.h.type=RC_MSG_TRAFFIC_CONTACT;m.h.sender=20;
 m.u.traffic_contact.to=10;m.u.traffic_contact.world=7;m.u.traffic_contact.id=1;m.u.traffic_contact.seq=65535;
 m.u.traffic_contact.dv[0]=12;m.u.traffic_contact.dw[0]=2;
 uint8_t b[RC_MAX_DGRAM],bad[RC_MAX_DGRAM];int n=rc_encode(&m,b,sizeof b);CHECK(n==92);CHECK(rc_decode(b,n,&d)==RC_OK);
 CHECK(d.u.traffic_contact.seq==65535&&d.u.traffic_contact.dv[0]==12);
 for(int len=0;len<n;len++)CHECK(rc_decode(b,len,&d)!=RC_OK);
 const unsigned offsets[]={16,17,24,32,40,52,64,76};
 for(unsigned i=0;i<sizeof offsets/sizeof offsets[0];i++){
  memcpy(bad,b,n);if(i<2)bad[offsets[i]]=2;else{uint32_t v=i<4?0:0x7fc00000;memcpy(bad+offsets[i],&v,4);}
  uint32_t crc=rc_crc32(bad,n-4);memcpy(bad+n-4,&crc,4);CHECK(rc_decode(bad,n,&d)!=RC_OK);
 }
 m.u.traffic_contact.dv[0]=26;CHECK(rc_encode(&m,b,sizeof b)<0);m.u.traffic_contact.dv[0]=1;
 m.u.traffic_contact.dw[0]=9;CHECK(rc_encode(&m,b,sizeof b)<0);m.u.traffic_contact.dw[0]=0;
 m.u.traffic_contact.p[2]=2001;CHECK(rc_encode(&m,b,sizeof b)<0);
 puts("traffic contact wire: 92-byte layout, truncated/malformed/NaN packets and excessive velocity/position rejected");
}
static void lifecycle(void){
 static ts_client s;rc_msg m;rc_pose p;ts_init(&s,20,10,1000000);fill(&m,16,1000000);
 CHECK(ts_receive(&s,10,&m.u.traffic,1010000,0));CHECK(ts_pose(&s,8,1050000,16000,&p));CHECK(p.p[0]>69.9f&&p.p[0]<70.1f);
 CHECK(!ts_receive(&s,10,&m.u.traffic,1010001,0));m.u.traffic.seq++;CHECK(!ts_receive(&s,11,&m.u.traffic,1010001,0));
 m.u.traffic.car[0].type++;CHECK(!ts_receive(&s,10,&m.u.traffic,1010001,0));m.u.traffic.car[0].type--;
 m.u.traffic.n=0;CHECK(ts_receive(&s,10,&m.u.traffic,1010001,0));CHECK(!ts_pose(&s,8,1050000,16000,&p));
 fill(&m,1,1100000);m.u.traffic.seq=3;CHECK(ts_receive(&s,10,&m.u.traffic,1110000,0));
 ts_scene(&s,88,1200000);m.u.traffic.seq=4;CHECK(!ts_receive(&s,10,&m.u.traffic,1210000,0));
 m.u.traffic.context=88;CHECK(!ts_receive(&s,10,&m.u.traffic,1210000,0)); /* old send time */
 m.u.traffic.world=8;m.u.traffic.t_us=1200000;m.u.traffic.car[0].state.t_us=1200000;CHECK(ts_receive(&s,10,&m.u.traffic,1210000,0));
 ts_scene(&s,0,1300000);m.u.traffic.context=0;m.u.traffic.world=7;m.u.traffic.t_us=1300000;CHECK(!ts_receive(&s,10,&m.u.traffic,1310000,0));
 m.u.traffic.world=9;CHECK(ts_receive(&s,10,&m.u.traffic,1310000,0));CHECK(ts_expire(&s,3400000));CHECK(!ts_pose(&s,1,3400000,16000,&p));
 m.u.traffic.seq++;m.u.traffic.t_us=3400000;CHECK(ts_receive(&s,10,&m.u.traffic,3410000,0));
 ts_init(&s,20,10,0xffff0000LL);fill(&m,1,0x10000);m.u.traffic.world=0xffffffff;m.u.traffic.seq=65535;
 CHECK(ts_receive(&s,10,&m.u.traffic,0x100010000LL,0));m.u.traffic.seq=0;m.u.traffic.t_us+=50000;CHECK(ts_receive(&s,10,&m.u.traffic,0x100010000LL+50000,0));
 m.u.traffic.world=1;CHECK(ts_receive(&s,10,&m.u.traffic,0x100010000LL+50001,0));m.u.traffic.world=0xffffffff;CHECK(!ts_receive(&s,10,&m.u.traffic,0x100010000LL+50002,0));
 puts("traffic lifecycle: empty despawn, identity reuse checks, scene barriers, stale packets, expiry/recovery and clock/world/sequence wrap passed");
}
int main(void){wire();contact_wire();sessions();lifecycle();printf("PASS %u traffic checks\n",checks);return 0;}
