#define main previous_coop_main
#include "coop-tests.c"
#undef main
static void enable_extension(void){
 connect_pair(1);
 for(int k=0;k<2;k++){
  sessions[k].conf.traffic=sessions[k].conf.traffic_contact=1;
  sessions[k].conf.world_host=!k;
  sessions[k].conf.extensions=RC_EXT_TRAFFIC_IMPULSE;
  sessions[k].conf.extension_nonce=k?222:111;
 }
 for(int64_t t=7100000;t<=12000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 for(int k=0;k<2;k++)CHECK(rc_extension_ready(&sessions[k],&sessions[k].peer[0],RC_EXT_TRAFFIC_IMPULSE,12000000));
}
static rc_impulse sample(void){
 rc_impulse h={0};h.to=100;h.source=222;h.destination=111;h.world=123;h.id=17;
 h.op=RC_IMPULSE_DATA;h.t_us=12000000;h.seq=65535;h.impulse[0]=100;h.arm[1]=1;return h;
}
int main(void){
 uint8_t bytes[RC_MAX_DGRAM];rc_msg m={0},out;
 m.h.sender=200;m.h.type=RC_MSG_IMPULSE;m.u.impulse=sample();
 int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==104);CHECK(rc_decode(bytes,n,&out)==RC_OK);
 CHECK(out.u.impulse.impulse[0]==100&&out.u.impulse.arm[1]==1&&out.u.impulse.seq==65535);
 for(int k=0;k<n;k++)CHECK(rc_decode(bytes,k,&out)!=RC_OK);
 m.u.impulse.arm[0]=13;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.impulse=sample();m.u.impulse.impulse[0]=rc_bitsf(0x7fc00000);CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.impulse=sample();m.u.impulse.destination=0;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.h.type=RC_MSG_EXTENSIONS;m.u.extensions=(rc_extensions){100,222,111,1,0xffffffff};
 n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==44&&rc_decode(bytes,n,&out)==RC_OK);
 for(int k=0;k<n;k++)CHECK(rc_decode(bytes,k,&out)!=RC_OK);
 enable_extension();rc_impulse h=sample(),got;uint32_t from;
 for(int i=0;i<3;i++)CHECK(rc_send_impulse(&sessions[1],&h,12000000));
 pump(12000000);CHECK(sessions[0].n_impulse==1);
 CHECK(rc_take_impulse(&sessions[0],&from,&got)&&from==200&&got.seq==65535&&got.impulse[0]==100);
 CHECK(!rc_take_impulse(&sessions[0],&from,&got));
 h.op=RC_IMPULSE_ACK;h.to=200;h.source=111;h.destination=222;
 CHECK(rc_send_impulse(&sessions[0],&h,12000000));pump(12000000);
 CHECK(rc_take_impulse(&sessions[1],&from,&got)&&from==100&&got.op==RC_IMPULSE_ACK);
 h=sample();CHECK(!rc_send_impulse(&sessions[0],&h,12000000));
 h.source=333;CHECK(!rc_send_impulse(&sessions[1],&h,12000000));
 h=sample();h.destination=444;CHECK(!rc_send_impulse(&sessions[1],&h,12000000));
 h=sample();CHECK(!rc_send_impulse(&sessions[1],&h,14000000)); /* extension lease */
 m.h.type=RC_MSG_IMPULSE;m.h.sender=200;m.u.impulse=sample();
 n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==104);
 rc_receive(&sessions[0],0x7f000002,47211,bytes,n,12010000);CHECK(!sessions[0].n_impulse);
 m.u.impulse.source=333;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[0],0x7f000001,47211,bytes,n,12020000);CHECK(!sessions[0].n_impulse);
 sessions[0].conf.extension_nonce=999;m.u.impulse=sample();n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[0],0x7f000001,47211,bytes,n,12030000);CHECK(!sessions[0].n_impulse);
 enable_extension();rc_peer *p=&sessions[0].peer[0];rc_extensions old=p->extensions;int64_t lease=p->extension_at;
 m.h.type=RC_MSG_EXTENSIONS;m.h.sender=200;m.u.extensions=old;
 n=rc_encode(&m,bytes,sizeof bytes);rc_receive(&sessions[0],0x7f000001,47211,bytes,n,12020000);
 CHECK(p->extension_at==lease); /* replay cannot extend lease */
 m.u.extensions.serial--;m.u.extensions.nonce=777;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[0],0x7f000001,47211,bytes,n,12030000);CHECK(p->extensions.nonce==222);
 sessions[1].conf.extensions=0;
 CHECK(!rc_send_impulse(&sessions[1],&(rc_impulse){0},12030000));
 for(int64_t t=12100000;t<=15000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(!rc_extension_ready(&sessions[0],p,1,15000000)&&p->core_ver==3);
 printf("PASS contact extension: %u checks; explicit v4 negotiation, nonce echo/lease, data/ACK roles, 104-byte wire, truncation, finite/bounds guards, duplicate inbox, stale serial and legacy withdrawal (no game)\n",checks);
 return 0;
}
