/* Native result adapter. Only the game thread reads these objects. */
#include "native_downhill.inc"
#include "race_clock.h"
#ifdef NATIVE_ENGINE_LAB
void roam_plat_lab_yield(uint32_t car);
#endif
static struct {
 rm_outcome outcome;uint32_t world,race,peer,source,destination;
 rk_clock clock;
 int active,applied,applying,finishing,released,prize_seen,downhill;uint32_t finish_tick,prize,attempted,committed;
 int64_t between_since,old_ack_next;rm_packet old_ack;int old_ack_valid;
} NM;
#include "native_race_clock.inc"
static int nm_mode(uint32_t event){
 if(!ok(event,0x88))return 0;
 int mode=nc_event_mode(event);if(mode)return mode;
 return MR8(event+0xf)<=2?RM_RANK:0;
}
static int nm_peer(uint32_t id,int64_t now,uint32_t *nonce){
 int ready=0;roam_plat_lock();
 /* Native vehicle assembly can take a whole frame. The receive thread may
  * renew the lease while it runs; compare against a clock read under lock. */
 now=roam_plat_now_us();
 for(int k=0;k<RC_TABLE;k++){
  rc_peer *p=&g_s.peer[k];
  if(p->id==id&&rc_extension_ready(&g_s,p,RC_EXT_RACE_MODES|RC_EXT_RANK_CLOCK|RC_EXT_PAUSE_CLOCK,now)&&
    (!!(g_s.conf.extensions&RC_EXT_CONTACT_HOST)==!!NC.host)&&
    (!!(p->extensions.features&RC_EXT_CONTACT_HOST)!=!!NC.host)){
   *nonce=p->extensions.nonce;ready=1;break;
  }
 }
 roam_plat_unlock();return ready;
}
static void nm_reset(void){memset(&NM,0,sizeof NM);if(!nmd_base())memset(&NMD,0,sizeof NMD);}
static int nm_scoring(uint32_t car,uint32_t record,uint32_t *style){
 uint32_t manager=rd32(record+0x1d4),first=rd32(manager);
 if(!ok(manager,0x9c)||!ok(first,0x54)||rd32(record)!=car)return 0;
 float score=MRF32(first+0x2c);
 if(!__builtin_isfinite(score)||score<0||score>(float)RM_MAX_SCORE)return 0;
 *style=first;return 1;
}
static uint32_t nm_layout(uint32_t event){
 /* Freeze native route/direction/laps and the career event's mode settings.
  * No pointers, clock fields, equipment or localized names enter the hash. */
 uint8_t b[64];rc_w w;rc_w_init(&w,b,sizeof b);
 rc_put32(&w,MR32(event+8));rc_put8(&w,MR8(event+0xf));
 for(int k=0;k<4;k++)rc_put32(&w,rd32(0x89e7a0+4*k));
 rc_put32(&w,rd32(0x89e7bc));rc_put8(&w,MR8(0x89e7d8));rc_put8(&w,MR8(0x89e7d9));
 rc_put8(&w,MR8(event+0x7e));rc_putb(&w,(void*)(uintptr_t)(event+0x18),16);
 return rc_crc32(b,w.n);
}
static int nm_import_scores(int all){
 rm_outcome *s=&NM.outcome;uint32_t styles[RM_CARS]={0};
 /* Validate the entire write set first; never partially replace a board. */
 for(int k=0;k<NC.stream.racers;k++)if(all||(NC.host?k==1:k!=1))
  if(!nm_scoring(NC.car[k],NC.record[k],&styles[k]))return 0;
 uint32_t virtuals[3];if(NM.downhill&&!nmd_entries(virtuals))return 0;
 for(int k=0;k<NC.stream.racers;k++)if(styles[k]){
  MRF32(styles[k]+0x2c)=(float)s->car[k].score;MRF32(styles[k]+0x24)=0;MR32(styles[k]+0x14)=2;
 }
 if(NM.downhill)for(int k=0;k<3;k++)MRF32(virtuals[k]+0x10)=(float)s->car[NC.stream.racers+k].score;
 ni_style_board();return 1;
}
static int nm_url_round(uint32_t event,int *round,int *rounds){
 const uint32_t series=0x85ad40+0xd24,first=series+0x54;
 uint32_t count=rd32(series+0x34e8),current=rd32(series+0x34e4);
 if(count<1||count>RM_ROUNDS||count!=MR8(event+0x7e)||current<first||
    (current-first)%0xd24||(current-first)/0xd24>=count||!ok(current,0xd24))return 0;
 *round=(int)((current-first)/0xd24);*rounds=(int)count;
 return rd32(current)==rd32(0x89e7a0)&&rd32(current+4)==rd32(0x89e7a4)&&rd32(current+8)==rd32(0x89e7bc);
}
static uint32_t nm_next_intro_run(uint32_t race){
 uint32_t event=rd32(G_CAREER_RACE);int round=0,rounds=0;
 if(NC.control.phase!=CC_RUNNING||!NM.active||NM.outcome.mode!=RM_URL||!NM.applied||
    race!=rd32(G_RACE)||!ok(event,0x88)||MR32(event+8)!=NM.outcome.event||
    !nm_url_round(event,&round,&rounds)||round!=NM.outcome.round+1||rounds!=NM.outcome.rounds)return 0;
 uint32_t run=NM.outcome.run+1;return run?run:1;
}
static int nm_url_ids(void){
 int own=NC.host?0:1;
 for(int k=0;k<NC.stream.racers;k++){
  uint32_t id=k==own?0:k<2?100:(uint32_t)(99+k);
  if(!ok(NC.car[k],0x9e0)||MR16(NC.car[k]+0xa)!=id)return 0;
 }
 return 1;
}
static void nm_retry_old_ack(int64_t now){
 if(!NM.old_ack_valid||now<NM.old_ack_next)return;
 NM.old_ack_next=now+100000;NM.old_ack.serial++;
 roam_plat_lock();rc_send_outcome(&g_s,&NM.old_ack,now);roam_plat_unlock();
}
static int nm_url_apply(CPU *,int,int64_t);
static int nm_rank_apply(CPU *);
static int nm_reject(int line){
 rlog("[race-mode] validation failed line=%d phase=%d active=%d fault=%d race=%08x/%08x world=%08x/%08x layout=%08x/%08x\n",
 line,NC.control.phase,NM.active,NM.outcome.fault,NC.race,NM.race,world(),NM.world,
 nm_layout(rd32(G_CAREER_RACE)),NM.outcome.layout);
 return -1;
}
static int nm_frame(CPU *c,int64_t now){
 uint32_t event=rd32(G_CAREER_RACE);int mode=nm_mode(event);
 if(!mode){if(NM.active)return nm_reject(__LINE__);return 1;}
 uint32_t nonce=0;
 if(!NC.bound||!nc_same_world())return nm_reject(__LINE__);
 if(NC.host&&NC.control.phase>=CC_RUNNING&&NC.control.phase<=CC_DONE){
  uint32_t wall;if(!nm_wall(&wall)||!rk_step(&NM.clock,wall,nc_pause_held()))return nm_reject(__LINE__);
 }
 /* A temporarily stale extension lease is a closed loading barrier, not an
  * incompatible race. Control timeouts still bound a disconnected peer. */
 if(!nm_peer(NC.peer,now,&nonce))return 0;
 if(mode==RM_RANK){
  int supported=0;roam_plat_lock();now=roam_plat_now_us();
  for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].id==NC.peer&&rc_extension_ready(&g_s,&g_s.peer[i],RC_EXT_RANK_TABLE|RC_EXT_RANK_CLOCK,now))supported=1;
  roam_plat_unlock();if(!supported)return nm_reject(__LINE__);
 }
 if(!NM.active){
  if(NC.control.phase!=CC_LOADING)return nm_reject(__LINE__);
  NM.downhill=mode==RM_DRIFT&&rd32(0x89e7a0)>=0x104e&&rd32(0x89e7a0)<=0x1053;
  uint32_t virtuals[3];
  if(NM.downhill){if(!nm_downhill_grid()||NC.stream.racers!=2)return nm_reject(__LINE__);if(!nmd_entries(virtuals))return 0;}
  uint32_t layout=nm_layout(event);int round=0,rounds=1;
  if(mode==RM_URL&&(!nm_url_round(event,&round,&rounds)||round||!nm_url_ids()))return nm_reject(__LINE__);
  if(!rm_init(&NM.outcome,NC.control.run,NC.control.event,layout,mode,round,rounds,NC.stream.racers+(NM.downhill?3:0),NC.host))return nm_reject(__LINE__);
  if(NM.downhill&&NC.host)for(int k=0;k<3;k++){
   rm_value v={0,(uint32_t)MRF32(virtuals[k]+0x10),RM_FINISH};if(!rm_set(&NM.outcome,NC.stream.racers+k,&v))return nm_reject(__LINE__);
  }
  NM.world=world();NM.race=NC.race;NM.peer=NC.peer;NM.source=g_s.conf.extension_nonce;NM.destination=nonce;NM.active=1;
  rlog("[race-mode] mode %d route %08x; waiting for matching rules\n",mode,layout);
 }
 rm_outcome *s=&NM.outcome;
 if(rd32(NC.race+0x24)!=(uint32_t)NC.stream.racers||s->racers!=NC.stream.racers+(NM.downhill?3:0))return nm_reject(__LINE__);
 for(int k=0;k<NC.stream.racers;k++)if(!ok(NC.record[k],0x1f0)||rd32(NC.record[k])!=NC.car[k])return nm_reject(__LINE__);
 if(NM.world!=world()||NM.race!=NC.race||NM.peer!=NC.peer||NM.source!=g_s.conf.extension_nonce||
    NM.destination!=nonce||s->run!=NC.control.run||s->event!=NC.control.event||s->layout!=nm_layout(event))return nm_reject(__LINE__);
 if(mode==RM_URL){
  int round=0,rounds=0;if(!nm_url_round(event,&round,&rounds)||round!=s->round||rounds!=s->rounds||!nm_url_ids())return nm_reject(__LINE__);
  if(NR_REWARD.terms){if(NM.prize_seen&&NR_REWARD.amount!=NM.prize)return nm_reject(__LINE__);NM.prize=NR_REWARD.amount;NM.prize_seen=1;}
 }
 nm_retry_old_ack(now);
 uint32_t from;rm_packet p;
 for(int k=0;k<RM_INBOX;k++){
  roam_plat_lock();int got=rc_take_outcome(&g_s,&from,&p);roam_plat_unlock();if(!got)break;
  if(from==NC.peer&&p.source==NM.destination&&p.destination==NM.source){
   if(rm_receive(s,&p)&&!NC.host&&(p.op==RM_TERMS||p.op==RM_BOARD))NM.old_ack_valid=0;
  }
 }
 if(s->fault)return nm_reject(__LINE__);
 for(int k=0;k<NC.stream.racers;k++)if(s->car[k].status&&!s->car[k].wall)return nm_reject(__LINE__);
 if(NC.control.phase==CC_RUNNING&&!nc_pause_held()&&!s->final){
  for(int k=0;k<NC.stream.racers;k++)if((NC.host?k!=1:k==1)&&!s->car[k].status){
   rm_value v={MR32(NC.race+0x14),0,RM_LIVE};
   if(mode==RM_DRIFT){uint32_t style=0;if(!nm_scoring(NC.car[k],NC.record[k],&style))return nm_reject(__LINE__);v.score=(uint32_t)MRF32(style+0x2c);}
   if(!rm_set(s,k,&v))return nm_reject(__LINE__);
  }
  int terminal=0;for(int k=0;k<NC.stream.racers;k++)terminal|=s->car[k].status!=0;
  uint32_t tick=MR32(NC.race+0x14);
  if(terminal&&!NM.finishing){NM.finishing=1;NM.finish_tick=tick;}
  /* Native FinishRacer is deferred, so its ordinary first-finisher timer
   * has not started. Bound the wait in shared simulation time, excluding
   * pauses; each machine may time out only its owned participants. */
  if(NM.finishing&&(uint32_t)(tick-NM.finish_tick)>=240000u){
   for(int k=0;k<NC.stream.racers;k++)if((NC.host?k!=1:k==1)&&!s->car[k].status){
    rm_value v={tick,s->car[k].score,RM_TIMEOUT};
    if(!nm_wall(&v.wall))return nm_reject(__LINE__);
    if(!rm_set(s,k,&v))return nm_reject(__LINE__);
   }
  }
  if(NC.host){
   if(s->car[0].status&&s->car[1].status){
    uint32_t wall;if(!nm_wall(&wall)||rm_close_ai(s,tick,wall)<0)return nm_reject(__LINE__);
   }
   if(nm_shared_timing())rm_resolve(s);
  }
 }
 if(mode==RM_URL&&s->final&&s->rounds_committed==s->round&&!rm_commit_round(s))return nm_reject(__LINE__);
 if(mode==RM_DRIFT&&NC.control.phase>=CC_RUNNING&&!NM.applied&&rm_ready(s)&&!nm_import_scores(0))return nm_reject(__LINE__);
 if(rm_next(s,now,&p)){
  p.to=NC.peer;p.source=NM.source;p.destination=NM.destination;
  roam_plat_lock();rc_send_outcome(&g_s,&p,now);roam_plat_unlock();
 }
 /* A terminal owner report is immutable; dropped packets are retried as
  * part of the board until ACK, not interpreted as another finish. */
 if(s->final&&NC.control.phase==CC_RUNNING){
  if(mode==RM_URL&&s->round+1<s->rounds){
   if(!NM.applied&&(!NC.host||s->board_ack)&&nm_url_apply(c,0,now)<0)return nm_reject(__LINE__);
   return rm_ready(s);
  }
  int winner=rm_team_winner(s);
  if(NC.host&&s->board_ack){
   if(winner)cc_confirm_finish(&NC.control,winner==1?g_s.conf.self_id:NC.peer,1,now);
   else cc_bot_won(&NC.control,now);
  }
 }
 /* Final native application is driven by CC_RESULT on both machines. */
 (void)c;return rm_ready(s);
}
int nm_finish(uint32_t race,uint32_t index,uint32_t position,uint32_t flag,uint32_t completion){
 (void)position;(void)flag;
 if(!NC.enabled||!NC.bound||race!=NC.race||!nc_same_world()||!nm_mode(rd32(G_CAREER_RACE)))return 0;
 if(NM.applying)return 1;
 if(NM.applied||NC.result_applied)return 0;
 if(NC.control.phase!=CC_RUNNING||!NM.active||!rm_ready(&NM.outcome))return 1;
 if(index>=6)return 1;
 uint32_t record=rd32(race+0xbd0+4*index);int slot=-1;
 for(int k=0;k<NC.stream.racers;k++)if(NC.record[k]==record){slot=k;break;}
 if(slot<0||(NC.host?slot==1:slot!=1))return 1;
 rm_outcome *s=&NM.outcome;if(s->car[slot].status)return 1;
 rm_value v={MR32(race+0x14),0,(uint8_t)completion};
 /* All race modes use the same countdown epoch and pause ledger. */
 if(!nm_wall(&v.wall)){cc_cancel(&NC.control,CC_BAD_CLOCK,roam_plat_now_us());return 1;}
 if(completion!=RM_FINISH&&completion!=RM_TOTALLED&&completion!=RM_BLOWN&&completion!=RM_TIMEOUT){cc_cancel(&NC.control,CC_UNSUPPORTED,roam_plat_now_us());return 1;}
 if(s->mode==RM_DRIFT){
  uint32_t style=0;if(!nm_scoring(NC.car[slot],record,&style)){cc_cancel(&NC.control,CC_ENGINE_ERROR,roam_plat_now_us());return 1;}
  /* Same native flush used by FinishRacer before it reads the score.
   * Flush once, then freeze the real owner's total. */
  ni_style_finalize(rd32(record+0x1d4));
  if(!nm_scoring(NC.car[slot],record,&style)){cc_cancel(&NC.control,CC_ENGINE_ERROR,roam_plat_now_us());return 1;}
  v.score=(uint32_t)MRF32(style+0x2c);
 }
 if(!rm_set(s,slot,&v))cc_cancel(&NC.control,CC_ENGINE_ERROR,roam_plat_now_us());
#ifdef NATIVE_ENGINE_LAB
 /* A lab AI driver must yield before native finish classifies the human.
  * This callback is absent from production and never touches remote cars. */
 if(slot==(NC.host?0:1)){
  roam_plat_lab_yield(NC.car[slot]);
 }
#endif
 rlog("[race-mode] frozen owner slot %d completion %u ticks %u score %u wall %u\n",slot,completion,v.ticks,v.score,v.wall);
 return 1;
}
static int nm_result_ready(void){
 return NM.active&&NM.outcome.final&&!NM.outcome.fault&&!!NC.control.winner==!!rm_team_winner(&NM.outcome);
}
/* A native finish may defer completion. Remember which records we own,
 * retry unfinished ones, and never call FinishRacer twice on a committed
 * opponent. Find its current index again: native calls can reorder it. */
