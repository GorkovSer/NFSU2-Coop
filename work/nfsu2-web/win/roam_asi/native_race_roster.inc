/* Full host AI vehicle roster. Never publish a bot through human CFG. */
static nv_slot NAI_MODELS[RC_RACE_CARS];
static struct {uint32_t world,run,event,peer;int racers,active;int64_t next_verify;} NAI;
static void nai_release(CPU *c){
 for(int k=2;k<RC_RACE_CARS;k++)if(NAI_MODELS[k].started)nv_reset(c,&NAI_MODELS[k]);
 memset(&NAI,0,sizeof NAI);
 roam_plat_lock();memset(&g_s.roster,0,sizeof g_s.roster);roam_plat_unlock();
}
static void nai_refresh(CPU *c,unsigned index){
 uint32_t car=NC.car[index];Slot *slot=&NC.slot[index];
 gcall0(c,FN_CAR_BODYPOS,car);float delta[3],f[3],l[3],u[3];
 rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);memset(slot->com,0,sizeof slot->com);
 for(int k=0;k<3;k++)delta[k]=MRF32(car+0x60+4*k)-MRF32(car+0x50+4*k);
 if(!rd32(car+0x28))memcpy(slot->com,delta,sizeof delta);
 else for(int k=0;k<3;k++){slot->com[0]+=delta[k]*f[k];slot->com[1]+=delta[k]*l[k];slot->com[2]+=delta[k]*u[k];}
 memset(slot->spin,0,sizeof slot->spin);slot->steer=0;slot->ghost=1;
}
static int nai_frame(CPU *c,int64_t now){
 if(!NC.bound||!nc_same_world())return -1;
 if(!NAI.active){
  if(NC.control.phase!=CC_LOADING)return -1;
  nai_release(c);NAI.world=world();NAI.run=NC.control.run;NAI.event=NC.control.event;NAI.peer=NC.peer;
  NAI.racers=NC.stream.racers;NAI.active=1;
  roam_plat_lock();ar_init(&g_s.roster,g_s.conf.self_id,NC.peer,NAI.run,NAI.event,NC.host,NAI.racers);roam_plat_unlock();
  if(NC.host){
   for(int k=2;k<NAI.racers;k++){
    uint8_t bytes[RC_CFG_MAX];char name[RC_CFG_TYPE_NAME+1];rc_carcfg cfg;
    uint32_t car=NC.car[k],ride=rd32(car+0x10),physics=0;
    int n=local_cfg_car(car,bytes,sizeof bytes,name);
    if(n<=0||rc_carcfg_decode(bytes,n,&cfg)!=RC_OK||!cfg.has_perf||
       !nv_matches_cfg(&cfg,ride)||!nv_physics_signature(ride,&physics))return -1;
    roam_plat_lock();int frozen=ar_host_set(&g_s.roster,k,bytes,n,physics);roam_plat_unlock();
    if(!frozen)return -1;
    rlog("[roster] frozen AI slot %d: %s, %d bytes, physics %08x\n",k,name,n,physics);
   }
   roam_plat_lock();int sealed=ar_seal(&g_s.roster);roam_plat_unlock();if(!sealed)return -1;
  }
 }
 if(NAI.world!=world()||NAI.run!=NC.control.run||NAI.event!=NC.control.event||NAI.peer!=NC.peer||NAI.racers!=NC.stream.racers)return -1;
 roam_plat_lock();int fault=g_s.roster.fault;roam_plat_unlock();if(fault)return -1;
 if(NC.control.phase==CC_DONE){roam_plat_lock();int done=ar_ready(&g_s.roster);roam_plat_unlock();return done;}
 int starts=0,verify=now>=NAI.next_verify;
 if(verify)NAI.next_verify=now+(NC.control.phase==CC_LOADING?250000:1000000);
 for(int k=2;k<NAI.racers;k++){
  uint8_t bytes[RC_CFG_MAX];int n=0;uint32_t digest=0,proof=0;
  roam_plat_lock();
  if(g_s.roster.active&&g_s.roster.car[k-2].complete&&(NC.host||g_s.roster.manifest_seen)){
   n=g_s.roster.car[k-2].length;memcpy(bytes,g_s.roster.car[k-2].bytes,(unsigned)n);
   digest=g_s.roster.car[k-2].digest;proof=g_s.roster.car[k-2].physics;
  }
  roam_plat_unlock();if(!n)continue;
  if(NC.host){
   if(verify){
    rc_carcfg cfg;uint32_t physical=0,ride=rd32(NC.car[k]+0x10);
    if(rc_carcfg_decode(bytes,n,&cfg)!=RC_OK||!nv_matches_cfg(&cfg,ride)||
       !nv_physics_signature(ride,&physical)||physical!=proof){
     rlog("[roster] host AI slot %d changed after freeze; refusing mixed roster\n",k);return -1;
    }
   }
   continue;
  }
  nv_slot *model=&NAI_MODELS[k];
  if(!model->started){
   if(starts)continue;
   rc_carcfg cfg;if(rc_carcfg_decode(bytes,n,&cfg)!=RC_OK||!cfg.has_perf)return -1;
   uint32_t car=NC.car[k],skin=skin_of(rd32(car+0x14));if(!skin)return -1;
   if(nv_begin(c,model,car,&cfg,skin,now,1)<0)return -1;starts++;
  }
  int was=model->applied,loaded=nv_poll(c,model,now);if(loaded<0)return -1;
  if(loaded&&!was)nai_refresh(c,(unsigned)k);
  if(loaded){
   roam_plat_lock();int matched=ar_loaded(&g_s.roster,k,digest,model->physics_digest);roam_plat_unlock();
   if(!matched){rlog("[roster] AI slot %d physics differs from host: %08x / %08x\n",k,model->physics_digest,proof);return -1;}
  }
 }
 roam_plat_lock();
 ar_fragment fragment;if(ar_next_fragment(&g_s.roster,now,&fragment))rc_send_race_cfg(&g_s,&fragment);
 uint8_t op;uint32_t value;
 if(ar_next_control(&g_s.roster,now,&op,&value)){
  rc_coop_msg message={op,NC.peer,NC.control.run,NC.control.event,value};rc_send_coop(&g_s,&message);
 }
 int ready=ar_ready(&g_s.roster),failed=g_s.roster.fault;
 roam_plat_unlock();return failed?-1:ready;
}
