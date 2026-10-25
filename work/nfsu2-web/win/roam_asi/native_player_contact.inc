/* Native body retention for the optional host-authoritative contact path.
 * Bodies require the mutual World/physics barrier; no legacy fallback. */
int np_contacts_enabled(void){
 static int checked,enabled;
 if(!checked){checked=1;enabled=env_int("NFSU2_ROAM_PLAYER_CONTACT",0,0,1);}
 return enabled;
}
void np_before_zero(uint32_t car){
 /* An engine-driven mode reset must disable the body BEFORE deleting its
  * physics object, even when it occurs between our own frame hooks. */
 int owned=nc_remote_car(car);
 if(!owned)for(int k=0;k<NSLOT;k++)if(g_sl[k].car==car&&g_sl[k].world==world()&&
     (g_sl[k].st==SL_ACTIVE||g_sl[k].st==SL_LOADING)){owned=1;break;}
 if(owned&&ok(car,0x9e0))body_set(0,car,0);
}
static int np_source_live(uint32_t peer,int64_t now){
 int live=0;
 roam_plat_lock();
 now=roam_plat_now_us();
 if(cp_held(&g_s.pause)){roam_plat_unlock();return 0;}
 for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];
  if(p->id!=peer||p->state!=RC_PS_EST||p->lost||!p->player_contact)continue;
  if(p->status==RC_ST_ROAM)live=now-p->last_snap_rx>=0&&now-p->last_snap_rx<500000;
  else if(p->status==RC_ST_EVENT&&NC.bound&&NC.peer==peer&&
          NC.control.phase==CC_RUNNING&&!cs_stale(&NC.stream,now))
   live=now-NC.stream.last_rx>=0&&now-NC.stream.last_rx<500000;
  break;
 }
 roam_plat_unlock();return live;
}
static int np_native_ready(uint32_t car){
 uint32_t body=rd32(car+0x3c),physics=rd32(car+0x34);
 if(!rd32(car+0x28)||!ok(physics,0x10)||!ok(body,0x134)||rd32(body+0x130)!=car)return 0;
 /* The 0x5939ce crash dereferenced this RideInfo physics link. Full
  * dynamic modes must have both native references before contact is on.
  * Kinematic native modes have body+5c=0 and do not own these links. */
 if(rd32(body+0x5c)&&(!ok(rd32(body+0x64),0x10)||!ok(rd32(body+0x68),0x1c0)))return 0;
 return 1;
}
static int np_prepare_pose(CPU *c,Slot *s,const rc_pose *pose){
 uint32_t car=s->car;
 if(!np_contacts_enabled()||!ok(car,0x9e0))return 0;
 uint32_t body=rd32(car+0x3c);
 /* A proxy restored while parked can be remapped to kinematic mode4.
  * Retry once the native streamed-cell flag becomes ready; a nonzero
  * mode alone does not establish full physics. Never force that flag. */
 int dynamic=ok(body,0x134)&&rd32(body+0x5c);
 if(!rd32(car+0x28)||(!dynamic&&MR8(car+0x5c0))){
  body_set(c,car,0);
  /* Native SetPhysicsMode selects the actual compatible mode from the
   * driver's/car's flags. Never force a mode by writing Car+28 directly. */
  gcall1(c,FN_CAR_SETPHYS,car,3);
#ifdef NATIVE_ENGINE_LAB
  static int64_t next_mode_log;
  int64_t mode_now=roam_plat_now_us();
  if(mode_now>=next_mode_log){
   uint32_t rebuilt_body=rd32(car+0x3c);
   rlog("[player-mode-rebuild] car=%08x mode=%u streamed=%u body-dynamic=%u\n",car,rd32(car+0x28),MR8(car+0x5c0),rd32(rebuilt_body+0x5c));
   next_mode_log=mode_now+2000000;
  }
#endif
  if(!np_native_ready(car))return 0;
  gcall0(c,FN_CAR_BODYPOS,car);
  float f[3],l[3],u[3],delta[3];rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);
  for(int k=0;k<3;k++){
   delta[k]=MRF32(car+0x60+4*k)-MRF32(car+0x50+4*k);
   if(!__builtin_isfinite(delta[k])||__builtin_fabsf(delta[k])>10.f)return 0;
  }
  memset(s->com,0,sizeof s->com);
  for(int k=0;k<3;k++){s->com[0]+=delta[k]*f[k];s->com[1]+=delta[k]*l[k];s->com[2]+=delta[k]*u[k];}
  s->ghost=1;
 }
 if(!np_native_ready(car)){body_set(c,car,0);return 0;}
 float jump=0;
 /* Compare successive network positions. A native collision can move the
  * physical proxy away from its last network pose; treating that response
  * as a teleport would disable contact while the cars are still touching. */
 for(int k=0;k<3;k++){float d=pose->p[k]-s->drawn[k];jump+=d*d;}
 if(s->pose_frames&&jump>JUMP_M*JUMP_M)s->ghost=1;
 return 1;
}
/* Body's native local bounds (min E0, max F0) and the current car pose.
 * The six face axes are a conservative separating-axis test: omitting edge
 * cross axes can keep a safe pair ghosted, but cannot certify an overlap.
 * This is shared by city and every event; the grid is not a special case. */
