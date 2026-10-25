/* Game-thread adapter. It never reads game objects from the network thread.
 * Missing traffic models load through the same native loader as race cars. */
#include "traffic_sync.h"
#include "native_world.h"
static struct {
 int checked,enabled,host,owned,missing_logged;
 uint32_t world,context,generation,next_id,peer,manager;
 uint32_t observer,observer_peer,spawn_turn;
 int64_t observer_until;
 uint16_t seq;
 int64_t next_send,last_frame;
 ts_client client;
 struct {uint32_t car,id,type;float p[3];int active;} host_car[RC_TRAFFIC_CARS];
 struct {uint32_t car,id,brain;Slot pose;} replica[RC_TRAFFIC_CARS];
} NW;
static nv_slot NW_MODELS[RC_TRAFFIC_CARS];
static struct {
 struct {uint32_t car,id;int valid;int64_t at;rc_pose written;} sample[RC_TRAFFIC_CARS];
 struct {rc_traffic_contact contact;int left;int64_t at;} pending[8];
 struct {uint32_t peer,world;uint16_t seq;uint64_t seen;float tokens;int64_t at;} replay[RC_TABLE];
 uint16_t seq;
 int64_t next_capture;
 uint32_t sent,accepted,rejected;
} NWC;
static void nw_pause_resume(int64_t now){
 for(int i=0;i<RC_TRAFFIC_CARS;i++)rc_interp_reset(&NW.client.car[i].motion);
 NW.client.scene_since=now;NW.client.last_rx=now;NW.last_frame=now;NW.next_send=0;
 memset(NWC.sample,0,sizeof NWC.sample);memset(NWC.pending,0,sizeof NWC.pending);
}
int nw_host(void){return NW.host;}
int nw_enabled(void){
 if(!NW.checked){
  NW.checked=1;NW.host=env_int("NFSU2_ROAM_WORLD_HOST",0,0,1);
  if(env_int("NFSU2_ROAM_TRAFFIC",0,0,1)){
   NW.enabled=nw_hooks_install();NW.generation=roam_plat_random();NW.next_id=1;
   rlog("[traffic] experimental adapter %s, role %s; native model loading\n",NW.enabled?"verified":"OFF: hook mismatch",NW.host?"host":"client");
  }
 }
 return NW.enabled;
}
static int nw_same(void){return NW.owned&&rd32(G_FLOW)==6&&world()==NW.world&&rd32(NW.world+0x118)==NW.manager;}
int nw_manager_owned(uint32_t manager){return nw_same()&&manager==NW.manager;}
static unsigned nw_pool(uint32_t out[RC_TRAFFIC_CARS]);
static int nw_pending_basic_controller(uint32_t brain){
 /* Native785428:+68 is41e7c0, which immediately dereferences +6fc.
  * A controller created for an inactive pool car has no route goal yet. */
 return ok(brain,0x79c)&&rd32(brain)==0x785428u&&!rd32(brain+0x6fc);
}
int nw_brain_owned(uint32_t brain){
 if(nw_pending_basic_controller(brain))return 1;
 if(!nw_same())return 0;
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0)&&rd32(NW.replica[i].car+0x2c)==brain)return 1;
 /* The complete pool is ours, including unassigned/inactive cars. Those
  * controllers are not present in the host-to-replica mapping. */
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);
 for(unsigned i=0;i<n;i++)if(rd32(pool[i]+0x2c)==brain)return 1;
 return 0;
}
static const char *nw_pool_status="not-checked",*nw_step_status="not-started";
static unsigned nw_pool(uint32_t out[RC_TRAFFIC_CARS]){
 uint32_t w=world();if(!ok(w,0x11c))return 0;
 unsigned first=MR32(w+0x18),end=MR32(w+0x10),n=0;
 nw_pool_status="valid";
 if(first>end||end>22||end-first>RC_TRAFFIC_CARS){nw_pool_status="invalid-range";return 0;}
 if(first==end)nw_pool_status="no-native-traffic-slots";
 for(unsigned i=first;i<end;i++){
  uint32_t car=rd32(w+0x1c+4*i),rec=rd32(car+0x14);
  if(!ok(car,0x9e0)||!ok(rec,0x940)){nw_pool_status="invalid-car-or-record";return 0;}
  if(MR32(rec+4)!=3){nw_pool_status="unexpected-record-kind";return 0;}
  for(unsigned j=0;j<n;j++)if(out[j]==car)return 0;
  out[n++]=car;
 }
 return n;
}
static uint32_t nw_type(uint32_t car){uint32_t ti=rd32(car+0xc);return ok(ti,0xd4)?MR32(ti+0xd0):0;}
#include "native_traffic_interest.inc"
#include "native_traffic_contact.inc"
#include "native_traffic_replica.inc"
static void nw_release(CPU *c){
 /* Deactivate replicas through the game, so its manager may safely respawn
  * them with original traffic brains after disconnect or a missing model. */
 /* Mapping rebuilds can fail halfway through. Withdraw the complete native
  * pool before releasing resources; the partially rebuilt map is insufficient. */
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_same()?nw_pool(pool):0;
 nw_withdraw_pool(c,pool,n);
 if(nw_same())for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0))body_set(c,NW.replica[i].car,0);
 /* Restore rides before deactivation replaces the controller identity used
  * by nv_reset's attachment guard, then withdraw any replacement body. */
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW_MODELS[i].started)nv_reset(c,&NW_MODELS[i]);
 if(nw_same()){
  for(unsigned i=0;i<n;i++)nw_park_replica(c,pool[i]);
  for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0)){
   int found=0;for(unsigned k=0;k<n;k++)if(pool[k]==NW.replica[i].car)found=1;
   if(!found)nw_park_replica(c,NW.replica[i].car);
  }
 }
 if(NW.owned)rlog("[traffic] shared roster released; ordinary traffic resumes\n");
 NW.owned=0;memset(NW.replica,0,sizeof NW.replica);
 memset(NWC.sample,0,sizeof NWC.sample);memset(NWC.pending,0,sizeof NWC.pending);
}
static void nw_publish(int64_t now,const uint32_t *pool,unsigned n){
 if(now<NW.next_send)return;
 NW.next_send=now+50000;
 rc_traffic_frame f;memset(&f,0,sizeof f);f.world=NW.generation;f.context=NW.context;f.seq=NW.seq++;f.t_us=(uint32_t)now;
 for(unsigned i=0;i<n;i++){
  uint32_t car=pool[i],type=nw_type(car);rc_state state;
  int active=MR8(car+0x550)!=0&&nw_traffic_driver(rd32(car+0x24))&&type&&nc_read_car(car,&state,now);
  if(!active){NW.host_car[i].active=0;continue;}
  float distance=0;for(int k=0;k<3;k++){float d=state.p[k]-NW.host_car[i].p[k];distance+=d*d;}
  if(!NW.host_car[i].active||NW.host_car[i].car!=car||NW.host_car[i].type!=type||distance>2500.f){
   if(!NW.next_id)NW.next_id=1;
   NW.host_car[i].id=NW.next_id++;
  }
  NW.host_car[i].active=1;NW.host_car[i].car=car;NW.host_car[i].type=type;memcpy(NW.host_car[i].p,state.p,sizeof state.p);
  unsigned j=f.n++;f.car[j].id=NW.host_car[i].id;f.car[j].type=type;f.car[j].state=state;
 }
 roam_plat_lock();
 for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].state==RC_PS_EST&&!g_s.peer[i].lost&&g_s.peer[i].traffic&&!g_s.peer[i].world_host){f.to=g_s.peer[i].id;rc_send_traffic(&g_s,&f);}
 roam_plat_unlock();
}
static int nw_assign(const uint32_t *pool,unsigned n,uint32_t selected[RC_TRAFFIC_CARS]){
 unsigned used=0;memset(selected,0,RC_TRAFFIC_CARS*sizeof *selected);
 /* Stable identities retain their car even if packets reorder the roster. */
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.client.car[i].id){
  for(int j=0;j<RC_TRAFFIC_CARS;j++)if(NW.replica[j].id==NW.client.car[i].id){
   for(unsigned k=0;k<n;k++)if(!(used&(1u<<k))&&pool[k]==NW.replica[j].car){selected[i]=pool[k];used|=1u<<k;break;}
  }
 }
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.client.car[i].id&&!selected[i]){
  for(unsigned k=0;k<n;k++)if(!(used&(1u<<k))&&nw_type(pool[k])==NW.client.car[i].type){selected[i]=pool[k];used|=1u<<k;break;}
 }
 /* After exact model matches, reserve another traffic slot and stream the
  * required stock model into it. Never consume the human or racer ranges. */
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.client.car[i].id&&!selected[i]){
  for(unsigned k=0;k<n;k++)if(!(used&(1u<<k))){selected[i]=pool[k];used|=1u<<k;break;}
  if(!selected[i])return 0;
 }
 return 1;
}
static void nw_frame_step(CPU *c,int64_t now){
 nw_step_status="disabled";
 if(!nw_enabled())return;
 /* Unattended developer probe: isolated loopback build, frontend only.
  * Its scratch ride never belongs to a world, save, or player. */
 static int probed,probe_failures;
 if(probed<46*12&&O.test&&env_int("NFSU2_ROAM_NATIVE_SELFTEST",0,0,1)&&rd32(G_FLOW)==3&&ok(rd32(G_CARTYPES),46*0x890)){
  int type=probed/12,variant=probed%3,level=(probed/3)%4;probed++;gmem_init(c);
  if(g_gmem){
   rc_carcfg cfg;memset(&cfg,0,sizeof cfg);uint32_t types=rd32(G_CARTYPES)+type*0x890;
   cfg.type_hash=MR32(types+0xd0);cstr(types,cfg.type_name,sizeof cfg.type_name);cfg.has_perf=1;cfg.ride_b[3]=1;
   memset(cfg.perf+11,variant==0?100:variant==1?0:200,41);
   uint32_t ride=g_sl[NSLOT-1].ride_new;char why[96]={0};
   int missing=build_ride(c,&cfg,ride,1,why,sizeof why);
   if(level){
    for(unsigned category=0;category<10;category++){uint32_t args[2]={(uint32_t)level,category};rh_call(c,0x5a63d0,ride+0x2f0,0,2,args);}
    if(!nv_physics(c,ride))missing=-1;
    memset(cfg.perf,0,11);
    for(int i=0;i<10;i++)if(MR32(ride+0x318+4*i))cfg.perf[i>>3]|=(uint8_t)(0x80u>>(i&7));
    for(int i=0;i<68;i++)if(MR8(ride+0x340+i))cfg.perf[2+(i>>3)]|=(uint8_t)(0x80u>>(i&7));
   }
   rlog("[native-test] %d/%d/%d %s actual physics: missing=%d class=%u engine points=%u embedded=%d\n",type,level,variant,cfg.type_name,missing,MR8(ride+0x14),MR32(ride+0x56c),MR32(ride+0x57c)==ride+0x46c);
   uint32_t clone=g_sl[NSLOT-1].ride_orig;gcall1(c,0x413470,clone,ride);
   int difference=memcmp(GPTR(ride+0x46c),GPTR(clone+0x46c),256);
   int valid=missing==0&&!difference&&MR32(clone+0x57c)==clone+0x46c&&MR32(clone+0x38c)==clone+0x2f0&&MR32(clone+0x390)==clone+0x40;
   if(level){
    uint8_t bytes[RC_CFG_MAX];int count=rc_carcfg_encode(&cfg,bytes,sizeof bytes);rc_carcfg decoded;
    if(count<=0||rc_carcfg_decode(bytes,count,&decoded)!=RC_OK)valid=0;
    else {
     int result=build_ride(c,&decoded,ride,1,why,sizeof why);
     if(result||memcmp(GPTR(ride+0x46c),GPTR(clone+0x46c),256)||memcmp(GPTR(ride+0x398),GPTR(clone+0x398),41))valid=0;
    }
   }
   float peak=0;for(int k=0;k<64;k++){float v=MRF32(clone+0x46c+4*k);if(!__builtin_isfinite(v))valid=0;if(v>peak)peak=v;}
   if(!MR32(types+0x844)&&peak<=0)valid=0;
   if(!valid)probe_failures++;
   rlog("[native-test] native copy: valid=%d peak=%.3f\n",valid,(double)peak);
   if(probed==46*12)rlog("[native-test] COMPLETE %d cases, %d failures\n",probed,probe_failures);
  }
 }
 int64_t dt=NW.last_frame?now-NW.last_frame:0;NW.last_frame=now;if(dt<0||dt>100000)dt=0;
 uint32_t w=world(),event=rd32(G_CAREER_RACE),context=event?rd32(event+8):0;
 int scene=rd32(G_FLOW)==6&&ok(w,0x11c)&&player_car();
 if(w!=NW.world||context!=NW.context||!scene){
  nw_release(c);NW.world=w;NW.context=context;NW.manager=rd32(w+0x118);
  if(++NW.generation==0)NW.generation=1;
  NW.seq=0;NW.next_send=0;memset(NW.host_car,0,sizeof NW.host_car);
  memset(NWC.replay,0,sizeof NWC.replay);
  ts_scene(&NW.client,context,now);NW.missing_logged=0;
 }
 if(!scene){nw_step_status="not-in-live-world";return;}
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);
 if(!n){nw_step_status=nw_pool_status;nw_release(c);return;}
 if(NW.host){
  nw_step_status="host-publishing";nw_update_interest(now);
  static int64_t next_interest_log;
  unsigned regions[2],capacity;uint32_t own=player_car();
  if(now>=next_interest_log&&nw_regions(own,regions,&capacity)&&nw_region_distance(own,NW.observer)>10000.f){
   next_interest_log=now+5000000;
   rlog("[world-interest] peer=%08x context=%08x distance2=%.1f traffic-local=%u remote=%u capacity=%u peer-native-section=%u physics=%u road=%.3f; no forced streaming\n",
    NW.observer_peer,NW.context,(double)nw_region_distance(own,NW.observer),regions[0],regions[1],capacity,
    MR8(NW.observer+0x5c0),rd32(NW.observer+0x28),(double)MRF32(NW.observer+0x8fc));
  }
  nw_host_contacts(now);nw_publish(now,pool,n);return;
 }
 uint32_t peer=0;int clock=0,got=0;int32_t offset=0;rc_traffic_frame incoming;
 roam_plat_lock();
 for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];if(p->state!=RC_PS_EST||p->lost||!p->traffic||!p->world_host)continue;
  if(peer){peer=0;break;}peer=p->id;
  clock=p->clock_valid&&now-p->clock_sample_at<10000000&&p->clock_rtt_us<=200000;offset=p->clock_offset_us;
 }
 if(peer&&clock)got=rc_take_traffic(&g_s,peer,&incoming);
 roam_plat_unlock();
 if(peer!=NW.peer){nw_release(c);NW.peer=peer;ts_init(&NW.client,g_s.conf.self_id,peer,now);ts_scene(&NW.client,context,now);}
 if(!peer||!clock){nw_step_status=peer?"host-clock-not-ready":"no-compatible-host";nw_release(c);return;}
 if(got)ts_receive(&NW.client,peer,&incoming,now,offset);
 ts_expire(&NW.client,now);
 if(!NW.client.active){nw_step_status="no-valid-host-roster";nw_release(c);return;}
 uint32_t selected[RC_TRAFFIC_CARS];
 if(!nw_assign(pool,n,selected)){
  nw_step_status="native-capacity-or-map-refusal";
  nw_release(c);
  if(!NW.missing_logged){NW.missing_logged=1;rlog("[traffic] host roster exceeds native traffic capacity; local traffic retained\n");}
  return;
 }
 if(!NW.owned){NW.owned=1;NW.manager=rd32(w+0x118);rlog("[traffic] host roster acquired\n");}
 NW.missing_logged=0;
 nw_step_status="client-applying";
 /* Withdraw every pool body before any loader/controller call can fail.
  * Inactive cars can still retain a body; activation is not a collision gate. */
 nw_withdraw_pool(c,pool,n);
 for(unsigned k=0;k<n;k++){
  int assigned=0;for(int i=0;i<RC_TRAFFIC_CARS;i++)if(selected[i]==pool[k])assigned=1;
  if(!assigned&&MR8(pool[k]+0x550))nw_park_replica(c,pool[k]);
 }
 Slot previous[RC_TRAFFIC_CARS];for(int i=0;i<RC_TRAFFIC_CARS;i++)previous[i]=NW.replica[i].pose;
 memset(NW.replica,0,sizeof NW.replica);
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(selected[i]){
  uint32_t car=selected[i];NW.replica[i].car=car;NW.replica[i].id=NW.client.car[i].id;
  NW.replica[i].brain=rd32(car+0x2c);
  unsigned index=0;while(index<n&&pool[index]!=car)index++;
  if(index>=n){nw_release(c);return;}
  nv_slot *model=&NW_MODELS[index];int replaced=0;
  if(model->started&&model->car!=car)nv_reset(c,model);
  if(!nw_prepare_replica(c,model,car)){nw_step_status="native-controller-refused";nw_release(c);return;}
  NW.replica[i].brain=rd32(car+0x2c);
  if((model->started&&model->type!=NW.client.car[i].type)||(!model->started&&nw_type(car)!=NW.client.car[i].type)){
   if(model->started)nv_reset(c,model);
   rc_carcfg cfg;memset(&cfg,0,sizeof cfg);cfg.type_hash=NW.client.car[i].type;
   uint32_t ti=gcall1(c,FN_TYPE_BY_HASH,0,cfg.type_hash);
   if(!ok(ti,0xd4)){nw_release(c);return;}
   cstr(ti,cfg.type_name,sizeof cfg.type_name);cfg.ride_b[3]=1;
   if(nv_begin(c,model,car,&cfg,1,now,0)<0){nw_release(c);return;}
  }
  if(model->started){
   int was=model->applied,result=nv_poll(c,model,now);
   if(result<0){nw_release(c);return;}
   if(!result){body_set(c,car,0);soft_park(c,car);continue;}
   replaced=!was;
  }
  if(!nw_prepare_physics(c,car)){nw_release(c);return;}
  /* Keep the body withdrawn until an actual fresh pose has been written.
   * A loaded model alone is not a visible synchronized traffic actor. */
  body_set(c,car,0);
  NW.replica[i].brain=rd32(car+0x2c);
  int retained=0;if(!replaced)for(int k=0;k<RC_TRAFFIC_CARS;k++)if(previous[k].car==car){NW.replica[i].pose=previous[k];retained=1;break;}
  NW.replica[i].pose.car=car;
  if(!retained){
   gcall0(c,FN_CAR_BODYPOS,car);
   float f[3],l[3],u[3];rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);
   for(int k=0;k<3;k++){
    float d=MRF32(car+0x60+4*k)-MRF32(car+0x50+4*k);
    if(__builtin_isfinite(d)&&__builtin_fabsf(d)<3.f){NW.replica[i].pose.com[0]+=d*f[k];NW.replica[i].pose.com[1]+=d*l[k];NW.replica[i].pose.com[2]+=d*u[k];}
   }
  }
  rc_pose pose;if(ts_pose(&NW.client,NW.client.car[i].id,now,dt,&pose)){
   nw_capture_contact(car,NW.client.car[i].id,index,now);
   nw_apply_visible(c,&NW.replica[i].pose,&pose,(float)dt/1000000.f);
   nw_contact_sample(car,NW.client.car[i].id,index,&pose,now);
  }else nw_apply_visible(c,&NW.replica[i].pose,0,(float)dt/1000000.f);
 }
 nw_send_contacts(now);
}
static void nw_frame(CPU *c,int64_t now){
 nw_frame_step(c,now);
 static int64_t next_log;if(!NW.enabled||now<next_log)return;next_log=now+5000000;
 unsigned active=0,replicas=0;for(int i=0;i<RC_TRAFFIC_CARS;i++){active+=NW.host_car[i].active!=0;replicas+=NW.replica[i].car!=0;}
 uint32_t w=world();int valid=ok(w,0x11c);
 rlog("[traffic-gate] role=%s stage=%s pool=%s world=%08x context=%08x first=%u end=%u host-active=%u client-active=%d replicas=%u source=%08x last-rx=%lld\n",NW.host?"host":"client",nw_step_status,nw_pool_status,w,NW.context,valid?rd32(w+0x18):0,valid?rd32(w+0x10):0,active,NW.client.active,replicas,NW.peer,(long long)NW.client.last_rx);
 if(!NW.host&&NW.owned){
  unsigned disabled=0,kinematic=0,dynamic=0,invalid=0,no_cell=0,no_render=0;
  for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car){
   uint32_t car=NW.replica[i].car,body=rd32(car+0x3c);
   if(!ok(car,0x9e0)||!ok(body,0x134)||!rd32(car+0x28)){invalid++;continue;}
   disabled+=MR8(body+0x17)!=0;
   no_cell+=!MR8(car+0x5c0);no_render+=!rd32(car+0x554);
   if(rd32(body+0x5c))dynamic++;else kinematic++;
  }
  rlog("[traffic-bodies] replicas=%u disabled=%u kinematic=%u dynamic=%u invalid=%u no-cell=%u no-render=%u\n",replicas,disabled,kinematic,dynamic,invalid,no_cell,no_render);
 }
 if(valid&&rd32(w+0x18)<rd32(w+0x10)&&rd32(w+0x10)<=22){
  uint32_t car=rd32(w+0x1c+4*rd32(w+0x18)),rec=rd32(car+0x14);
  if(ok(car,0x9e0)&&ok(rec,0x940))rlog("[traffic-slot] car=%08x kind=%u activated=%u driver=%u mode=%u type=%08x\n",car,rd32(rec+4),MR8(car+0x550),rd32(car+0x24),rd32(car+0x28),nw_type(car));
 }
}
