#define main previous_coop_main
#include "coop-tests.c"
#undef main
static void prepare_modes(void){
 connect_pair(1);
 for(int k=0;k<2;k++){sessions[k].conf.extensions=RC_EXT_RACE_MODES|RC_EXT_RANK_TABLE|RC_EXT_RANK_CLOCK|(k?0:RC_EXT_CONTACT_HOST);sessions[k].conf.extension_nonce=k?222:111;}
 for(int64_t t=7100000;t<=11000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 for(int k=0;k<2;k++)CHECK(rc_extension_ready(&sessions[k],&sessions[k].peer[0],RC_EXT_RACE_MODES,11000000));
}
static int fixture_resolve(rm_outcome *s){
 uint32_t elapsed[RM_CARS]={0};for(int k=0;k<s->racers;k++)elapsed[k]=s->car[k].wall;
 rm_timing(s,elapsed);return rm_resolve(s);
}
static rm_outcome outcomes[2];
static struct {int to,n;uint16_t source;int64_t due;uint8_t data[RC_MAX_DGRAM];} lossy[1024];
static int lossy_n;static int64_t mode_now;
static void mode_send(void *u,uint32_t ip,uint16_t port,const uint8_t *b,int n){
 (void)ip;(void)port;int from=(int)(intptr_t)u;
 if(random32()%100<35)return;
 int copies=random32()%3==0?2:1;
 while(copies--){CHECK(lossy_n<1024);lossy[lossy_n].to=from^1;lossy[lossy_n].n=n;lossy[lossy_n].source=from?47211:47210;lossy[lossy_n].due=mode_now+(random32()%150000);memcpy(lossy[lossy_n++].data,b,n);}
}
static void mode_pump(int64_t now){
 for(int k=lossy_n-1;k>=0;k--)if(lossy[k].due<=now){rc_receive(&sessions[lossy[k].to],0x7f000001,lossy[k].source,lossy[k].data,lossy[k].n,now);lossy[k]=lossy[--lossy_n];}
}
static void exchange(int64_t now){
 mode_now=now;
 for(int k=0;k<2;k++){
  rm_packet p;if(rm_next(&outcomes[k],now,&p)){p.to=k?100:200;p.source=k?222:111;p.destination=k?111:222;rc_send_outcome(&sessions[k],&p,now);}
  rc_tick(&sessions[k],now);
 }
 mode_pump(now);
 for(int k=0;k<2;k++){uint32_t from;rm_packet p;while(rc_take_outcome(&sessions[k],&from,&p)){CHECK(from==(k?100u:200u));rm_receive(&outcomes[k],&p);}}
}
#ifndef OUTCOME_FIXTURE_ONLY
int main(void){
 rm_packet p={0};p.to=p.source=p.destination=p.run=p.event=p.layout=1;p.op=RM_OWNER;p.mode=RM_DRAG;p.rounds=1;p.racers=4;
 rc_msg m={0},out;m.h.sender=100;m.h.type=RC_MSG_OUTCOME;m.u.outcome=p;
 uint8_t bytes[RC_MAX_DGRAM];int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==176&&rc_decode(bytes,n,&out)==RC_OK);
 for(int i=0;i<n;i++)CHECK(rc_decode(bytes,i,&out)!=RC_OK);
 for(int i=0;i<6;i++){m.u.outcome=p;m.u.outcome.car[i].status=7;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);}
 m.u.outcome=p;m.u.outcome.car[0].status=RM_FINISH;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.outcome=p;m.u.outcome.car[1].score=1;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.outcome=p;m.u.outcome.mode=RM_DRIFT;m.u.outcome.car[1].score=RM_MAX_SCORE+1;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 m.u.outcome=p;m.u.outcome.round=1;CHECK(rc_encode(&m,bytes,sizeof bytes)<0);
 prepare_modes();p.to=200;p.source=111;p.destination=222;p.op=RM_TERMS;
 CHECK(rc_send_outcome(&sessions[0],&p,11000000));pump(11000000);uint32_t from;rm_packet got;
 CHECK(rc_take_outcome(&sessions[1],&from,&got)&&from==100);
 CHECK(!rc_send_outcome(&sessions[1],&p,11000000));p.destination++;CHECK(!rc_send_outcome(&sessions[0],&p,11000000));
 sessions[0].peer[0].extensions.features=0;p.destination=222;CHECK(!rc_send_outcome(&sessions[0],&p,11000000));
 prepare_modes();p.mode=RM_RANK;
 CHECK(rc_send_outcome(&sessions[0],&p,11000000));pump(11000000);
 CHECK(rc_take_outcome(&sessions[1],&from,&got)&&got.mode==RM_RANK);
 sessions[0].peer[0].extensions.features&=~RC_EXT_RANK_TABLE;
 CHECK(!rc_send_outcome(&sessions[0],&p,11000000));
 sessions[0].peer[0].extensions.features|=RC_EXT_RANK_TABLE;
 sessions[1].conf.extensions&=~RC_EXT_RANK_TABLE;
 CHECK(rc_send_outcome(&sessions[0],&p,11000000));pump(11000000);
 CHECK(!rc_take_outcome(&sessions[1],&from,&got));
 prepare_modes();p.mode=RM_DRAG;sessions[0].peer[0].extensions.features&=~RC_EXT_RANK_CLOCK;
 CHECK(!rc_send_outcome(&sessions[0],&p,11000000));
 sessions[0].peer[0].extensions.features|=RC_EXT_RANK_CLOCK;sessions[1].conf.extensions&=~RC_EXT_RANK_CLOCK;
 CHECK(rc_send_outcome(&sessions[0],&p,11000000));pump(11000000);CHECK(!rc_take_outcome(&sessions[1],&from,&got));
 unsigned wins=0;
 for(unsigned seed=1;seed<=500;seed++){
  prepare_modes();rng=seed;lossy_n=0;for(int k=0;k<2;k++)sessions[k].io.send=mode_send;
  int mode=seed%3==0?RM_RANK:seed%2?RM_DRAG:RM_DRIFT;
  for(int k=0;k<2;k++)CHECK(rm_init(&outcomes[k],77,88,99,mode,0,1,4,!k));
  for(int64_t t=11000000;t<15000000;t+=50000)exchange(t);
  CHECK(rm_ready(&outcomes[0])&&rm_ready(&outcomes[1]));
  rm_value a={100000,mode==RM_DRIFT?4000u:0u,RM_FINISH,100000},b={110000,mode==RM_DRIFT?5000u:0u,RM_FINISH,110000};
  rm_value c={90000,0,seed%3?RM_BLOWN:RM_FINISH,90000},d={120000,0,RM_TOTALLED,120000};
  CHECK(!rm_set(&outcomes[0],1,&b));CHECK(!rm_set(&outcomes[1],0,&a));
  CHECK(rm_set(&outcomes[0],0,&a)&&rm_set(&outcomes[0],2,&c)&&rm_set(&outcomes[0],3,&d)&&rm_set(&outcomes[1],1,&b));
  for(int64_t t=15000000;t<25000000;t+=50000){exchange(t);fixture_resolve(&outcomes[0]);}
  if(!outcomes[0].final||!outcomes[1].final||!outcomes[0].board_ack)fprintf(stderr,"seed%u h final%d fault%d agreed%d slot1=%u seq%u guest final%d fault%d agreed%d confirmed%d seq%u\n",seed,outcomes[0].final,outcomes[0].fault,outcomes[0].peer_agreed,outcomes[0].car[1].status,outcomes[0].received_serial,outcomes[1].final,outcomes[1].fault,outcomes[1].agreed,outcomes[1].peer_agreed,outcomes[1].received_serial);
  CHECK(outcomes[0].final&&outcomes[1].final&&outcomes[0].board_ack);
  CHECK(!outcomes[0].fault&&!outcomes[1].fault);
  CHECK(!memcmp(outcomes[0].rank,outcomes[1].rank,sizeof outcomes[0].rank));
  int expected=mode==RM_DRIFT?2:seed%3?1:0;CHECK(rm_team_winner(&outcomes[0])==expected&&rm_team_winner(&outcomes[1])==expected);wins+=expected!=0;
  /* An altered final board cannot change a previously accepted result. */
  rm_packet final={0};CHECK(rm_next(&outcomes[1],26000000,&final));final.op=RM_BOARD;final.final=1;memcpy(final.car,outcomes[0].car,sizeof final.car);memcpy(final.elapsed,outcomes[0].elapsed,sizeof final.elapsed);final.car[0].ticks++;
  final.to=200;final.source=111;final.destination=222;final.serial=outcomes[1].received_serial+1;
  CHECK(!rm_receive(&outcomes[1],&final)&&outcomes[1].fault);

 }
 rm_outcome series;CHECK(rm_init(&series,77,88,99,RM_URL,0,3,3,1));series.peer_agreed=1;
 for(int round=0;round<3;round++){
  for(int k=0;k<3;k++)if(k!=1){rm_value v={(uint32_t)(100000+((k+round)%3)*10000),0,RM_FINISH,(uint32_t)(100000+((k+round)%3)*10000)};CHECK(rm_set(&series,k,&v));}
  series.car[1]=(rm_value){(uint32_t)(100000+((1+round)%3)*10000),0,RM_FINISH,(uint32_t)(100000+((1+round)%3)*10000)};
  CHECK(fixture_resolve(&series)&&rm_commit_round(&series));CHECK(!rm_commit_round(&series));
  if(round<2){CHECK(!rm_advance(&series,100+round));series.board_ack=1;CHECK(rm_advance(&series,100+round));CHECK(series.round==round+1&&!series.final);series.peer_agreed=1;}
 }
 CHECK(series.points[0]==18&&series.points[1]==18&&series.points[2]==18);CHECK(rm_team_winner(&series)==1);
 CHECK(!rm_advance(&series,111));
 rm_outcome live;CHECK(rm_init(&live,77,88,99,RM_DRAG,0,1,2,0));live.agreed=1;
 rm_packet premature={0};premature.to=premature.source=premature.destination=1;premature.run=77;premature.event=88;premature.layout=99;premature.mode=RM_DRAG;premature.rounds=1;premature.racers=2;premature.op=RM_BOARD;premature.final=1;premature.serial=1;
 premature.car[0]=(rm_value){100,0,RM_FINISH,100};premature.car[1]=(rm_value){200,0,RM_FINISH,200};premature.elapsed[0]=100;premature.elapsed[1]=200;CHECK(!rm_receive(&live,&premature)&&live.fault);
 CHECK(!rm_init(&series,77,88,99,259,0,3,3,1));CHECK(!rm_init(&series,77,88,99,RM_URL,256,3,3,1));
 CHECK(rm_init(&series,77,88,99,RM_URL,0,1,2,1));series.peer_agreed=1;series.car[0]=(rm_value){100,0,RM_TIMEOUT,100};series.car[1]=(rm_value){100,0,RM_TIMEOUT,100};
 CHECK(fixture_resolve(&series)&&rm_commit_round(&series)&&rm_team_winner(&series)==1);
 puts("PASS mode results: 500 lossy sessions, wire176, negotiated roles/nonces, ranked-table capability on send/receive, immutable owner results, Circuit/Sprint/StreetX and Drag time/DNF, Drift score/ties, final-board ACK and URL points/round barriers; no game");
 printf("checks=%u team wins=%u\n",checks,wins);return 0;
}
#endif
