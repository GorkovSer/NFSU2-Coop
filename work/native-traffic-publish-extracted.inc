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