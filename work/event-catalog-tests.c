#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "roam_core.h"
static unsigned checks,sent;static uint8_t packet[RC_MAX_DGRAM];static int length;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static void send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){(void)u;(void)ip;(void)port;memcpy(packet,b,n);length=n;sent++;}
static void setup(rc_session *s,uint32_t own,uint32_t remote){
 memset(s,0,sizeof *s);s->conf.self_id=own;s->conf.coop=1;s->conf.extensions=RC_EXT_CAREER_MAP;s->conf.extension_nonce=own*100;s->io.send=send_packet;
 rc_peer *p=&s->peer[0];p->id=remote;p->ip=remote;p->port=47210;p->configured=1;p->state=RC_PS_EST;p->coop=1;p->core_ver=4;p->extension_seen=1;p->extension_at=1000000;
 p->extensions.nonce=remote*100;p->extensions.echo=own*100;p->extensions.features=RC_EXT_CAREER_MAP;
}
int main(void){
 static rc_session a,b;setup(&a,1,2);setup(&b,2,1);
 rc_event_catalog v={0};v.to=2;v.source=100;v.destination=200;v.serial=1;v.stage=2;v.n=3;v.event[0]=101;v.event[1]=102;v.event[2]=103;
 CHECK(rc_send_event_catalog(&a,&v,1000000)&&sent==1);rc_msg decoded;CHECK(rc_decode(packet,length,&decoded)==RC_OK);
 for(int i=0;i<length;i++)CHECK(rc_decode(packet,i,&decoded)!=RC_OK);
 rc_receive(&b,1,47210,packet,length,1000000);CHECK(b.peer[0].event_catalog_seen&&b.peer[0].event_catalog.n==3);
 rc_receive(&b,1,47210,packet,length,1100000);CHECK(b.peer[0].event_catalog_at==1000000);
 v.serial=2;v.n=0;CHECK(rc_send_event_catalog(&a,&v,1100000));rc_receive(&b,1,47210,packet,length,1100000);CHECK(b.peer[0].event_catalog.n==0);
 v.serial=1;v.n=3;CHECK(rc_send_event_catalog(&a,&v,1100000));rc_receive(&b,1,47210,packet,length,1100000);CHECK(b.peer[0].event_catalog.n==0);
 v.serial=3;v.source=999;CHECK(!rc_send_event_catalog(&a,&v,1100000));v.source=100;v.destination=999;CHECK(!rc_send_event_catalog(&a,&v,1100000));v.destination=200;
 a.conf.extensions=0;CHECK(!rc_send_event_catalog(&a,&v,1100000));a.conf.extensions=RC_EXT_CAREER_MAP;
 CHECK(rc_send_event_catalog(&a,&v,1100000));b.peer[0].extensions.nonce=999;rc_receive(&b,1,47210,packet,length,1100000);CHECK(b.peer[0].event_catalog.serial==2);b.peer[0].extensions.nonce=100;
 CHECK(!rc_send_event_catalog(&a,&v,2600000));
 rc_msg m={0};m.h.type=RC_MSG_EVENT_CATALOG;m.h.sender=1;m.u.event_catalog=v;
 m.u.event_catalog.n=65;CHECK(rc_encode(&m,packet,sizeof packet)<0);m.u.event_catalog.n=3;
 m.u.event_catalog.event[2]=101;CHECK(rc_encode(&m,packet,sizeof packet)<0);m.u.event_catalog.event[2]=0;CHECK(rc_encode(&m,packet,sizeof packet)<0);
 m.u.event_catalog.n=64;for(int i=0;i<64;i++)m.u.event_catalog.event[i]=i+1;
 int n=rc_encode(&m,packet,sizeof packet);CHECK(n>0&&n<400&&rc_decode(packet,n,&decoded)==RC_OK&&decoded.u.event_catalog.n==64);
 printf("PASS career catalog: %u checks; codec/truncation/64 IDs/duplicates/empty withdrawal, negotiated recipient+nonces+lease, replay and reorder; no game\n",checks);
}
