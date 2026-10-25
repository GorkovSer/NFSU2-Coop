/* Real core datagrams/reducers, synthetic loading and balances. No engine. */
#define OUTCOME_FIXTURE_ONLY
#include "race-outcome-tests.c"
#include "reward_sync.h"
static cc_race stages[2];static rw_reward prizes[2];
static int loaded[2],released[2],settled[2];
static uint32_t balances[2];
static void series_tick(int64_t t,int final){
 mode_now=t;
 for(int k=0;k<2;k++){
  rm_packet p;if(rm_next(&outcomes[k],t,&p)){p.to=k?100:200;p.source=k?222:111;p.destination=k?111:222;rc_send_outcome(&sessions[k],&p,t);}
  rc_coop_msg m;if(rw_next(&prizes[k],t,&m))rc_send_coop(&sessions[k],&m);
  if(cc_next_packet(&stages[k],t,&m))rc_send_coop(&sessions[k],&m);
  rc_tick(&sessions[k],t);
 }
 mode_pump(t);
 for(int k=0;k<2;k++){
  uint32_t from;rm_packet p;rc_coop_msg m;
  while(rc_take_outcome(&sessions[k],&from,&p)){CHECK(from==(k?100u:200u));rm_receive(&outcomes[k],&p);}
  while(rc_take_coop(&sessions[k],&from,&m)){
   if(m.op==RC_COOP_PRIZE||m.op==RC_COOP_PRIZE_ACK)rw_receive(&prizes[k],from,&m);
   else cc_receive(&stages[k],from,&m,t,0);
  }
  if(k&&stages[k].phase==CC_INVITED)CHECK(cc_accept(&stages[k],88,t));
  if(stages[k].phase==CC_LOADING&&rm_ready(&outcomes[k])&&rw_ready(&prizes[k])&&!loaded[k]){loaded[k]=1;cc_loaded(&stages[k],t);}
  cc_step(&stages[k],t,1);
  uint32_t a=cc_take_actions(&stages[k]);released[k]+=!!(a&CC_RELEASE_GAME);
  CHECK(stages[k].phase!=CC_CANCELLED&&!outcomes[k].fault&&!prizes[k].fault);
  if(k==0)fixture_resolve(&outcomes[k]);
  if(outcomes[k].final&&!outcomes[k].rounds_committed)CHECK(rm_commit_round(&outcomes[k]));
  else if(outcomes[k].final&&outcomes[k].rounds_committed==outcomes[k].round)CHECK(rm_commit_round(&outcomes[k]));
  if(final&&k==0&&outcomes[k].final&&outcomes[k].board_ack&&stages[k].phase==CC_RUNNING){
   int win=rm_team_winner(&outcomes[k]);if(win)CHECK(cc_confirm_finish(&stages[k],win==1?100:200,1,t));else cc_bot_won(&stages[k],t);
  }
  if(final&&stages[k].phase==CC_DONE&&outcomes[k].final){
   CHECK(!!stages[k].winner==!!rm_team_winner(&outcomes[k]));CHECK(rw_authorize(&prizes[k],stages[k].winner!=0)==1);
   uint32_t after=0;int n=rw_credit(&prizes[k],k+10,balances[k],&after);CHECK(n>=0);if(n){balances[k]=after;settled[k]++;}
  }else {uint32_t after=0;CHECK(!rw_credit(&prizes[k],k+10,balances[k],&after));}
 }
}
int main(void){
 unsigned rounds_tested=0,won=0;
 for(unsigned seed=1;seed<=500;seed++){
  prepare_modes();for(int k=0;k<2;k++)sessions[k].conf.rewards=1;
  for(int64_t t=11000000;t<15000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
  rng=seed;lossy_n=0;for(int k=0;k<2;k++)sessions[k].io.send=mode_send;
  int rounds=1+seed%4,racers=2+seed%5;uint32_t run=seed%2?0xfffffffeu:77;
  balances[0]=500;balances[1]=123456;memset(settled,0,sizeof settled);memset(released,0,sizeof released);
  for(int k=0;k<2;k++){cc_init(&stages[k],k?200:100,k?100:200,!k);CHECK(rm_init(&outcomes[k],run,88,99,RM_URL,0,rounds,racers,!k));}
  /* Also crosses the 32-bit countdown clock boundary. */
  int64_t t=seed%2?0x1ffff0000LL:15000000;
  CHECK(cc_offer(&stages[0],run,88,t));
  for(int round=0;round<rounds;round++){
   memset(loaded,0,sizeof loaded);
   for(int k=0;k<2;k++)rw_init(&prizes[k],k?200:100,k?100:200,run,88,k+10,!k);
   CHECK(rw_offer(&prizes[0],2500));
   int64_t until=t+20000000;
   for(;t<until;t+=50000){series_tick(t,0);if(stages[0].phase==CC_RUNNING&&stages[1].phase==CC_RUNNING)break;}
   CHECK(t<until&&rm_ready(&outcomes[0])&&rm_ready(&outcomes[1]));
   for(int j=0;j<racers;j++){
    rm_value v={100000u+10000u*((j+round+seed)%racers),0,(seed+j+round)%7?RM_FINISH:RM_TIMEOUT,100000u+10000u*((j+round+seed)%racers)};
    CHECK(rm_set(&outcomes[j==1?1:0],j,&v));
   }
   until=t+15000000;
   for(;t<until;t+=50000){series_tick(t,round+1==rounds);if(outcomes[0].board_ack&&outcomes[1].final&&outcomes[1].rounds_committed==round+1)break;}
   CHECK(t<until&&outcomes[0].rounds_committed==round+1);
   CHECK(!memcmp(outcomes[0].points,outcomes[1].points,sizeof outcomes[0].points));rounds_tested++;
   if(round+1<rounds){
    CHECK(!settled[0]&&!settled[1]&&balances[0]==500&&balances[1]==123456);
    CHECK(stages[0].phase==CC_RUNNING&&stages[1].phase==CC_RUNNING);
    rm_packet stale={0};stale.to=200;stale.source=111;stale.destination=222;stale.run=run;stale.event=88;stale.layout=99+round;stale.op=RM_BOARD;stale.mode=RM_URL;stale.round=round;stale.rounds=rounds;stale.racers=racers;stale.serial=12345;
    uint32_t old=run;if(!++run)run=1;
    for(int k=0;k<2;k++){CHECK(rm_advance(&outcomes[k],100+round));outcomes[k].run=run;CHECK(cc_continue_stage(&stages[k],run,t));CHECK(!cc_take_actions(&stages[k]));}
    CHECK(!rm_receive(&outcomes[1],&stale)&&!outcomes[1].fault);
    rc_coop_msg late={RC_COOP_RESULT,200,old,88,100};cc_receive(&stages[1],100,&late,t,0);CHECK(stages[1].phase==CC_LOADING);
   }
   t+=50000;
  }
  int64_t until=t+8000000;
  for(;t<until;t+=50000)series_tick(t,1);
  CHECK(stages[0].phase==CC_DONE&&stages[1].phase==CC_DONE&&stages[0].result_acked);
  CHECK(settled[0]==1&&settled[1]==1&&released[0]==rounds&&released[1]==rounds);
  int win=rm_team_winner(&outcomes[0]);won+=!!win;
  CHECK(balances[0]==500+(win?2500:0)&&balances[1]==123456+(win?2500:0));
 }
 printf("PASS URL series: %u checks, 500 lossy series/%u stages, 35%% loss/duplicates/reorder, run/clock wrap, fresh countdowns, stale rejection, separate balances and one final settlement (%u team wins); no game\n",checks,rounds_tested,won);return 0;
}