typedef struct {float centre[3],axis[3][3],half[3];} np_box;
static int np_read_box(uint32_t car,np_box *box){
 uint32_t body=rd32(car+0x3c);
 if(!ok(car,0xbc)||!ok(body,0x134)||rd32(body)!=0x7a2478u||rd32(body+0x130)!=car)return 0;
 float local[3];
 for(int j=0;j<3;j++){
  float lo=MRF32(body+0xe0+4*j),hi=MRF32(body+0xf0+4*j);
  if(!__builtin_isfinite(lo)||!__builtin_isfinite(hi)||lo>=hi||lo< -8.f||hi>8.f)return 0;
  float size=hi-lo;
  if(size<(j==0?2.f:j==1?1.f:.3f)||size>(j==0?8.f:4.f))return 0;
  local[j]=(lo+hi)*.5f;box->half[j]=size*.5f+.15f;
  for(int k=0;k<3;k++){
   float a=MRF32(car+0x90+16*j+4*k);
   if(!__builtin_isfinite(a))return 0;
   box->axis[j][k]=a;
  }
 }
 for(int j=0;j<3;j++)for(int k=j;k<3;k++){
  float dot=0;for(int m=0;m<3;m++)dot+=box->axis[j][m]*box->axis[k][m];
  if(__builtin_fabsf(dot-(j==k?1.f:0.f))>.01f)return 0;
 }
 for(int k=0;k<3;k++){
  box->centre[k]=MRF32(car+0x60+4*k);
  if(!__builtin_isfinite(box->centre[k]))return 0;
  for(int j=0;j<3;j++)box->centre[k]+=local[j]*box->axis[j][k];
 }
 return 1;
}
static int np_spawn_clear(uint32_t car,uint32_t me){
 np_box boxes[2];if(!np_read_box(car,&boxes[0])||!np_read_box(me,&boxes[1]))return 0;
 for(int b=0;b<2;b++)for(int a=0;a<3;a++){
  float d=0,r=0;const float *axis=boxes[b].axis[a];
  for(int k=0;k<3;k++)d+=(boxes[0].centre[k]-boxes[1].centre[k])*axis[k];
  for(int n=0;n<2;n++)for(int j=0;j<3;j++){
   float dot=0;for(int k=0;k<3;k++)dot+=boxes[n].axis[j][k]*axis[k];
   r+=boxes[n].half[j]*__builtin_fabsf(dot);
  }
  if(__builtin_fabsf(d)>r+.1f)return 1;
 }
 return 0;
}
static void np_update_body(CPU *c,Slot *s){
 uint32_t me=player_car();
 int live=np_contacts_enabled()&&np_native_ready(s->car)&&!s->paused&&
          np_source_live(s->peer_id,roam_plat_now_us())&&npi_contact_ready(s->peer_id,roam_plat_now_us());
 if(!live){s->ghost=1;body_set(c,s->car,0);return;}
 if(s->ghost&&ok(me,0x70)){
  float d2=0;for(int k=0;k<3;k++){float d=MRF32(s->car+0x60+4*k)-MRF32(me+0x60+4*k);d2+=d*d;}
  if(d2>CLEAR_M*CLEAR_M||np_spawn_clear(s->car,me))s->ghost=0;
 }
 body_set(c,s->car,!s->ghost);
}
