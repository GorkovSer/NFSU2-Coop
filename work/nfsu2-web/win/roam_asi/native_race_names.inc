/* Game-thread presentation adapter. IDs and human slots are canonical;
 * profile names, AI ownership and save data are never used as network IDs. */
static int nc_name_scope(void){
 return NC.enabled&&NC.bound&&nc_running()&&nc_same_world();
}
static Slot *nc_name_slot(int index){
 if(!nc_name_scope()||index!=0)return 0;
 Slot *s=&NC.slot[NC.host?1:0];
 return s->name[0]&&s->car?s:0;
}
static void nc_names_frame(void){
 if(!nc_name_scope())return;
 char own[RC_NAME_MAX+1]={0},peer[RC_NAME_MAX+1]={0};
 roam_plat_lock();
 snprintf(own,sizeof own,"%s",g_s.conf.name);
 for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].state==RC_PS_EST&&g_s.peer[i].id==NC.peer){
  snprintf(peer,sizeof peer,"%s",g_s.peer[i].name);break;
 }
 roam_plat_unlock();
 for(int i=0;i<2;i++){
  Slot *s=&NC.slot[i];const char *name=i==(NC.host?0:1)?own:peer;
  if(!name[0])continue;
  snprintf(s->name,sizeof s->name,"%s",name);
  set_name(s);
 }
}
