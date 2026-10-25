static int nv_owner_frame(CPU *c,uint32_t own,int64_t now){
 uint32_t ride=rd32(own+0x10);
 nv_owner_cleanup(c);
 if(nv_matches_equipment(&NV_FROZEN.cfg,ride,0)&&(!NV_OWNER.started||NV_OWNER.applied))return 1;
 if(NC.control.phase!=CC_LOADING||own!=player_car()||!ok(own,0x9e0))return -1;
 if(NV_OWNER.started&&(NV_OWNER.car!=own||NV_OWNER.world!=world()))return -1;
 uint32_t previous=NV_OWNER_CAR;NV_OWNER_CAR=own;int result=0;
 if(!NV_OWNER.started){
  uint32_t skin=skin_of(rd32(own+0x14));
  result=nv_begin(c,&NV_OWNER,own,&NV_FROZEN.cfg,skin,now,1);
 }
 if(result>=0)result=nv_poll(c,&NV_OWNER,now);
 NV_OWNER_CAR=previous;
 if(result>0){
  if(!nv_matches_cfg(&NV_FROZEN.cfg,rd32(own+0x10)))return -1;
  rlog("[vehicle] recovered own pre-race car and equipment before start; type=%08x\n",NV_FROZEN.cfg.type_hash);
 }
 return result;
}
/* Called only with the original race and AI records bound. A missing or
 * incomplete car record cannot pass the co-op loading barrier. */
static int nc_vehicle_frame(CPU *c,int64_t now){
 if(!NC.bound||!nc_same_world())return -1;
 if(!NV_FROZEN.valid||NV_FROZEN.peer!=NC.peer||
    NV_FROZEN.run!=NC.control.run||NV_FROZEN.event!=NC.control.event)return -1;
 char profile[17];cstr(G_PROFILE_NAME,profile,sizeof profile);
 if(strcmp(profile,NV_FROZEN.profile))return -1;
 uint32_t own=NC.car[NC.host?0:1],ride=rd32(own+0x10),signature=0;
 if(!NV_FROZEN.finalized){
  int restored=nv_owner_frame(c,own,now);if(restored<=0)return restored;
  if(!nv_finalize_owner(own,now))return -1;
  ride=rd32(own+0x10);
 }
 if(!NV_BARRIER.initialized)return -1;
 if(NC.control.phase==CC_DONE)return vs_ready(&NV_BARRIER);
 if(own!=player_car()||!ok(own,0x9e0)||rd32(own+0x20)!=ride+0x40||
    !nv_matches_cfg(&NV_FROZEN.cfg,ride)||!nv_physics_signature(ride,&signature)||signature!=NV_FROZEN.physics){
  rlog("[vehicle] local human car differs from its pre-load freeze; refusing substituted vehicle\n");return -1;
 }
 if(NV_BARRIER.fault)return -1;
 rc_coop_msg message;
 if((!vs_ready(&NV_BARRIER)||NC.control.phase==CC_LOADING)&&vs_next(&NV_BARRIER,now,&message)){roam_plat_lock();rc_send_coop(&g_s,&message);roam_plat_unlock();}
 if(!NV_BARRIER.remote_seen||!NV_BARRIER.physics_seen)return 0;
 if(!NV_RACE.started){
  uint8_t bytes[RC_CFG_MAX];int n=0;rc_carcfg cfg;
  roam_plat_lock();
  for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].id==NC.peer&&g_s.peer[i].state==RC_PS_EST&&
      g_s.peer[i].vehicles&&g_s.peer[i].cfg_len>0&&g_s.peer[i].cfg_rev==g_s.peer[i].cfg_rev_seen){
   n=g_s.peer[i].cfg_len;memcpy(bytes,g_s.peer[i].cfg,n);break;
  }
  roam_plat_unlock();
  if(!n)return 0;
  uint32_t digest=rc_fnv1a_bytes(bytes,(uint32_t)n);if(digest!=NV_BARRIER.remote)return 0;
  if(rc_carcfg_decode(bytes,n,&cfg)!=RC_OK||!cfg.has_perf)return -1;
  uint32_t car=NC.car[NC.host?1:0],skin=skin_of(rd32(car+0x14));
  if(!skin)return -1;
  int result=nv_begin(c,&NV_RACE,car,&cfg,skin,now,1);if(result<0)return -1;
  NV_LOADED_DIGEST=digest;
 }
 int was_applied=NV_RACE.applied,loaded=nv_poll(c,&NV_RACE,now);if(loaded<0)return -1;
 if(loaded&&!was_applied){
  /* SetRideInfo changes the native centre of mass. The old bot's offset
   * must not keep shifting the new model relative to the network pose. */
  unsigned index=NC.host?1:0;uint32_t car=NC.car[index];Slot *slot=&NC.slot[index];
  gcall0(c,FN_CAR_BODYPOS,car);float delta[3],f[3],l[3],u[3];
  rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);memset(slot->com,0,sizeof slot->com);
  for(int k=0;k<3;k++)delta[k]=MRF32(car+0x60+4*k)-MRF32(car+0x50+4*k);
  if(!rd32(car+0x28))memcpy(slot->com,delta,sizeof delta); /* mode 0 returns the unrotated ride vector */
  else for(int k=0;k<3;k++){slot->com[0]+=delta[k]*f[k];slot->com[1]+=delta[k]*l[k];slot->com[2]+=delta[k]*u[k];}
  memset(slot->spin,0,sizeof slot->spin);slot->steer=0;
 }
 if(loaded&&!vs_loaded_physics(&NV_BARRIER,NV_LOADED_DIGEST,NV_RACE.physics_digest)){
  rlog("[vehicle] calculated remote physics differs from the owner's proof: local %08x owner %08x\n",NV_RACE.physics_digest,NV_BARRIER.remote_physics);return -1;
 }
 return vs_ready(&NV_BARRIER);
}