static int nm_commit_car(CPU *c,int k,unsigned position,unsigned status){
 uint32_t rec=NC.record[k],bit=1u<<k;int index=-1;
 if(!nc_same_world()||!ok(rec,0x1f0)||rd32(rec)!=NC.car[k])return -1;
 for(int i=0;i<NC.stream.racers;i++)if(rd32(NC.race+0xbd0+4*i)==rec){index=i;break;}
 if(index<0)return -1;
 if(MR16(rec+0x10)&&!(NM.attempted&bit))return -1;
 if(!MR16(rec+0x10)){
  if(NM.committed&bit)return -1;
  uint32_t args[5]={NC.race,(uint32_t)index,position,0,status};
  NM.attempted|=bit;NM.applying=1;
  rh_call(c,(uint32_t)(uintptr_t)nc_original_finish,0,0,5,args);
  NM.applying=0;
  if(!nc_same_world()||!ok(rec,0x1f0)||NC.record[k]!=rec||rd32(rec)!=NC.car[k])return -1;
  if(!MR16(rec+0x10))return 0;
 }
 NM.committed|=bit;return 1;
}
static int nm_apply_result(CPU *c){
 if(!NM.active)return 0;
 rm_outcome *s=&NM.outcome;
 if(!s->final||s->fault||!nc_same_world())return -1;
 if(NM.applied)return 1;
 if(NM.applying)return 0;
 int team=rm_team_winner(s),own=NC.host?0:1;
 if(s->mode==RM_URL)return nm_url_apply(c,1,roam_plat_now_us());
 if(s->mode==RM_RANK)return nm_rank_apply(c);
 if(!!NC.control.winner!=!!team)return -1;
 /* Native drift results consult the live board, not racer +140/+194.
  * Copy only the agreed frozen values, clear pending drift and refresh the
  * ordinary board before native completion. No invented points. */
 if(s->mode==RM_DRIFT&&!nm_import_scores(1))return -1;
 /* Cooperative local career outcome: a human team win is a win for each
  * profile, while the separate authoritative board keeps individual ranks. */
 uint32_t rec=NC.record[own];
 for(unsigned i=0;i<(unsigned)NC.stream.racers;i++)if(rd32(NC.race+0xbd0+4*i)==rec){
  int committed=nm_commit_car(c,own,team?1u:(uint32_t)s->rank[own],team?1u:3u);
  if(committed<=0)return committed;
  MR32(rec+0x198)=s->elapsed[own];NM.applied=1;return 1;
 }
 return -1;
}

