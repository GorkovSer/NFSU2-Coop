/* Real core packets and interpolation, synthetic frame clock; no game/socket. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame_watch.h"
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {int from,n;uint8_t b[RC_MAX_DGRAM];} packet;
static packet queue[512];static int qn;static int64_t now;
static rc_session a,b;
static void send_packet(void *u,uint32_t ip,uint16_t port,const uint8_t *buf,int n){
    (void)ip;(void)port;CHECK(qn<512);packet *p=&queue[qn++];
    p->from=(int)(uintptr_t)u;p->n=n;memcpy(p->b,buf,(size_t)n);
}
static void flush(void){
    for(int i=0;i<qn;i++){
        packet p=queue[i];rc_receive(p.from==1?&b:&a,0x7f000001,p.from==1?12001:12002,p.b,p.n,now);
    }qn=0;
}
int main(void){
    rc_config c={0};c.self_id=1;c.port=12001;c.room=123;c.test_mode=1;c.max_players=2;c.rate_hz=20;strcpy(c.name,"Fixture");
    rc_io io={0};io.user=(void *)1;io.send=send_packet;
    rc_session_init(&a,&c,&io);c.self_id=2;c.port=12002;io.user=(void *)2;rc_session_init(&b,&c,&io);
    CHECK(rc_add_peer(&a,0x7f000001,12002,1,0)==0);
    CHECK(rc_add_peer(&b,0x7f000001,12001,1,0)==0);
    frame_watch w={0};CHECK(!fw_tick(&w,&a,10000000));
    for(now=10000;now<2500000;now+=10000){rc_tick(&a,now);rc_tick(&b,now);flush();}
    CHECK(rc_established(&a)==1&&rc_established(&b)==1);
    rc_set_status(&a,RC_ST_ROAM);flush();
    rc_state st={0};st.t_us=500000;st.q[3]=1;st.p[0]=100;st.v[0]=40;st.speed=40;
    fw_frame(&w,&a,now);CHECK(rc_push_state(&a,&st,now));flush();
    CHECK(b.peer[0].mo.n==1);int64_t last_snap=b.peer[0].last_snap_rx;
    CHECK(!fw_tick(&w,&a,now+749999));CHECK(a.status==RC_ST_ROAM);
    now+=750000;CHECK(fw_tick(&w,&a,now));flush();
    CHECK(a.status==RC_ST_PAUSED&&b.peer[0].status==RC_ST_PAUSED);
    rc_pose p;CHECK(rc_interp_peek(&b.peer[0].mo,b.peer[0].mo.t[0]+1000000,150000,&p));
    CHECK(p.p[0]==100&&p.v[0]==0&&p.speed==0&&p.mode==RC_MODE_HOLD);
    for(int i=0;i<30000;i++){
        now+=10000;CHECK(!fw_tick(&w,&a,now));rc_tick(&a,now);rc_tick(&b,now);flush();
    }
    CHECK(rc_established(&a)==1&&rc_established(&b)==1&&!b.peer[0].lost);
    CHECK(b.peer[0].last_snap_rx==last_snap&&a.nlast==1&&b.peer[0].mo.n==1);
    uint8_t clock=a.clock;fw_frame(&w,&a,now);CHECK(a.clock==(uint8_t)(clock+1)&&a.nlast==0);
    fw_frame(&w,&a,now+10000);CHECK(a.clock==(uint8_t)(clock+1));
    rc_set_status(&a,RC_ST_ROAM);flush();
    CHECK(b.peer[0].mo.status==RC_ST_PAUSED); /* HELLO alone cannot resume old velocity. */
    st.t_us=550000;st.p[0]=102;CHECK(rc_push_state(&a,&st,now));flush();
    CHECK(b.peer[0].mo.n==1&&b.peer[0].mo.clock==a.clock&&b.peer[0].mo.status==RC_ST_ROAM);
    CHECK(b.peer[0].mo.s[0].p[0]==102);
    /* Other lifecycle states, including races, are never changed by this guard. */
    for(int k=0;k<RC_ST__N;k++)if(k!=RC_ST_ROAM){
        frame_watch z={0};a.status=(uint8_t)k;fw_frame(&z,&a,now);
        CHECK(!fw_tick(&z,&a,now+5000000)&&a.status==k&&!z.parked);
    }
    a.status=RC_ST_ROAM;a.closed=1;fw_frame(&w,&a,now);CHECK(!fw_tick(&w,&a,now+5000000));
    printf("PASS: %d checks; five-minute frame stop, real HELLO keepalive, stationary pose, fresh-clock resume, lifecycle guards\n",checks);
    return 0;
}
