/* Reuse real-core pair setup; never starts a socket or game. */
#define main previous_coop_main
#include "coop-tests.c"
#undef main
typedef struct {int to;cp_packet msg;int64_t at;} pause_delivery;
static pause_delivery deliveries[4096];static int nd;
static cp_pause pair[2];
static void deliver_pause(int64_t time,int32_t offset){
 for(int i=0;i<nd;){
  if(deliveries[i].at>time){i++;continue;}
  pause_delivery d=deliveries[i];deliveries[i]=deliveries[--nd];
  cp_receive(&pair[d.to],d.msg.op,d.msg.value,time+(d.to?offset:0),d.to?offset:-offset);
 }
}
static void simulate_pause(unsigned seed,int32_t offset,int64_t base){
 memset(pair,0,sizeof pair);nd=0;rng=seed;
 int runs=0,stops=0;int64_t release[2]={0};
 int last_held[2]={1,1};
 for(int64_t elapsed=0;elapsed<36000000;elapsed+=10000){
  int64_t time=base+elapsed;
  deliver_pause(time,offset);
  for(int k=0;k<2;k++){
   int64_t local=time+(k?offset:0);
   /* Client stops producing frames completely, then host opens pause menu. */
   if(!(k==1&&elapsed>=7000000&&elapsed<11000000))
    cp_frame(&pair[k],k?200:100,k?100:200,77,88,!k,k==0&&elapsed>=24000000&&elapsed<26000000,local);
   cp_step(&pair[k],local,1);
   int held=cp_held(&pair[k]);
   if(last_held[k]&&!held){runs++;release[k]=time;}
   if(!last_held[k]&&held)stops++;
   last_held[k]=held;
   if(elapsed>=8200000&&elapsed<11000000)CHECK(held);
   if(elapsed>=25200000&&elapsed<26000000)CHECK(held);
   cp_packet out[2];int n=cp_packets(&pair[k],local,out);
   for(int j=0;j<n;j++){
    if(random32()%100<20)continue;
    CHECK(nd<4095);deliveries[nd++]=(pause_delivery){1-k,out[j],time+10000+(random32()%70000)};
   }
  }
 }
 CHECK(!cp_held(&pair[0])&&!cp_held(&pair[1]));
 if(runs<6||stops<4)printf("seed %u runs %d stops %d final release %lld %lld\n",seed,runs,stops,(long long)release[0],(long long)release[1]);
 CHECK(runs>=6&&stops>=4);
 CHECK(llabs(release[0]-release[1])<=20000);
 /* Delayed old generation/sequence cannot restart or unpause a new hold. */
 cp_packet old={CP_GO,pair[0].proposal};int64_t now=base+36000000;
 cp_frame(&pair[0],100,200,77,88,1,1,now);cp_step(&pair[0],now,1);
 cp_packet out[2];CHECK(cp_packets(&pair[0],now,out)==2&&out[1].op==CP_HOLD);
 cp_receive(&pair[1],CP_HOLD,out[1].value,now+offset,offset);
 cp_receive(&pair[1],old.op,old.value,now+offset,offset);CHECK(cp_held(&pair[1]));
}
static void real_core_pause(void){
 connect_pair(1);sessions[0].conf.race_pause=sessions[1].conf.race_pause=1;
 for(int64_t t=7100000;t<11000000;t+=10000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(sessions[0].peer[0].race_pause&&sessions[1].peer[0].race_pause);
 for(int64_t t=11000000;t<31000000;t+=10000){
  for(int k=0;k<2;k++){
   if(!((k==0&&t>=16000000&&t<21000000)||(k==1&&t>=18000000&&t<23000000)))cp_frame(&sessions[k].pause,k?200:100,k?100:200,123,456,!k,0,t);
   rc_tick(&sessions[k],t);
  }
  pump(t);
  if(t>=17000000&&t<23000000)CHECK(cp_held(&sessions[0].pause)&&cp_held(&sessions[1].pause));
 }
 CHECK(!cp_held(&sessions[0].pause)&&!cp_held(&sessions[1].pause));
 CHECK(!sessions[0].n_coop_in&&!sessions[1].n_coop_in); /* no unbounded paused-game FIFO */
 cp_pause before=sessions[1].pause;
 rc_coop_msg m={RC_COOP_PAUSE_HOLD,200,999,456,0x77770000};
 CHECK(rc_send_coop(&sessions[0],&m));pump(31000000);CHECK(!memcmp(&before,&sessions[1].pause,sizeof before));
 sessions[0].conf.race_pause=0;CHECK(!rc_send_coop(&sessions[0],&m));
}
static void wraps_and_roles(void){
 cp_pause h={0},g={0};int64_t now=5000000;
 cp_frame(&h,100,200,77,88,1,0,now);cp_frame(&g,200,100,77,88,0,0,now);
 h.generation=g.generation=65535;h.phase=g.phase=CP_RUNNING;
 h.blocked=1;cp_step(&h,now,1);CHECK(h.generation==1&&cp_held(&h));
 cp_packet p[2];CHECK(cp_packets(&h,now,p)==2&&p[1].op==CP_HOLD);
 cp_receive(&g,p[1].op,p[1].value,now,0);CHECK(g.generation==1&&cp_held(&g));
 cp_receive(&g,CP_GO,0xffff0000u|5000,now,0);CHECK(g.generation==1&&cp_held(&g));
 cp_receive(&h,CP_STATE,0xfffffffcu,now,0);CHECK(h.remote_seen);
 cp_receive(&h,CP_STATE,0,now+1,0);CHECK(h.remote_seq==0&&h.peer_at==now+1);
 cp_receive(&h,CP_STATE,0xfffffffcu,now+2,0);CHECK(h.remote_seq==0&&h.peer_at==now+1);
 cp_receive(&h,CP_STATE,1,now+3,0);CHECK(h.peer_at==now+1); /* same revision cannot change flags */
 cp_pause before=h;cp_receive(&h,CP_GO,0x12340000,now,0);CHECK(!memcmp(&before,&h,sizeof h));
}
static void short_hitch(void){
 cp_pause h={0};int64_t now=5000000;
 cp_frame(&h,100,200,77,88,1,0,now);h.phase=CP_RUNNING;
 h.remote_seen=1;h.remote_flags=0;h.peer_at=now;
 cp_step(&h,now+300000,1);CHECK(!cp_held(&h));
 h.peer_at=now+700000;cp_step(&h,now+749999,1);CHECK(!cp_held(&h));
 cp_step(&h,now+750000,1);CHECK(cp_held(&h)&&(h.hold_reason&8));
 CHECK(h.hold_frame_age==750000);
 cp_frame(&h,100,200,77,88,1,1,now+760000);h.phase=CP_RUNNING;
 cp_step(&h,now+760000,1);CHECK(cp_held(&h)&&(h.hold_reason&2));
}
static void independent_clocks(void){
 /* Two real PCs expose only modulo-2^32 microsecond clock offsets.
  * Different uptimes, including a uint32 wrap during the resume window. */
 const int64_t hosts[]={44485300000LL,4294900000LL,10000000LL};
 const int64_t guests[]={900000000LL,17179800000LL,98765432100LL};
 for(int i=0;i<3;i++){
  cp_pause h={0},g={0};int64_t hb=hosts[i],gb=guests[i];
  int32_t offset=(int32_t)((uint32_t)gb-(uint32_t)hb);
  for(int64_t dt=0;dt<5000000;dt+=10000){
   cp_frame(&h,100,200,77,88,1,0,hb+dt);cp_frame(&g,200,100,77,88,0,0,gb+dt);
   cp_step(&h,hb+dt,1);cp_step(&g,gb+dt,1);
   cp_packet out[2];int n=cp_packets(&h,hb+dt,out);
   for(int k=0;k<n;k++)cp_receive(&g,out[k].op,out[k].value,gb+dt,offset);
   n=cp_packets(&g,gb+dt,out);
   for(int k=0;k<n;k++)cp_receive(&h,out[k].op,out[k].value,hb+dt,-offset);
  }
  CHECK(!cp_held(&h)&&!cp_held(&g));
  CHECK(llabs((h.release_at-hb)-(g.release_at-gb))<2000);
 }
}
int main(void){
 independent_clocks();
 for(unsigned seed=1;seed<=100;seed++)simulate_pause(seed,seed%2?456789000:-12345000,seed%3?100000000:65500000);
 real_core_pause();wraps_and_roles();short_hitch();
 printf("PASS race pause: %u checks; 100 lossy/reordered pause/resume sessions, source/host frame loss, menu pause, clock offsets/16-bit ms wrap, old-generation rejection and real-core integration\n",checks);return 0;
}