/* Finish real participants only after both owners' immutable records and
 * the host's AI board are agreed. A team win advances each local career,
 * while the displayed native ranks/times remain the actual frozen board. */
static int nm_rank_apply(CPU *c){
 rm_outcome *s=&NM.outcome;int own=NC.host?0:1,indices[RM_CARS];
 if(s->mode!=RM_RANK||!s->final||s->fault||!nc_same_world()||
    !!NC.control.winner!=!!rm_team_winner(s))return -1;
 for(int k=0;k<s->racers;k++){
  uint32_t rec=NC.record[k];indices[k]=-1;
  if(!ok(rec,0x1f0)||rd32(rec)!=NC.car[k]||(MR16(rec+0x10)&&!(NM.attempted&(1u<<k))))return -1;
  for(int i=0;i<s->racers;i++)if(rd32(NC.race+0xbd0+4*i)==rec){indices[k]=i;break;}
  if(indices[k]<0||!s->car[k].status||!s->rank[k]||s->rank[k]>s->racers)return -1;
 }
 int team=rm_team_winner(s)!=0;
 /* Local finish opens the results frontend: populate its opponents first. */
 for(int pass=0;pass<s->racers;pass++){
  int k=pass==s->racers-1?own:pass>=own?pass+1:pass;
  uint32_t rec=NC.record[k];unsigned local_win=k==own&&team;
  int committed=nm_commit_car(c,k,local_win?1u:s->rank[k],local_win?RM_FINISH:s->car[k].status);
  if(committed<=0)return committed;
  MR8(rec+0xa)=s->rank[k];MR8(rec+0xc)=s->rank[k];
  MR16(rec+0x10)=s->car[k].status;MR32(rec+0x198)=s->elapsed[k];
 }
 NM.applied=1;ni_race_results(NC.race);
 rlog("[race-mode] agreed Circuit/Sprint/StreetX board committed; owner finish moments, common pause ledger\n");
 return 1;
}

