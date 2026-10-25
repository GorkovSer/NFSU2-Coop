/* Experimental velocity-response feedback, opt-in TrafficFeedback=1.
 * The host remains the only traffic authority. Capture native solver changes
 * before pose correction, not the correction itself. This is a proximity /
 * separating-velocity detector, not yet a native contact-manifold callback;
 * keep it off by default until actual collision driving is authorized. */
static float nw_len2(const float *v){return v[0]*v[0]+v[1]*v[1]+v[2]*v[2];}
static float nw_dist2(const float *a,const float *b){float d[3];for(int k=0;k<3;k++)d[k]=a[k]-b[k];return nw_len2(d);}
static void nw_limit(float *v,float max){
 float n=nw_len2(v);if(n>max*max){float scale=max/rc_sqrtf(n);for(int k=0;k<3;k++)v[k]*=scale;}
}
static void nw_contact_sample(uint32_t car,uint32_t id,unsigned slot,const rc_pose *pose,int64_t now){
 if(slot>=RC_TRAFFIC_CARS)return;
 NWC.sample[slot].car=car;NWC.sample[slot].id=id;NWC.sample[slot].written=*pose;
 NWC.sample[slot].at=now;NWC.sample[slot].valid=1;
}
static void nw_capture_contact(uint32_t car,uint32_t id,unsigned slot,int64_t now){
 if(!g_s.conf.traffic_contact||slot>=RC_TRAFFIC_CARS||now<NWC.next_capture||!NW.owned||
    !NWC.sample[slot].valid||NWC.sample[slot].car!=car||NWC.sample[slot].id!=id)return;
 int64_t dt=now-NWC.sample[slot].at;
 if(dt<1000||dt>50000||now-NW.client.last_rx>300000)return;
 uint32_t me=player_car();if(!ok(me,0x9e0))return;
 rc_traffic_contact hit;memset(&hit,0,sizeof hit);
 rd3(car+0x60,hit.p);rd3(me+0x60,hit.player);
 if(nw_dist2(hit.p,hit.player)>42.25f||__builtin_fabsf(hit.p[2]-hit.player[2])>3.f)return;
 float natural[3],dv[3],dw[3];rd3(car+0x70,dv);rd3(car+0x110,dw);
 for(int k=0;k<3;k++){
  natural[k]=NWC.sample[slot].written.p[k]+NWC.sample[slot].written.v[k]*(float)dt/1000000.f;
  dv[k]-=NWC.sample[slot].written.v[k];dw[k]-=NWC.sample[slot].written.w[k];
  if(!__builtin_isfinite(dv[k])||!__builtin_isfinite(dw[k]))return;
 }
 /* Reject teleports, large corrections and gravity-only changes. A traffic
  * impact must create horizontal separating motion away from the local car. */
 if(nw_dist2(hit.p,natural)>9.f||dv[0]*dv[0]+dv[1]*dv[1]<0.64f||nw_len2(dv)>625.f||nw_len2(dw)>64.f)return;
 float outward=dv[0]*(hit.p[0]-hit.player[0])+dv[1]*(hit.p[1]-hit.player[1]);
 if(outward<0.5f)return;
 int free=-1;for(int i=0;i<8;i++)if(!NWC.pending[i].left){free=i;break;}
 if(free<0)return;
 hit.to=NW.peer;hit.world=NW.client.world;hit.context=NW.context;hit.id=id;hit.seq=++NWC.seq;hit.t_us=(uint32_t)now;
 nw_limit(dv,12.f);nw_limit(dw,2.f);memcpy(hit.dv,dv,sizeof dv);memcpy(hit.dw,dw,sizeof dw);
 NWC.pending[free].contact=hit;NWC.pending[free].left=3;NWC.pending[free].at=now;
 /* Leave packet budget for ordinary snapshots and race/control traffic:
  * at most ten observations/s, three transmissions per observation. */
 NWC.next_capture=now+100000;
}
static void nw_send_contacts(int64_t now){
 for(int i=0;i<8;i++)if(NWC.pending[i].left&&now>=NWC.pending[i].at){
  rc_traffic_contact *hit=&NWC.pending[i].contact;
  if(hit->world!=NW.client.world||hit->context!=NW.context||hit->to!=NW.peer||
     (int32_t)((uint32_t)now-hit->t_us)>300000){NWC.pending[i].left=0;continue;}
  roam_plat_lock();int sent=rc_send_traffic_contact(&g_s,hit);roam_plat_unlock();
  if(sent)NWC.sent++;
  NWC.pending[i].left--;NWC.pending[i].at=now+50000;
 }
}
static void nw_host_contacts(int64_t now){
 rc_traffic_contact hit;uint32_t from;
 for(int drain=0;drain<RC_TRAFFIC_CONTACT_INBOX;drain++){
  rc_state remote;int available=0,clock=0,index=-1;int32_t offset=0;
  roam_plat_lock();int got=rc_take_traffic_contact(&g_s,&from,&hit);
  if(got)for(int i=0;i<RC_TABLE;i++){
   rc_peer *p=&g_s.peer[i];
   if(p->id!=from||p->state!=RC_PS_EST||p->lost||!p->traffic_contact||p->world_host)continue;
   clock=p->clock_valid&&now-p->clock_sample_at<10000000&&p->clock_rtt_us<=200000;
   offset=p->clock_offset_us;index=i;
   if(p->mo.n&&now-p->last_snap_rx<500000){remote=p->mo.s[p->mo.n-1];available=1;}
   break;
  }
  roam_plat_unlock();if(!got)break;
  NWC.rejected++;
  if(!available||!clock||index<0||hit.world!=NW.generation||hit.context!=NW.context||hit.to!=g_s.conf.self_id)continue;
  int32_t age=(int32_t)((uint32_t)now-hit.t_us-(uint32_t)offset);
  if(age< -100000||age>500000||nw_dist2(hit.player,hit.p)>42.25f)continue;
  float peer_tolerance=8.f+0.5f*rc_sqrtf(nw_len2(remote.v));
  if(nw_dist2(remote.p,hit.player)>peer_tolerance*peer_tolerance)continue;
  uint32_t car=0,type=0;float published[3]={0};
  for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.host_car[i].active&&NW.host_car[i].id==hit.id){
   car=NW.host_car[i].car;type=NW.host_car[i].type;memcpy(published,NW.host_car[i].p,sizeof published);break;
  }
  rc_state current;
  if(!car||!ok(car,0x9e0)||!MR8(car+0x550)||!nw_traffic_driver(rd32(car+0x24))||nw_type(car)!=type||
     !nc_read_car(car,&current,now)||nw_dist2(current.p,published)>2500.f)continue;
  float tolerance=8.f+0.5f*rc_sqrtf(nw_len2(current.v));
  if(nw_dist2(current.p,hit.p)>tolerance*tolerance||__builtin_fabsf(current.p[2]-hit.p[2])>5.f)continue;
  /* 64-event replay window accepts reordered first deliveries, never retries.
   * Token budget limits additive velocity corrections during sustained contact. */
  if(NWC.replay[index].peer!=from||NWC.replay[index].world!=NW.generation){
   memset(&NWC.replay[index],0,sizeof NWC.replay[index]);NWC.replay[index].peer=from;
   NWC.replay[index].world=NW.generation;NWC.replay[index].tokens=24.f;NWC.replay[index].at=now;
  }
  uint16_t ahead=(uint16_t)(hit.seq-NWC.replay[index].seq);
  uint64_t seen=NWC.replay[index].seen,bit=1;
  if(seen&&ahead>=0x8000u){unsigned behind=(uint16_t)(NWC.replay[index].seq-hit.seq);if(behind>=64)continue;bit=(uint64_t)1<<behind;if(seen&bit)continue;}
  else if(seen&&!ahead)continue;
  float elapsed=(float)(now-NWC.replay[index].at)/1000000.f;
  if(elapsed<0)elapsed=0;
  float tokens=NWC.replay[index].tokens+elapsed*20.f;if(tokens>24.f)tokens=24.f;
  nw_limit(hit.dv,12.f);nw_limit(hit.dw,2.f);float cost=rc_sqrtf(nw_len2(hit.dv))+rc_sqrtf(nw_len2(hit.dw));
  if(cost>tokens)continue;
  float result[3],rotation[3];
  for(int k=0;k<3;k++){result[k]=current.v[k]+hit.dv[k];rotation[k]=current.w[k]+hit.dw[k];}
  if(nw_len2(result)>22500.f||nw_len2(rotation)>400.f)continue;
  float speed=0;for(int k=0;k<3;k++)speed+=result[k]*MRF32(car+0x90+4*k);
  if(!__builtin_isfinite(speed))continue;
  if(!seen||ahead<0x8000u){NWC.replay[index].seen=!seen||ahead>=64?1:(seen<<ahead)|1;NWC.replay[index].seq=hit.seq;}
  else NWC.replay[index].seen|=bit;
  NWC.replay[index].tokens=tokens-cost;NWC.replay[index].at=now;
  for(int k=0;k<3;k++){MRF32(car+0x70+4*k)=result[k];MRF32(car+0x110+4*k)=rotation[k];}
  MRF32(car+0x42c)=speed;MRF32(car+0x430)=speed;
  NWC.rejected--;NWC.accepted++;
  rlog("[traffic-contact] accepted peer %08x car %u seq %u dv %.2f %.2f %.2f\n",from,hit.id,hit.seq,(double)hit.dv[0],(double)hit.dv[1],(double)hit.dv[2]);
 }
}
