/* Ordinary pre-race NIS runs in flow handler603bc0 BEFORE SetRaceState(3).
 * Hold its action1 pump, then let the original scene finish naturally.
 * The state3/5f94e0 gate remains only for alternate replay/start paths. */
static uint32_t nm_next_intro_run(uint32_t race);
static struct {uint32_t race,world,run,poses;int waiting,released,ready_frames,nis;} NCP;
static int ncp_current(uint32_t race){
 return NCP.waiting&&race==NCP.race&&race==rd32(G_RACE)&&world()==NCP.world&&NC.control.run==NCP.run;
}
static int ncp_hold(uint32_t race,uint32_t state){
 if(!NC.enabled||state!=3||race!=rd32(G_RACE)||
    !ok(race,0xc44)||MR32(race)>3||!world()||
    rd32(rd32(G_CAREER_RACE)+8)!=NC.control.event)return 0;
 uint32_t run=NC.control.phase==CC_LOADING?NC.control.run:nm_next_intro_run(race);
 if(!run)return 0;
 if(!NCP.waiting||NCP.race!=race||NCP.world!=world()||NCP.run!=run){
  memset(&NCP,0,sizeof NCP);
  NCP.race=race;NCP.world=world();NCP.run=run;NCP.waiting=1;
  rlog("[coop-cinematic] preparation barrier, run=%08x state=%u\n",run,MR32(race));
 }
 return !NCP.released;
}
int ncp_nis_hold(uint32_t flow,uint32_t action){
 uint32_t race=rd32(G_RACE);
 if(action!=1||flow!=rd32(0x8900d8)||!ok(race,0xc44)||MR32(race)!=2)return 0;
 if(!ncp_hold(race,3))return 0;
 if(!NCP.nis){
  NCP.nis=1;
  rlog("[coop-cinematic] holding pre-race NIS pump BEFORE scene playback, run=%08x\n",NCP.run);
 }
 return 1;
}
int ncp_intro_hold(uint32_t manager){
 if(manager!=0x89cf48||rd32(manager)!=3)return 0;
 return ncp_hold(rd32(G_RACE),3);
}
static void ncp_pose_ready(unsigned slot){
 if(ncp_current(NC.race)&&slot<6)NCP.poses|=1u<<slot;
}
static int ncp_bind_ready(uint32_t race){
 return MR32(race)==3||(MR32(race)<3&&ncp_current(race));
}
static int ncp_frame(CPU *c,int vehicles_ready){
 if(!ncp_current(NC.race))return MR32(NC.race)==3;
 if(!vehicles_ready){NCP.ready_frames=0;return 0;}
 uint32_t wanted=0;
 for(unsigned k=0;k<(unsigned)NC.stream.racers;k++)if(nc_remote_car(NC.car[k]))wanted|=1u<<k;
 if((NCP.poses&wanted)!=wanted)return 0;
 /* Let the renderer consume an assembled roster before native intro starts.
  * The barrier is reached after applying remote grid poses, not before. */
 if(!NCP.released&&++NCP.ready_frames<2)return 0;
 if(NCP.nis){
  if(!NCP.released){
   NCP.released=1;
   rlog("[coop-cinematic] releasing NIS pump with both models and grid poses ready, run=%08x\n",NCP.run);
  }
  /* Do not jump to state3 here: that would skip the very scene we held.
   * 603bc0 itself requests state3 only after432550 reports NIS finished. */
  return nc_same_world()&&MR32(NC.race)==3;
 }
 if(!NCP.released||MR32(NC.race)<3){
  int first=!NCP.released;NCP.released=1;uint32_t args[2]={NC.race,3};
  rh_call(c,(uint32_t)(uintptr_t)nc_original_state,0,0,2,args);
  if(first)rlog("[coop-cinematic] requested native intro after both player vehicles and roster resources became ready\n");
 }
 /* The native manager may not be ready on the first request. Its state4
  * branch already ignores requests while the cinematic is running. */
 /* Native cinematic completion calls SetRaceState(3) again. Keep the
  * network start barrier closed until that normal transition occurs. */
 return nc_same_world()&&MR32(NC.race)==3&&rd32(0x89cf48)!=4;
}
