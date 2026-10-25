#define main previous_coop_main
#include "coop-tests.c"
#undef main
static rc_contact_scene scene_for(int k,uint32_t echo){
 return (rc_contact_scene){k?100:200,k?222:111,k?111:222,0,k?202:101,echo,0,0,k?302:301,1,1};
}
static rc_player_impulse impact(void){
 rc_player_impulse h={0};h.to=200;h.source=111;h.destination=222;h.host_world=101;h.guest_world=202;
 h.physics=302;h.op=RC_IMPULSE_DATA;h.seq=65535;h.t_us=12000000;h.dp[0]=10;h.dl[2]=2;return h;
}
static void prepare(void){
 connect_pair(1);
 for(int k=0;k<2;k++){
  sessions[k].conf.player_contact=1;sessions[k].conf.world_host=!k;
  sessions[k].conf.extensions=RC_EXT_PLAYER_IMPULSE|(k?0:RC_EXT_CONTACT_HOST);
  sessions[k].conf.extension_nonce=k?222:111;rc_set_status(&sessions[k],RC_ST_ROAM);
 }
 for(int64_t t=7100000;t<=11000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 for(int k=0;k<2;k++)CHECK(rc_extension_ready(&sessions[k],&sessions[k].peer[0],RC_EXT_PLAYER_IMPULSE,11000000));
 for(int64_t t=11050000;t<=12000000;t+=50000){
  for(int k=0;k<2;k++){
   uint32_t echo=sessions[k].peer[0].contact_seen?sessions[k].peer[0].contact_scene.world:0;
   rc_contact_scene s=scene_for(k,echo);CHECK(rc_publish_contact_scene(&sessions[k],&s,t));rc_tick(&sessions[k],t);
  }
  pump(t);
 }
 for(int k=0;k<2;k++)CHECK(rc_player_contact_ready(&sessions[k],&sessions[k].peer[0],12000000));
 CHECK(!sessions[0].conf.traffic&&!sessions[1].conf.traffic); /* independent */
}
int main(void){
 uint8_t bytes[RC_MAX_DGRAM];rc_msg m={0},out;m.h.sender=100;m.h.type=RC_MSG_PLAYER_IMPULSE;m.u.player_impulse=impact();
 int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==108&&rc_decode(bytes,n,&out)==RC_OK);
 CHECK(out.u.player_impulse.dp[0]==10&&out.u.player_impulse.dl[2]==2&&out.u.player_impulse.seq==65535);
 for(int i=0;i<n;i++)CHECK(rc_decode(bytes,i,&out)!=RC_OK);
 m.u.player_impulse.dp[0]=10001;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.player_impulse=impact();m.u.player_impulse.dl[1]=rc_bitsf(0x7fc00000);CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.player_impulse=impact();m.u.player_impulse.run=1;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.h.type=RC_MSG_CONTACT_SCENE;m.u.contact_scene=scene_for(0,202);n=rc_encode(&m,bytes,sizeof bytes);
 CHECK(n==64&&rc_decode(bytes,n,&out)==RC_OK);
 for(int i=0;i<n;i++)CHECK(rc_decode(bytes,i,&out)!=RC_OK);
 m.u.contact_scene.contact_class=17;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 prepare();rc_player_impulse h=impact(),got;uint32_t from;
 for(int i=0;i<3;i++)CHECK(rc_send_player_impulse(&sessions[0],&h,12000000));pump(12000000);
 CHECK(sessions[1].n_player_impulse==1);CHECK(rc_take_player_impulse(&sessions[1],&from,&got)&&from==100&&got.seq==65535);
 h.op=RC_IMPULSE_ACK;h.to=100;h.source=222;h.destination=111;
 CHECK(rc_send_player_impulse(&sessions[1],&h,12000000));pump(12000000);
 CHECK(rc_take_player_impulse(&sessions[0],&from,&got)&&from==200&&got.op==RC_IMPULSE_ACK);
 CHECK(!rc_send_player_impulse(&sessions[0],&h,12000000));
 h=impact();h.guest_world=999;CHECK(!rc_send_player_impulse(&sessions[0],&h,12000000));
 h=impact();h.physics=303;CHECK(!rc_send_player_impulse(&sessions[0],&h,12000000));
 h=impact();h.destination=999;CHECK(!rc_send_player_impulse(&sessions[0],&h,12000000));
 CHECK(!rc_player_contact_ready(&sessions[0],&sessions[0].peer[0],12250001));
 sessions[0].peer[0].extensions.features|=RC_EXT_CONTACT_HOST;CHECK(!rc_player_contact_ready(&sessions[0],&sessions[0].peer[0],12000000));
 prepare();
 /* Source frames stop but its network thread keeps running. */
 for(int64_t t=12050000;t<=12350000;t+=50000){rc_tick(&sessions[0],t);pump(t);}
 CHECK(!sessions[1].peer[0].contact_scene.active);
 CHECK(!rc_player_contact_ready(&sessions[1],&sessions[1].peer[0],12350000));
 prepare();rc_peer *p=&sessions[1].peer[0];int64_t seen=p->contact_at;
 m.h.type=RC_MSG_CONTACT_SCENE;m.h.sender=100;m.u.contact_scene=p->contact_scene;
 n=rc_encode(&m,bytes,sizeof bytes);rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12020000);
 CHECK(p->contact_at==seen);
 rc_contact_scene changed=scene_for(1,101);changed.world=203;CHECK(rc_publish_contact_scene(&sessions[1],&changed,12020000));
 CHECK(!rc_player_contact_ready(&sessions[1],p,12020000));
 m.h.type=RC_MSG_PLAYER_IMPULSE;m.u.player_impulse=impact();n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12030000);CHECK(!sessions[1].n_player_impulse);
 changed=scene_for(1,101);CHECK(rc_publish_contact_scene(&sessions[1],&changed,12040000));
 rc_clear_contact_scene(&sessions[1],12050000);CHECK(!rc_player_contact_ready(&sessions[1],p,12050000));
 prepare();p=&sessions[1].peer[0];m.h.type=RC_MSG_CONTACT_SCENE;m.h.sender=100;m.u.contact_scene=p->contact_scene;
 m.u.contact_scene.serial++;m.u.contact_scene.contact_class=2;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12010000);CHECK(p->contact_scene.active&&!p->contact_fault_world&&p->contact_scene.contact_class==2);
 m.u.contact_scene.serial++;m.u.contact_scene.contact_class=1;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12020000);CHECK(rc_player_contact_ready(&sessions[1],p,12020000));
 m.u.contact_scene.serial++;m.u.contact_scene.physics++;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12020001);CHECK(!p->contact_scene.active&&p->contact_fault_world==101);
 m.u.contact_scene.physics--;
 m.u.contact_scene.world=102;m.u.contact_scene.serial++;n=rc_encode(&m,bytes,sizeof bytes);
 rc_receive(&sessions[1],0x7f000001,47210,bytes,n,12030000);
 CHECK(p->contact_scene.world==102&&!p->contact_fault_world);
 changed=scene_for(1,102);CHECK(rc_publish_contact_scene(&sessions[1],&changed,12030000));CHECK(rc_player_contact_ready(&sessions[1],p,12030000));
 changed.physics++;CHECK(!rc_publish_contact_scene(&sessions[1],&changed,12040000));
 printf("PASS player impulse protocol: %u checks; 64/108-byte wire, role-independent-of-traffic, physical proof, mutual World echo, frame/scene leases, replay serial, nonce and DATA/ACK scope guards (no game)\n",checks);return 0;
}
