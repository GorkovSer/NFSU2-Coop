/* Shared native-distance and traffic observers. Do not invent another human Player, change the
 * human-player table, move the local car, or enable split-screen machinery.
 * World distance helpers already compare two cached positions/road handles.
 * Borrow the second cache during distance/traffic queries, restoring it on
 * return. This does not load map cells; spawn selection still uses actual
 * native road tracking and native placement eligibility. */
static int nw_interest_context(void){
 return NW.context?NC.bound&&NC.control.phase==CC_RUNNING&&nc_same_world()&&
  NC.control.event==NW.context:roaming();
}
static int nw_observer_valid(void){
 return NW.enabled&&NW.host&&nw_interest_context()&&NW.observer&&
        roam_plat_now_us()<NW.observer_until&&rd32(G_FLOW)==6&&world()==NW.world&&
        ok(NW.world,0xf0)&&rd32(NW.world+0x118)==NW.manager&&
        ok(NW.observer,0x9e0)&&MR8(NW.observer+0x550)&&rd32(NW.observer+0x8fc);
}
static void nw_update_interest(int64_t now){
 uint32_t selected=0,peer=0;
 if(!NW.context&&roaming()){
  for(int k=0;k<NSLOT;k++){
   Slot *slot=&g_sl[k];
   if(slot->st!=SL_ACTIVE||slot->world!=NW.world||slot->hidden||!slot->car||!ok(slot->car,0x9e0))continue;
   int live=0;roam_plat_lock();now=roam_plat_now_us();
   for(int i=0;i<RC_TABLE;i++){
    rc_peer *p=&g_s.peer[i];
    if(p->state==RC_PS_EST&&p->id==slot->peer_id&&!p->lost&&p->traffic&&!p->world_host&&
       ((p->status==RC_ST_ROAM&&p->last_snap_rx&&now-p->last_snap_rx<1000000)||
        (p->status==RC_ST_PAUSED&&now-p->last_rx_us<2000000))){live=1;break;}
   }
   roam_plat_unlock();
   if(live&&rd32(slot->car+0x8fc)){selected=slot->car;peer=slot->peer_id;break;}
  }
 }else if(NW.context&&nw_interest_context()){
  uint32_t car=NC.car[NC.host?1:0];int live=0;
  roam_plat_lock();now=roam_plat_now_us();
  for(int i=0;i<RC_TABLE;i++){
   rc_peer *p=&g_s.peer[i];
   if(p->id==NC.peer&&p->state==RC_PS_EST&&!p->lost&&p->traffic&&!p->world_host&&
      p->status==RC_ST_EVENT&&now>=NC.stream.last_rx&&now-NC.stream.last_rx<500000){live=1;break;}
  }
  roam_plat_unlock();
  if(live&&ok(car,0x9e0)&&rd32(car+0x8fc)){selected=car;peer=NC.peer;}
 }
 if(peer!=NW.observer_peer)rlog("[traffic] second spawning/retention observer: %08x\n",peer);
 NW.observer=selected;NW.observer_peer=peer;NW.observer_until=selected?now+250000:0;
}
void nw_interest_begin(nw_interest_scope *scope){
 memset(scope,0,sizeof *scope);if(!nw_observer_valid())return;
 scope->world=NW.world;
 memcpy(scope->position,GPTR(NW.world+0x90),12);scope->road=MR32(NW.world+0xe4);
 memcpy(GPTR(NW.world+0x90),GPTR(NW.observer+0x60),12);
 MR32(NW.world+0xe4)=MR32(NW.observer+0x8fc);scope->active=1;
}
void nw_interest_end(const nw_interest_scope *scope){
 if(!scope->active||world()!=scope->world||!ok(scope->world,0xf0))return;
 memcpy(GPTR(scope->world+0x90),scope->position,12);MR32(scope->world+0xe4)=scope->road;
}
static float nw_region_distance(uint32_t a,uint32_t b){
 float n=0;for(int k=0;k<2;k++){float d=MRF32(a+0x60+4*k)-MRF32(b+0x60+4*k);n+=d*d;}return n;
}
static int nw_regions(uint32_t me,unsigned count[2],unsigned *capacity){
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);count[0]=count[1]=0;*capacity=n;
 if(!n||!ok(me,0x9e0)||!nw_observer_valid())return 0;
 for(unsigned i=0;i<n;i++)if(MR8(pool[i]+0x550)&&nw_traffic_driver(rd32(pool[i]+0x24))){
  float local=nw_region_distance(pool[i],me),remote=nw_region_distance(pool[i],NW.observer);
  if(local<90000.f||remote<90000.f)count[remote<local]++;
 }
 return 1;
}
static int nw_behind_and_distant(uint32_t car,uint32_t observer){
 float dx=MRF32(car+0x60)-MRF32(observer+0x60),dy=MRF32(car+0x64)-MRF32(observer+0x64);
 float distance=dx*dx+dy*dy;
 /* A conservative handoff: do not steal nearby cars to fill the other half.
  * Outside 300 m is eligible regardless of orientation; otherwise require
  * at least 100 m and behind the observer. Camera look-back remains a limit. */
 return distance>90000.f||(distance>10000.f&&dx*MRF32(observer+0x90)+dy*MRF32(observer+0x94)<0.f);
}
uint8_t nw_recycle(void *manager,uint32_t car,nw_eligible_fn original){
 /* 417be0 casts Car+2c to the large native AI controller and calls +68.
  * NullController has only26 entries: its +68 is "Onli" (696c6e4f),
  * exactly the target in the user's minidump. Never call it for an inert
  * proxy/transitional controller, even when the ordinary manager runs. */
 if(NW.owned&&world()==NW.world&&(uint32_t)(uintptr_t)manager==NW.manager)return 0;
 uint32_t brain=rd32(car+0x2c);
 if(!ok(car,0x9e0)||!ok(brain,0x79c)||!nw_traffic_controller(rd32(car+0x24),rd32(brain)))return 0;
 if(rd32(brain)==0x785428u&&!rd32(brain+0x6fc))return 0;
 uint8_t eligible=original(manager,car);if(eligible||!nw_observer_valid()||
    (uint32_t)(uintptr_t)manager!=NW.manager||!ok(car,0x9e0)||!MR8(car+0x550)||!nw_traffic_driver(rd32(car+0x24)))return eligible;
 uint32_t me=player_car();unsigned count[2],capacity;
 if(!nw_regions(me,count,&capacity)||nw_region_distance(me,NW.observer)<10000.f)return 0;
 int region=nw_region_distance(car,NW.observer)<nw_region_distance(car,me);
 if(count[region]<=capacity/2||count[1-region]>=capacity/2)return 0;
 /* Native placement already supports reusing active traffic. Merely permit
  * an out-of-view surplus slot; the manager still owns placement/reset and
  * its ordinary attempt cadence. This also handles a guest joining a full pool. */
 return nw_behind_and_distant(car,me)&&nw_behind_and_distant(car,NW.observer);
}
uint8_t nw_spawn(void *manager,uint32_t car,const float *density,nw_spawn_fn original){
 if(!nw_observer_valid()||(uint32_t)(uintptr_t)manager!=NW.manager)
  return original(manager,car,density);
 /* Keep the original single attempt/manager timer and shared traffic pool.
  * Alternate regions when separated, never doubling native spawn frequency.
  * A full pool cannot recycle traffic still near either observer because
  * the native distance checks above include both positions/road handles. */
 uint32_t me=player_car();float distance=0;
 if(!ok(me,0x9e0))return original(manager,car,density);
 for(int k=0;k<2;k++){float d=MRF32(me+0x60+4*k)-MRF32(NW.observer+0x60+4*k);distance+=d*d;}
 unsigned count[2],capacity;int choose_remote=++NW.spawn_turn&1;
 if(nw_regions(me,count,&capacity)&&count[0]!=count[1])choose_remote=count[1]<count[0];
 if(distance<10000.f||!choose_remote)return original(manager,car,density);
 float quote=((float(__attribute__((thiscall))*)(void*,uint32_t))(uintptr_t)0x418270)(manager,NW.observer);
 if(!__builtin_isfinite(quote)||__builtin_fabsf(quote-99999.f)<=0.001f)return 0;
 /* Mirror the original post-selection branch, with the remote observer's
  * actual density. Its native car+18 may be null; native 42ea30 supports it. */
 if(MR8(0x89e7d8)||rd32(0x890108))
  return ((uint8_t(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,float))(uintptr_t)0x42e660)(manager,car,NW.observer,quote);
 if(!((uint8_t(__attribute__((thiscall))*)(void*))(uintptr_t)0x4035b0)(manager))return 0;
 return ((uint8_t(__attribute__((thiscall))*)(void*,uint32_t,uint32_t,float))(uintptr_t)0x42ea30)(manager,car,NW.observer,quote);
}
