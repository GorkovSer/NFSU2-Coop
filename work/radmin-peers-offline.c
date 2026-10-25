/* Real core handshake, synthetic transport: no sockets, game or VPN changes. */
#define main previous_coop_main
#include "coop-tests.c"
#undef main
static const uint32_t vpn_ip[2]={0x1a8d6a34,0x1a0c2238};
static void vpn_send(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 int from=(int)(intptr_t)u;CHECK(ip==vpn_ip[from^1]&&port==47210&&network_n<256);
 network[network_n].to=from^1;network[network_n].source=47210;network[network_n].n=n;memcpy(network[network_n++].data,b,n);
}
static void vpn_pump(int64_t t){
 for(int k=0;k<network_n;k++){int to=network[k].to;rc_receive(&sessions[to],vpn_ip[to^1],47210,network[k].data,network[k].n,t);}network_n=0;
}
static void vpn_pair(int both){
 network_n=0;
 for(int k=0;k<2;k++){
  rc_config cfg={0};cfg.self_id=k?200:100;cfg.port=47210;cfg.room=1234;cfg.max_players=2;cfg.rate_hz=20;cfg.coop=1;strcpy(cfg.name,k?"Guest":"Host");
  rc_io io={(void*)(intptr_t)k,vpn_send,0,0};rc_session_init(&sessions[k],&cfg,&io);
  if(k||both)CHECK(rc_add_peer(&sessions[k],vpn_ip[k^1],47210,1,1)>=0);
 }
 for(int64_t t=1;t<7000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);vpn_pump(t);}
}
int main(void){
 CHECK(!rc_addr_private(vpn_ip[0])&&!rc_addr_private(vpn_ip[1]));
 vpn_pair(0);CHECK(!rc_established(&sessions[0])&&!rc_established(&sessions[1])&&sessions[0].rx_policy>0);
 vpn_pair(1);CHECK(rc_established(&sessions[0])==1&&rc_established(&sessions[1])==1);
 rc_coop_msg m={RC_COOP_INVITE,200,77,88,0},got;uint32_t from;
 CHECK(rc_send_coop(&sessions[0],&m));vpn_pump(7100000);CHECK(rc_take_coop(&sessions[1],&from,&got)&&from==100&&got.run==77);
 m=(rc_coop_msg){RC_COOP_READY,100,77,88,0};CHECK(rc_send_coop(&sessions[1],&m));vpn_pump(7150000);CHECK(rc_take_coop(&sessions[0],&from,&got)&&from==200);
 /* The explicit exception does not open unrelated public endpoints. */
 rc_msg raw={0};raw.h.type=RC_MSG_COOP;raw.h.sender=100;raw.u.coop=(rc_coop_msg){RC_COOP_INVITE,200,99,88,0};uint8_t b[RC_MAX_DGRAM];int n=rc_encode(&raw,b,sizeof b);CHECK(n>0);
 uint32_t refused=sessions[1].rx_policy;rc_receive(&sessions[1],0x1a010203,47210,b,n,7200000);CHECK(sessions[1].rx_policy==refused+1&&!sessions[1].n_coop_in);
 sessions[0].conf.test_mode=1;CHECK(rc_add_peer(&sessions[0],vpn_ip[1],47210,1,7300000)<0);
 printf("PASS Radmin explicit peers: %u checks; real core HELLO/handshake and bidirectional co-op over synthetic26/8 endpoints; both-side configuration required, unlisted peers refused, loopback test guard retained; no sockets/game\n",checks);return 0;
}
