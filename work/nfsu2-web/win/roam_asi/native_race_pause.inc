/* Game-thread integration. Network thread sees only cp_pause and rc_session. */
static int nc_pause_was_held;
static int nc_pause_clock_depth;
static uint32_t nc_pause_original_flag;
/* A network tick may release cp_pause halfway through an engine frame.
 * Once stopped, keep this engine frame stopped through physics and poses. */
static uint32_t nc_pause_tick_world,nc_pause_tick_run;
static int nc_pause_tick_held,nc_pause_clock_was_held;
static int nc_pause_scope(void){
 return NC.enabled&&NC.bound&&NC.control.phase==CC_RUNNING&&nc_same_world();
}
static int nc_pause_tick_matches(void){
 return nc_pause_scope()&&nc_pause_tick_world==NC.world&&nc_pause_tick_run==NC.control.run;
}
static int nc_pause_latch(int held){
 if(nc_pause_tick_matches()){
  if(held){nc_pause_tick_held=1;nc_pause_clock_was_held=1;}
  return held||nc_pause_tick_held;
 }
 return held;
}
static int nc_pause_raw(int native_paused){
 uint32_t manager=rd32(0x8650b8),flow=rd32(0x8900d8);
 uint32_t pause_flag=nc_pause_clock_depth?nc_pause_original_flag:rd32(0x86676c);
 return native_paused||!nc_window_active()||pause_flag!=0||
        (ok(manager,0x20)&&rd32(manager+0x1c)!=0)||(ok(flow,4)&&rd32(flow)==1);
}
static int nc_pause_publish(int64_t now,int blocked){
 int held=0,capable=0,connected=0;
 roam_plat_lock();
 /* Packet receive may publish a newer peer lease during this frame. Never
  * compare that lease to a timestamp taken before acquiring the lock. */
 now=roam_plat_now_us();
 if(nc_pause_scope()&&g_s.conf.race_pause)for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];
  if(p->id!=NC.peer||p->state!=RC_PS_EST)continue;
  capable=p->race_pause;
  connected=!p->lost&&p->clock_valid&&p->clock_rtt_us<=200000&&now-p->clock_sample_at<=10000000&&now-p->last_rx_us<1000000;
  break;
 }
 if(capable){
  cp_frame(&g_s.pause,g_s.conf.self_id,NC.peer,NC.control.run,NC.control.event,NC.host,blocked,now);
  cp_step(&g_s.pause,now,connected);held=cp_held(&g_s.pause);
 }else cp_clear(&g_s.pause);
 roam_plat_unlock();
 return held;
}
int nc_pause_gate(uint32_t w,int native_paused){
 if(!nc_pause_scope()||w!=NC.world)return native_paused;
 return nc_pause_latch(nc_pause_publish(roam_plat_now_us(),nc_pause_raw(native_paused))||native_paused);
}
static int nc_pause_held(void){
 int held;roam_plat_lock();held=nc_pause_scope()&&cp_held(&g_s.pause);roam_plat_unlock();return nc_pause_latch(held);
}
static int nc_pause_frame(int64_t now){
 static int64_t next_wait_log;
 int raw=nc_pause_scope()?nc_pause_raw(nc_original_world_paused(NC.world)):0;
 int held=nc_pause_latch(nc_pause_publish(now,raw));
 if(held!=nc_pause_was_held){
  if(!held&&nc_pause_scope()){
   cs_reanchor(&NC.stream,now);NC.next_send=0;
   nw_pause_resume(now);
   /* Native local humans and host AI retain their own body lifecycle.
    * Only received network actors are re-ghosted until fresh poses arrive. */
   for(int i=0;i<NC.stream.racers;i++)if(nc_remote_car(NC.car[i])){NC.slot[i].ghost=1;body_set(0,NC.car[i],0);}
  }
  roam_plat_lock();
  int64_t sample=roam_plat_now_us();
  rlog("[coop-pause] %s: run %08x raw=%d phase=%d blocked=%d flags=%d/%d frame-age=%lld peer-age=%lld; frames and network have separate liveness\n",held?"holding":"released",NC.control.run,raw,g_s.pause.phase,g_s.pause.blocked,g_s.pause.local_flags,g_s.pause.remote_flags,(long long)(sample-g_s.pause.frame_at),(long long)(sample-g_s.pause.peer_at));
  if(held)rlog("[coop-pause-trigger] reason=%u frame-age=%lld peer-age=%lld trigger-age=%lld us\n",g_s.pause.hold_reason,(long long)g_s.pause.hold_frame_age,(long long)g_s.pause.hold_peer_age,(long long)(sample-g_s.pause.hold_at));
  roam_plat_unlock();
  nc_pause_was_held=held;
 }
 if(held&&now>=next_wait_log){
  next_wait_log=now+5000000;
  roam_plat_lock();
  int64_t sample=roam_plat_now_us();
  rlog("[coop-pause-wait] run=%08x raw=%d phase=%d flags=%d/%d seen=%d arm-ack=%d go-ack=%d generation=%u frame-age=%lld peer-age=%lld us\n",
   NC.control.run,raw,g_s.pause.phase,g_s.pause.local_flags,g_s.pause.remote_flags,g_s.pause.remote_seen,
   g_s.pause.arm_acked,g_s.pause.go_acked,g_s.pause.generation,
   (long long)(sample-g_s.pause.frame_at),(long long)(g_s.pause.peer_at?sample-g_s.pause.peer_at:-1));
  roam_plat_unlock();
 }
 return held;
}
int nc_pause_clock_enter(uint32_t clock,uint32_t *saved){
 if(clock!=0x89cf48)return 0;
 if(!nc_pause_clock_depth){
  if(!nc_pause_scope()){
   nc_pause_tick_world=nc_pause_tick_run=0;
   nc_pause_tick_held=nc_pause_clock_was_held=0;return 0;
  }
  int previous=nc_pause_tick_matches()&&nc_pause_clock_was_held;
  /* Evaluate raw pause BEFORE the native clock consumes this frame's dt.
   * Do not use nc_pause_held here: it includes the previous frame's latch. */
  int held=nc_pause_publish(roam_plat_now_us(),nc_pause_raw(nc_original_world_paused(NC.world)));
  nc_pause_tick_world=NC.world;nc_pause_tick_run=NC.control.run;
  nc_pause_tick_held=held||previous;nc_pause_clock_was_held=held;
  if(previous&&!held)rlog("[coop-pause-clock] resume boundary: run=%08x; discard paused frame dt and refresh native anchor\n",NC.control.run);
 }
 if(!nc_pause_tick_held)return 0;
 /* Scope the game's own pause flag to its original time calculation. The
  * original paused branch then freezes simulation time and updates its
  * wall-clock anchor. Never retain a borrowed flag between engine calls. */
 *saved=rd32(0x86676c);if(!nc_pause_clock_depth)nc_pause_original_flag=*saved;
 /* The paused flag bypasses the native +54 wall-clock anchor update.
  * Keep that anchor current, or unpausing adds the whole stopped interval
  * to the native synchronization target despite a zero simulation dt. */
 uint32_t anchor=rd32(clock+0x54);
 if(anchor&&anchor!=0x7fffffffu)MR32(clock+0x54)=rd32(0x8651ac);
 nc_pause_clock_depth++;MR32(0x86676c)=1;return 1;
}
void nc_pause_clock_leave(uint32_t saved){MR32(0x86676c)=saved;if(nc_pause_clock_depth)nc_pause_clock_depth--;}
static int nc_pause_accept_frame(const rc_race_frame *f,int64_t now){
 int active;roam_plat_lock();active=g_s.pause.active;roam_plat_unlock();
 if(!active)return 1;
 if(!NC.clock_valid)return 0;
 for(int i=0;i<f->n;i++){
  int32_t age=(int32_t)((uint32_t)now-(f->car[i].state.t_us+(uint32_t)NC.clock_offset));
  if(age>500000||age< -200000)return 0;
 }
 return 1;
}
