static void nc_apply_result(CPU *c){
 if(NC.result_applied||!nc_same_world())return;
 if(nm_mode(rd32(G_CAREER_RACE))){
  int ready=nm_frame(c,roam_plat_now_us());
  if(ready<=0||!nm_result_ready()||!nr_reward_result(NC.control.winner!=0))return;
  if(nm_apply_result(c)>0)NC.result_applied=1;
  return;
 }
 uint32_t rec=NC.record[NC.host?0:1];int index=-1;
 if(!ok(rec,0x20))return;
 for(unsigned i=0;i<(unsigned)NC.stream.racers;i++)if(rd32(NC.race+0xbd0+4*i)==rec){index=(int)i;break;}
 if(index<0||!nr_reward_result(NC.control.winner!=0))return;
 if(!MR16(rec+0x10)){
  /* Use the native finish path for both local career profiles. If native
   * completion is deferred, retry next frame; reward credit is idempotent. */
  unsigned position=MR8(rec+0xa);
  if(!position||position>(unsigned)NC.stream.racers)position=(unsigned)NC.stream.racers;
  uint32_t args[5]={NC.race,(uint32_t)index,NC.control.winner?1u:position,0,NC.control.winner?1u:3u};
  rh_call(c,(uint32_t)(uintptr_t)nc_original_finish,0,0,5,args);
 }
 if(ok(rec,0x20)&&MR16(rec+0x10))NC.result_applied=1;
}
