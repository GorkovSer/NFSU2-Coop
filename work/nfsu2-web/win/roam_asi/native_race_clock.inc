/* Shared race lifecycle clock: every event uses the same start epoch,
 * owner finish stamp and host pause ledger. Never accesses sockets or cars. */
static int nm_wall(uint32_t *wall){
 int64_t delta=roam_plat_now_us()-NC.control.release_at;
 if(!NC.control.release_at||delta<0||delta>(int64_t)RM_MAX_TICKS*250)return 0;
 *wall=(uint32_t)(delta/250);return 1;
}
static int nm_shared_timing(void){
 uint32_t elapsed[RM_CARS]={0};rm_outcome *s=&NM.outcome;
 for(int k=0;k<s->racers;k++){
  if(!s->car[k].status)return 0;
  if(s->mode==RM_DRIFT&&!s->car[k].wall)continue;
  if(!rk_elapsed(&NM.clock,s->car[k].wall,&elapsed[k]))return 0;
 }
 return rm_timing(s,elapsed);
}
