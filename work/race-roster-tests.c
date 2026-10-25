/* Frozen AI roster wire/state/session checks. No engine or socket. */
#define main previous_coop_main
#include "coop-tests.c"
#undef main
static ar_roster host,guest;
static uint8_t cfg_bytes[4][AR_CFG_MAX];static int lengths[4];
static uint32_t proofs[4];
static void init_roster(unsigned seed,int racers){
 rng=seed;ar_init(&host,100,200,seed,88,1,racers);ar_init(&guest,200,100,seed,88,0,racers);
 for(int k=2;k<racers;k++){
  rc_carcfg cfg={0};cfg.type_hash=100u+k;snprintf(cfg.type_name,sizeof cfg.type_name,"BOT%d",k);
  cfg.has_perf=1;memset(cfg.perf+11,100,41);cfg.ride_b[3]=1;
  cfg.n_parts=(uint8_t)(seed%3==0?170:seed%3==1?100:2);
  for(int j=0;j<cfg.n_parts;j++){cfg.part[j].slot=(uint8_t)j;cfg.part[j].key=random32();cfg.part[j].owner=cfg.type_hash;}
  lengths[k-2]=rc_carcfg_encode(&cfg,cfg_bytes[k-2],AR_CFG_MAX);CHECK(lengths[k-2]>0);
  proofs[k-2]=random32()|1;CHECK(ar_host_set(&host,k,cfg_bytes[k-2],lengths[k-2],proofs[k-2]));
 }
 CHECK(ar_seal(&host));CHECK(!ar_ready(&host)&&!ar_ready(&guest));
}
static void simulate_roster(unsigned seed,int racers){
 init_roster(seed,racers);
 struct {int to,n;int64_t due;uint8_t bytes[RC_MAX_DGRAM];} q[256];int nq=0;
 int64_t load_at[6]={0};int premature_ack=0;
 for(int64_t now=1;now<40000000;now+=10000){
  for(int i=0;i<nq;){
   if(q[i].due>now){i++;continue;}
   rc_msg msg;CHECK(rc_decode(q[i].bytes,q[i].n,&msg)==RC_OK);int to=q[i].to;
   if(msg.h.type==RC_MSG_RACE_CFG){CHECK(to==1);CHECK(ar_receive(&guest,100,&msg.u.race_cfg)>=0);}
   else {ar_roster *a=to?&guest:&host;CHECK(ar_control(a,to?100:200,msg.u.coop.op,msg.u.coop.to,msg.u.coop.run,msg.u.coop.event,msg.u.coop.value)>=0);}
   q[i]=q[--nq];
  }
  for(int k=2;k<racers;k++)if(guest.car[k-2].complete&&guest.manifest_seen){
   if(!load_at[k])load_at[k]=now+100000+(random32()%800000);
   if(now>=load_at[k])CHECK(ar_loaded(&guest,k,host.car[k-2].digest,host.car[k-2].physics));
  }
  for(int from=0;from<2;from++){
   ar_roster *a=from?&guest:&host;rc_msg messages[2];int n=0;memset(messages,0,sizeof messages);
   ar_fragment f;if(ar_next_fragment(a,now,&f)){messages[n].h.type=RC_MSG_RACE_CFG;messages[n++].u.race_cfg=f;}
   uint8_t op;uint32_t value;
   if(ar_next_control(a,now,&op,&value)){
    if(op==AR_ACK&&!ar_ready(&guest))premature_ack++;
    messages[n].h.type=RC_MSG_COOP;messages[n++].u.coop=(rc_coop_msg){op,from?100:200,seed,88,value};
   }
   for(int j=0;j<n;j++){
    messages[j].h.sender=from?200:100;uint8_t bytes[RC_MAX_DGRAM];int len=rc_encode(&messages[j],bytes,sizeof bytes);CHECK(len>0&&len<=1052);
    if(random32()%100<35)continue;
    int copies=random32()%5==0?2:1;
    while(copies--){CHECK(nq<256);q[nq].to=1-from;q[nq].due=now+10000+(random32()%150000);q[nq].n=len;memcpy(q[nq++].bytes,bytes,(unsigned)len);}
   }
  }
  CHECK(!host.fault&&!guest.fault&&!premature_ack);
  if(ar_ready(&host)&&ar_ready(&guest)){
   for(int k=2;k<racers;k++)CHECK(guest.car[k-2].length==lengths[k-2]&&!memcmp(guest.car[k-2].bytes,cfg_bytes[k-2],(unsigned)lengths[k-2]));
   return;
  }
 }
 CHECK(0);
}
static void wire_and_guards(void){
 init_roster(3,6);ar_fragment f;CHECK(ar_next_fragment(&host,1,&f));
 rc_msg m={0},out;m.h.type=RC_MSG_RACE_CFG;m.h.sender=100;m.u.race_cfg=f;
 uint8_t bytes[RC_MAX_DGRAM];int n=rc_encode(&m,bytes,sizeof bytes);CHECK(n==1052);
 CHECK(rc_decode(bytes,n,&out)==RC_OK&&out.u.race_cfg.physics==f.physics&&out.u.race_cfg.slot==2);
 for(int len=0;len<n;len++)CHECK(rc_decode(bytes,len,&out)!=RC_OK);
 ar_roster before=guest;f.to=999;CHECK(!ar_receive(&guest,100,&f));CHECK(!memcmp(&before,&guest,sizeof guest));f.to=200;
 CHECK(!ar_receive(&guest,999,&f));f.run++;CHECK(!ar_receive(&guest,100,&f));f.run--;
 CHECK(!ar_receive(&host,200,&f));CHECK(ar_receive(&guest,100,&f)==1);
 uint8_t corrupt[AR_FRAGMENT];memcpy(corrupt,f.data,f.dlen);corrupt[0]^=1;f.data=corrupt;
 CHECK(ar_receive(&guest,100,&f)==-1&&guest.fault);
 init_roster(4,3);CHECK(ar_next_fragment(&host,1,&f));f.physics=0;CHECK(!ar_fragment_valid(&f));f.physics=proofs[0];
 f.slot=0;CHECK(!ar_fragment_valid(&f));f.slot=2;f.dlen--;CHECK(!ar_fragment_valid(&f));f.dlen++;
 CHECK(ar_control(&guest,100,AR_MANIFEST,200,4,88,host.manifest)==1);
 for(int64_t t=1;!guest.car[0].complete;t+=100000){if(ar_next_fragment(&host,t,&f))CHECK(ar_receive(&guest,100,&f)==1);}
 CHECK(!ar_ready(&guest));CHECK(!ar_loaded(&guest,2,guest.car[0].digest,guest.car[0].physics^1)&&guest.fault);
 init_roster(5,2);CHECK(!ar_next_fragment(&host,1,&f));CHECK(ar_control(&guest,100,AR_MANIFEST,200,5,88,host.manifest)==1&&ar_ready(&guest));
 CHECK(!ar_control(&host,200,AR_ACK,100,999,88,host.manifest));CHECK(!ar_control(&host,200,AR_ACK,100,5,88,host.manifest^1));
 CHECK(ar_control(&host,200,AR_ACK,100,5,88,host.manifest)==1&&ar_ready(&host));
}
static void session_roster(void){
 connect_pair(1);sessions[0].conf.race_roster=sessions[1].conf.race_roster=1;
 for(int64_t t=7100000;t<11000000;t+=50000){rc_tick(&sessions[0],t);rc_tick(&sessions[1],t);pump(t);}
 CHECK(sessions[0].peer[0].race_roster&&sessions[1].peer[0].race_roster);
 init_roster(77,3);sessions[0].roster=host;sessions[1].roster=guest;
 const uint8_t owner[]={1,2,3,4};memcpy(sessions[0].cfg,owner,sizeof owner);sessions[0].cfg_len=sizeof owner;
 rc_coop_msg manifest={AR_MANIFEST,200,77,88,host.manifest};CHECK(rc_send_coop(&sessions[0],&manifest));pump(11000000);
 CHECK(sessions[1].roster.manifest_seen&&!sessions[1].n_coop_in);
 ar_fragment f;
 for(int64_t t=11000000;!sessions[1].roster.car[0].complete;t+=100000){CHECK(ar_next_fragment(&sessions[0].roster,t,&f));CHECK(rc_send_race_cfg(&sessions[0],&f));pump(t);}
 CHECK(sessions[0].cfg_len==sizeof owner&&!memcmp(sessions[0].cfg,owner,sizeof owner));
 CHECK(ar_loaded(&sessions[1].roster,2,host.car[0].digest,host.car[0].physics));
 uint8_t op;uint32_t value;CHECK(ar_next_control(&sessions[1].roster,12000000,&op,&value));
 rc_coop_msg ack={op,100,77,88,value};CHECK(rc_send_coop(&sessions[1],&ack));pump(12000000);CHECK(ar_ready(&sessions[0].roster));
 sessions[0].conf.race_roster=0;CHECK(!rc_send_race_cfg(&sessions[0],&f)&&!rc_send_coop(&sessions[0],&manifest));
}
int main(void){
 for(unsigned seed=1;seed<=500;seed++)simulate_roster(seed,2+(int)(seed%5));
 wire_and_guards();session_roster();
 printf("PASS AI roster: %u checks; 500 frozen rosters with 35%% loss, duplication/reordering, asynchronous attach proofs, canonical models, wire bounds, authority/context/mutation guards and independent human CFG\n",checks);return 0;
}
