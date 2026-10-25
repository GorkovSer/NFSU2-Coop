/* Disposable two-engine laboratory. Never included in production builds. */
#ifndef NATIVE_ENGINE_LAB
#error Lab race commands require the isolated laboratory build
#endif
/* Actual contact fixture: give only the local human car real momentum.
 * No fabricated collision pair, remote impulse, readiness flag or result. */
static int native_lab_contact(unsigned command,uint32_t speed_cm){
 if(!NC.bound||NC.control.phase!=CC_RUNNING||!nc_same_world()||nc_pause_held()||
    speed_cm<100||speed_cm>2000)return -6;
 uint32_t own=player_car(),target=0;
 if(own!=NC.car[NC.host?0:1]||rd32(own+0x24)!=1||!ni_ready(own))return -7;
 if(command==4){
  if(!npi_contact_ready(NC.peer,roam_plat_now_us()))return -8;
  target=NC.car[NC.host?1:0];
 }else{
  uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);float best=1600.f;
  for(unsigned i=0;i<n;i++){
   uint32_t car=pool[i];float d=nw_dist2((float*)GPTR(own+0x60),(float*)GPTR(car+0x60));
   uint32_t body=rd32(car+0x3c);
   if(MR8(car+0x550)&&rd32(car+0x28)&&ok(body,0x134)&&!MR8(body+0x17)&&d>=9.f&&d<best){target=car;best=d;}
  }
 }
 if(!ok(target,0x9e0))return -9;
 if(MR8(rd32(own+0x3c)+0x17)||MR8(rd32(target+0x3c)+0x17))return -13;
 float delta[3],v[3],dp[3],arm[3]={0};rd3(own+0x70,v);
 for(int k=0;k<3;k++)delta[k]=MRF32(target+0x60+4*k)-MRF32(own+0x60+4*k);
 float distance=rc_sqrtf(nw_len2(delta));
 /* Native two-wide grid centres can be 2.7 m apart without overlap. */
 if(!__builtin_isfinite(distance)||distance<2.5f||distance>40.f||__builtin_fabsf(delta[2])>2.f)return -10;
 uint32_t body=rd32(own+0x3c),info=rd32(body+0x68);float mass=MRF32(info+0x164);
 if(!__builtin_isfinite(mass)||mass<=.000001f||mass>10000.f)return -11;
 for(int k=0;k<3;k++){
  dp[k]=mass*(delta[k]/distance*(float)speed_cm/100.f-v[k]);
  if(!__builtin_isfinite(dp[k]))return -12;
 }
 if(nw_len2(dp)>100000000.f)return -12;
 ni_apply(body,dp,arm);
 rlog("[lab-contact] own=%08x target=%08x kind=%s distance=%.3f speed=%.2f; real own-body momentum only\n",
 own,target,command==4?"player":"traffic",distance,(float)speed_cm/100.f);
 return 1;
}
__declspec(dllexport) int NativeLabRaceCommand(unsigned command,uint32_t hash){
 if(!nc_enabled()||rd32(G_FLOW)!=6)return -1;
 if(command==4||command==5)return native_lab_contact(command,hash);
 if(command==2){if(NC.host||NC.control.phase!=CC_INVITED)return -2;NATIVE_LAB_READY=1;return 1;}
 if(command==3){NATIVE_LAB_CANCEL=1;return 1;}
 if(command!=1||!NC.host||!NC.connected||!roaming()||nc_busy())return -3;
 uint32_t event=0;
 for(unsigned i=0;i<64;i++){
  uint32_t candidate=rd32(0x85ad40u+0x71c8u+4*i);
  if(!ok(candidate,0x88)||!nc_event_supported(candidate))continue;
  if(hash?MR32(candidate+8)==hash:MR8(candidate+0xf)==0&&!nc_event_mode(candidate)){event=candidate;break;}
 }
 if(!event)return -4;
 MR32(0x863444)=1;MR32(0x863448)=event;
 int handled=nc_start();
 rlog("[lab-race] normal start handler: event=%08x handled=%d pending=%08x\n",MR32(event+8),handled,NC.pending_event);
 return handled&&NC.pending_event?1:-5;
}