int nm_career_win(uint32_t series){
 if(series!=0x85ad40+0xd24||!NM.active||(!NM.applied&&!NM.applying)||!NM.outcome.final||NM.outcome.fault||
    !NR_REWARD.initialized||!NR_REWARD.authorized||NR_REWARD.fault||NR_REWARD.profile!=nr_profile()||
    NM.outcome.run!=NR_REWARD.run||NM.outcome.event!=NR_REWARD.event)return -1;
 return rm_team_winner(&NM.outcome)!=0;
}

/* Withdraw ownership before restoring native physics. Preserve the frozen
 * HUMAN configuration across stages; never serialize a post-finish AI car. */
static void nm_url_release(CPU *c,int keep){
 int live=nc_same_world(),valid=NV_FROZEN.valid;NC.bound=0;
 if(live)for(int k=0;k<NC.stream.racers;k++)if(k!=(NC.host?0:1)&&ok(NC.car[k],0x9e0)){
  body_set(c,NC.car[k],0);gcall1(c,FN_CAR_SETPHYS,NC.car[k],NC.old_phys[k]);
  body_set(c,NC.car[k],NC.old_phys[k]!=0&&rd32(NC.car[k]+0x28)!=0);
 }
 nv_race_release(c);NV_FROZEN.valid=keep&&valid;NV_FROZEN.finalized=0;
 roam_plat_lock();cp_clear(&g_s.pause);roam_plat_unlock();NM.released=1;
}
static int nm_url_apply(CPU *c,int last,int64_t now){
 rm_outcome *s=&NM.outcome;
 if(NM.applied)return 1;
 if(!s->final||s->fault||s->rounds_committed!=s->round+1||!nc_same_world()||!nm_url_ids())return 0;
 if(last!=(s->round+1==s->rounds))return 0;
 /* A guest may already press Continue while its final ACK was lost. Keep
  * retrying the old ACK through the transition until the new host terms. */
 if(!NC.host&&!last){
  memset(&NM.old_ack,0,sizeof NM.old_ack);NM.old_ack.op=RM_BOARD_ACK;NM.old_ack.mode=RM_URL;
  NM.old_ack.to=NC.peer;NM.old_ack.source=NM.source;NM.old_ack.destination=NM.destination;
  NM.old_ack.run=s->run;NM.old_ack.event=s->event;NM.old_ack.layout=s->layout;NM.old_ack.serial=s->serial;
  NM.old_ack.round=(uint8_t)s->round;NM.old_ack.rounds=(uint8_t)s->rounds;NM.old_ack.racers=(uint8_t)s->racers;NM.old_ack_valid=1;NM.old_ack_next=0;
 }
 int native_index[RM_CARS];
 for(int k=0;k<s->racers;k++){
  native_index[k]=-1;uint32_t rec=NC.record[k];
  if(!ok(rec,0x1f0)||rd32(rec)!=NC.car[k]||(MR16(rec+0x10)&&!(NM.attempted&(1u<<k))))return -1;
  for(int j=0;j<s->racers;j++)if(rd32(NC.race+0xbd0+4*j)==rec){native_index[k]=j;break;}
  if(native_index[k]<0)return 0;
 }
 NM.between_since=now;
 /* Finish the local human last, so the ordinary results frontend cannot
  * snapshot a partially completed native roster. Preserve individual ranks. */
 int own=NC.host?0:1;
 for(int pass=0;pass<s->racers;pass++){
  int k=pass==s->racers-1?own:pass>=own?pass+1:pass;uint32_t rec=NC.record[k];int index=native_index[k];
  (void)index;int committed=nm_commit_car(c,k,s->rank[k],s->car[k].status);
  if(committed<=0)return committed;
  /* Native result builder515d10 reads current rank +a, completion +10,
   * elapsed +198 and stable Car+0a id. Use the agreed stage measurements. */
  MR8(rec+0xa)=s->rank[k];MR8(rec+0xc)=s->rank[k];MR32(rec+0x198)=s->elapsed[k];
 }
 NM.applied=1;ni_race_results(NC.race);
 nm_url_release(c,!last);
 rlog("[race-mode] URL stage %d/%d committed; %s\n",s->round+1,s->rounds,last?"series settled":"waiting for both next-stage loads");
 return 1;
}
int nm_can_continue(uint32_t series){
 if(series!=0x85ad40+0xd24||!NM.active||NM.outcome.mode!=RM_URL)return -1;
 return NM.applied&&!NM.outcome.fault&&NM.outcome.round+1<NM.outcome.rounds;
}
static int nm_between(CPU *c,int64_t now){
 rm_outcome *s=&NM.outcome;
 if(!NM.active||s->mode!=RM_URL||!NM.applied||s->round+1>=s->rounds)return 0;
 uint32_t nonce=0;int connected=nm_peer(NM.peer,now,&nonce)&&nonce==NM.destination;
 if(!connected||now<NM.between_since||now-NM.between_since>120000000||nc_cancel_key()){
  cc_cancel(&NC.control,connected?CC_TIMEOUT:CC_DISCONNECTED,now);nc_unbind(c);return 0;
 }
 uint32_t from;rc_coop_msg control;
 for(int k=0;k<RC_COOP_INBOX;k++){
  roam_plat_lock();int got=rc_take_coop(&g_s,&from,&control);roam_plat_unlock();if(!got)break;
  if(control.op==RC_COOP_CANCEL)cc_receive(&NC.control,from,&control,now,NC.clock_offset);
 }
 if(NC.control.phase==CC_CANCELLED){nc_unbind(c);return 0;}
 nm_retry_old_ack(now);
 uint32_t event=rd32(G_CAREER_RACE),race=rd32(G_RACE);int round=0,rounds=0;
 if(rd32(G_FLOW)!=6||!ok(event,0x88)||!ok(race,0xc44))return 1;
 int held=NCP.waiting&&NCP.race==race&&NCP.world==world()&&NCP.run==nm_next_intro_run(race);
 if(MR32(race)!=3&&!(MR32(race)<3&&held))return 1;
 /* Native allocators may reuse World/Race addresses. Require the new stage
  * descriptor and a reset native roster, rather than a changed pointer. */
 if(rd32(race+0x24)!=(uint32_t)NC.stream.racers)return 1;
 for(int k=0;k<NC.stream.racers;k++){uint32_t rec=rd32(race+0xbd0+4*k);if(!ok(rec,0x1f0)||MR16(rec+0x10))return 1;}
 if(MR32(event+8)!=s->event||!nm_url_round(event,&round,&rounds)||round!=s->round+1||rounds!=s->rounds){
  cc_cancel(&NC.control,CC_MISMATCH,now);nc_unbind(c);return 0;
 }
 /* Retired loads may have stayed pinned while the old scene owned them.
  * Retry cleanup before advancing the run, preserving the frozen own car. */
 int frozen=NV_FROZEN.valid;nv_race_release(c);NV_FROZEN.valid=frozen;
 rm_outcome next=*s;uint32_t run=s->run+1;if(!run)run=1;
 if(!rm_advance(&next,nm_layout(event))||!cc_continue_stage(&NC.control,run,now)){
  cc_cancel(&NC.control,CC_ENGINE_ERROR,now);nc_unbind(c);return 0;
 }
 next.run=run;*s=next;NM.world=world();NM.race=race;NM.applied=NM.applying=NM.released=NM.finishing=0;NM.attempted=NM.committed=0;NM.finish_tick=0;memset(&NM.clock,0,sizeof NM.clock);
 NC.loaded_noted=NC.result_applied=0;NC.next_send=0;NC.seq=0;NC.race=NC.world=0;
 memset(NC.car,0,sizeof NC.car);memset(NC.brain,0,sizeof NC.brain);memset(NC.record,0,sizeof NC.record);memset(NC.slot,0,sizeof NC.slot);
 if(NC.host)NC.next_run=run;
 NV_FROZEN.run=run;NV_FROZEN.finalized=0;memset(&NR_REWARD,0,sizeof NR_REWARD);
 rlog("[race-mode] URL stage %d/%d new run %08x; new vehicle/AI/rules/countdown barrier\n",round+1,rounds,run);
 return 0;
}
