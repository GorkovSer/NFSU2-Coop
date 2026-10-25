/* One native player/player solver: host. DATA carries measured momentum
 * changes for the guest, so its owner does not rescale a raw impulse again. */
typedef struct {uint32_t nonce;int index;int32_t offset;rc_contact_scene scene;rc_state remote;int position;int64_t source_at;} npi_peer_info;
static struct {
 uint32_t world,car,ride,run,event,physics,contact_class,peer,nonce,token,next_token;
 int active,fault;int64_t frame_at;float position[3];
 struct {uint32_t body,car,world,info;int active,fault;float dp[3],dl[3];} scope;
 struct {rc_player_impulse hit;int active;int64_t born,next,after_ack;} pending[16];
 struct {uint32_t peer,nonce,host_world,guest_world;uint16_t seq;uint64_t seen;} replay;
 uint16_t seq;float tokens;int64_t token_at,next_send;
 uint32_t captured,applied,repeated,dropped,rejected;
 struct {uint32_t car,world,info;int reason;int64_t at;} synchronized[RC_TRAFFIC_CARS+RC_RACE_CARS+NSLOT+2];
} NPI;
static int npi_physics(uint32_t car,uint32_t *digest){
 uint32_t ride=rd32(car+0x10),base=0;if(!ni_ready(car)||!nv_physics_signature(ride,&base))return 0;
 uint32_t info=rd32(rd32(car+0x3c)+0x68);uint8_t bytes[32];rc_w w;rc_w_init(&w,bytes,sizeof bytes);
 rc_put32(&w,base);rc_put32(&w,MR32(ride));
 float values[5]={MRF32(info+0x164),MRF32(info+0x168),MRF32(info+0x170),MRF32(info+0x184),MRF32(info+0x198)};
 for(int k=0;k<5;k++){if(!__builtin_isfinite(values[k])||values[k]<.000001f||values[k]>10000.f)return 0;rc_putf(&w,values[k]);}
 if(__builtin_fabsf(values[0]*values[1]-1.f)>.01f)return 0;
 *digest=rc_crc32(bytes,w.n);return *digest!=0;
}
static int npi_peer(uint32_t id,int64_t now,npi_peer_info *out){
 int found=0;memset(out,0,sizeof *out);
 roam_plat_lock();
 now=roam_plat_now_us();
 for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];
  if(p->id!=id||!rc_player_contact_ready(&g_s,p,now)||!p->clock_valid||p->clock_rtt_us>200000||
     now<p->clock_sample_at||now-p->clock_sample_at>10000000)continue;
  out->nonce=p->extensions.nonce;out->index=i;out->offset=p->clock_offset_us;out->scene=p->contact_scene;found=1;
  if(p->status==RC_ST_ROAM&&p->mo.n&&now>=p->last_snap_rx&&now-p->last_snap_rx<500000){out->position=1;out->remote=p->mo.s[p->mo.n-1];}
  break;
 }
 roam_plat_unlock();
 if(found&&NC.bound&&NC.peer==id&&NC.control.phase==CC_RUNNING&&nc_same_world()&&!cs_stale(&NC.stream,now)){
  rc_interp *m=&NC.stream.motion[NC.host?1:0];if(m->n){out->remote=m->s[m->n-1];out->position=1;}
 }
 /* Compare production timestamps, not UDP arrival: a delayed old pose
  * received after ACK does not yet contain the owner's collision response. */
 if(out->position){int32_t age=(int32_t)((uint32_t)now-out->remote.t_us-(uint32_t)out->offset);out->source_at=now-age;}
 return found;
}
static int npi_sync_proxy(CPU *c,uint32_t car){
 (void)c;
 int slot=-1;int64_t now=roam_plat_now_us();
 for(unsigned k=0;k<sizeof NPI.synchronized/sizeof NPI.synchronized[0];k++){
  if(NPI.synchronized[k].car==car&&NPI.synchronized[k].world==world()){slot=(int)k;break;}
  if(slot<0&&(!NPI.synchronized[k].at||now-NPI.synchronized[k].at>100000))slot=(int)k;
 }
 if(slot<0)slot=0;
 NPI.synchronized[slot].at=0;
 NPI.synchronized[slot].car=car;NPI.synchronized[slot].world=world();
#define NPI_SYNC_FAIL(code) do{NPI.synchronized[slot].reason=(code);return 0;}while(0)
 if(car==player_car()||!ok(car,0x9e0))NPI_SYNC_FAIL(1);
 uint32_t body=rd32(car+0x3c);
 if(ni_car(body)!=car||!rd32(car+0x28))NPI_SYNC_FAIL(2);
 /* Native kinematic bodies read Car's pose directly; they have no hidden
  * full-physics state to synchronize. Do not ghost ordinary traffic. */
 if(!rd32(body+0x5c)){NPI.synchronized[slot].reason=11;return 1;}
 if(!ni_ready(car))NPI_SYNC_FAIL(3);
 uint32_t info=rd32(rd32(car+0x3c)+0x68);float matrix[16]={0},p[3],v[3],w[3];
 float mass=MRF32(info+0x164),inverse=MRF32(info+0x168);
 if(!__builtin_isfinite(mass)||!__builtin_isfinite(inverse)||mass<.000001f||mass>10000.f||inverse<=0)NPI_SYNC_FAIL(4);
 for(int k=0;k<3;k++){
  float inertia=MRF32(info+0x170+20*k);
  if(!__builtin_isfinite(inertia)||inertia<.000001f)NPI_SYNC_FAIL(5);
  for(int j=0;j<3;j++){float value=MRF32(car+0x90+16*k+4*j);if(!__builtin_isfinite(value))NPI_SYNC_FAIL(6);matrix[4*k+j]=value;}
  float length=matrix[4*k]*matrix[4*k]+matrix[4*k+1]*matrix[4*k+1]+matrix[4*k+2]*matrix[4*k+2];
  if(length<.99f||length>1.01f)NPI_SYNC_FAIL(7);
 }
 for(int k=0;k<3;k++)for(int j=k+1;j<3;j++){
  float dot=0;for(int a=0;a<3;a++)dot+=matrix[4*k+a]*matrix[4*j+a];if(__builtin_fabsf(dot)>.01f)NPI_SYNC_FAIL(8);
 }
 matrix[15]=1;rd3(car+0x60,p);rd3(car+0x70,v);rd3(car+0x110,w);
 /* While contact DATA is in flight, the host's proxy already contains
  * the measured response. Replacing that momentum with an older network
  * velocity makes each solver step hit the same unresponsive car again.
  * Keep native linear/angular response until ACK plus a newer owner pose;
  * positions/orientation remain owned by the normal snapshot stream. */
 if(g_s.conf.world_host&&NPI.active&&NPI.world==world()&&NPI.synchronized[slot].info==info){
  for(int k=0;k<16;k++)if(NPI.pending[k].active&&NPI.pending[k].hit.host_world==NPI.token&&
     ni_peer_car(NPI.pending[k].hit.to)==car){rd3(info+0xa0,v);rd3(info+0xc0,w);break;}
 }
 for(int k=0;k<3;k++)if(!__builtin_isfinite(p[k])||!__builtin_isfinite(v[k])||!__builtin_isfinite(w[k]))NPI_SYNC_FAIL(9);
 if(nw_len2(v)>40000.f||nw_len2(w)>400.f)NPI_SYNC_FAIL(10);
 ni_set_state(info,p,matrix,v,w);
 NPI.synchronized[slot].reason=0;NPI.synchronized[slot].info=info;NPI.synchronized[slot].at=now;
#undef NPI_SYNC_FAIL
 return 1;
}
static int npi_contact_reason(uint32_t peer,int64_t now){
 npi_peer_info p;uint32_t me=player_car(),remote=ni_peer_car(peer),own=0,other=0;
 if(NPI.fault||!NPI.active||NPI.world!=world()||NPI.car!=me)return 1;
 if(!npi_peer(peer,now,&p))return 2;
 now=roam_plat_now_us();
 if(!p.position)return 3;
 if(!ni_ready(me))return 4;
 if(!remote||!ni_ready(remote))return 5;
 int synchronized=0;uint32_t info=rd32(rd32(remote+0x3c)+0x68);
 for(unsigned k=0;k<sizeof NPI.synchronized/sizeof NPI.synchronized[0];k++)if(NPI.synchronized[k].car==remote&&
  NPI.synchronized[k].world==world()&&NPI.synchronized[k].info==info&&NPI.synchronized[k].at&&
  now>=NPI.synchronized[k].at&&now-NPI.synchronized[k].at<100000){synchronized=1;break;}
 if(!synchronized)return 6;
 /* Car+38 changes inside the native solver before the next scene publish.
  * Its valid transient state must not drop the body/ghost latch mid-impact. */
 if(!npi_physics(me,&own)||own!=NPI.physics||rd32(me+0x38)>16)return 7;
 if(!npi_physics(remote,&other)||other!=p.scene.physics)return 8;
 return 0;
}
static int npi_contact_ready(uint32_t peer,int64_t now){return npi_contact_reason(peer,now)==0;}
static void npi_diagnostic(int64_t now){
 uint32_t diagnostic_peer=NPI.peer;
 if(!diagnostic_peer){roam_plat_lock();for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].state==RC_PS_EST&&!g_s.peer[i].lost){diagnostic_peer=g_s.peer[i].id;break;}roam_plat_unlock();}
 uint32_t remote=ni_peer_car(diagnostic_peer),digest=0,expected=0;
 int wire=0,status=-1,remote_active=0,remote_cap=0,clock=0,ext=0,echo=0;
 roam_plat_lock();
 now=roam_plat_now_us();
 for(int i=0;i<RC_TABLE;i++)if(g_s.peer[i].id==diagnostic_peer&&diagnostic_peer){
  rc_peer *p=&g_s.peer[i];wire=rc_player_contact_ready(&g_s,p,now);status=p->status;
  remote_active=p->contact_seen&&p->contact_scene.active;remote_cap=p->player_contact;
  ext=rc_extension_ready(&g_s,p,RC_EXT_PLAYER_IMPULSE,now);clock=p->clock_valid&&p->clock_rtt_us<=200000;
  echo=g_s.contact_scene.world==p->contact_scene.peer_world&&p->contact_scene.world==g_s.contact_scene.peer_world;
  expected=p->contact_scene.physics;break;
 }
 roam_plat_unlock();
 int physical=remote&&npi_physics(remote,&digest),ready=diagnostic_peer&&npi_contact_ready(diagnostic_peer,now);
 uint32_t body=remote?rd32(remote+0x3c):0;
 int reason=-1;int64_t age=-1;
 for(unsigned k=0;k<sizeof NPI.synchronized/sizeof NPI.synchronized[0];k++)if(NPI.synchronized[k].car==remote&&NPI.synchronized[k].world==world()){
  reason=NPI.synchronized[k].reason;if(NPI.synchronized[k].at)age=now-NPI.synchronized[k].at;break;
 }
 rlog("[player-sync-gate] car=%08x reason=%d age-us=%lld body-ready=%d own-proof=%08x contact-reason=%d collision-off=%u\n",remote,reason,(long long)age,remote&&ni_ready(remote),NPI.physics,npi_contact_reason(diagnostic_peer,now),ok(body,0x18)?MR8(body+0x17):0);
 rlog("[player-contact-gate] peer=%08x local-active=%d remote-cap=%d extension=%d wire-ready=%d remote-active=%d status=%d clock=%d world-echo=%d proxy=%08x driver=%u mode=%u full=%u allow-full=%u physics=%08x expected=%08x physics-match=%d synchronized-ready=%d\n",
  diagnostic_peer,NPI.active,remote_cap,ext,wire,remote_active,status,clock,echo,remote,remote?rd32(remote+0x24):0,remote?rd32(remote+0x28):0,body?rd32(body+0x5c):0,remote&&ok(remote,0x5c1)?MR8(remote+0x5c0):0,digest,expected,physical&&digest==expected,ready);
}
static void npi_publish(int64_t now){
 uint32_t car=player_car(),w=world(),run=0,event=0,physics=0,peer=0,nonce=0,echo=0,contact_class=0,ride=0;
 int active=0,enabled=0,paused=0;rc_contact_scene remote={0};
 roam_plat_lock();enabled=g_s.conf.player_contact&&(g_s.conf.extensions&RC_EXT_PLAYER_IMPULSE)!=0;paused=cp_held(&g_s.pause);roam_plat_unlock();
 if(enabled&&!NPI.fault&&rd32(G_FLOW)==6&&w&&ok(car,0x9e0)&&ni_ready(car)&&!paused&&!nc_pause_raw(0)){
  int scene=roaming();
  if(NC.bound&&NC.control.phase==CC_RUNNING&&nc_same_world()){scene=1;run=NC.control.run;event=NC.control.event;}
  ride=rd32(car+0x10);contact_class=rd32(car+0x38);
  active=scene&&contact_class<=16&&npi_physics(car,&physics);
 }
 roam_plat_lock();
 now=roam_plat_now_us();
 if(enabled)for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];
  if(!p->player_contact||!rc_extension_ready(&g_s,p,RC_EXT_PLAYER_IMPULSE,now)||
     (!!g_s.conf.world_host==!!(p->extensions.features&RC_EXT_CONTACT_HOST)))continue;
  if(peer){peer=0;break;}peer=p->id;nonce=p->extensions.nonce;
  if(p->contact_seen&&now>=p->contact_at&&now-p->contact_at<350000){remote=p->contact_scene;echo=remote.world;}
 }
 roam_plat_unlock();
 float position[3]={0};if(ok(car,0x70))rd3(car+0x60,position);
 int changed=!NPI.token||NPI.world!=w||NPI.car!=car||NPI.ride!=ride||NPI.run!=run||NPI.event!=event||
  NPI.physics!=physics||NPI.peer!=peer||NPI.nonce!=nonce||NPI.active!=active||
  (NPI.frame_at&&(now<NPI.frame_at||now-NPI.frame_at>250000))||nw_dist2(position,NPI.position)>JUMP_M*JUMP_M;
 if(changed){
  for(int k=0;k<16;k++)if(NPI.pending[k].active)rlog("[player-impulse-drop] scene changed: active=%d/%d physics=%08x/%08x class=%u/%u peer=%08x/%08x nonce=%08x/%08x gap=%lld jump2=%.3f\n",NPI.active,active,NPI.physics,physics,NPI.contact_class,contact_class,NPI.peer,peer,NPI.nonce,nonce,(long long)(now-NPI.frame_at),(double)nw_dist2(position,NPI.position));
  if(!NPI.next_token)NPI.next_token=roam_plat_random();if(++NPI.next_token==0)++NPI.next_token;
  NPI.token=NPI.next_token;
  for(int k=0;k<16;k++)if(NPI.pending[k].active){NPI.pending[k].active=0;NPI.dropped++;}
 }
 NPI.world=w;NPI.car=car;NPI.ride=ride;NPI.run=run;NPI.event=event;NPI.physics=physics;NPI.contact_class=contact_class;
 NPI.peer=peer;NPI.nonce=nonce;NPI.active=active;NPI.frame_at=now;memcpy(NPI.position,position,sizeof position);
 roam_plat_lock();
 if(peer){
  rc_contact_scene s={peer,g_s.conf.extension_nonce,nonce,0,NPI.token,echo,run,event,physics,contact_class,(uint8_t)active};
  if(!rc_publish_contact_scene(&g_s,&s,now))rc_clear_contact_scene(&g_s,now);
 }else rc_clear_contact_scene(&g_s,now);
 roam_plat_unlock();
}
uint32_t npi_before(uint32_t body,float before[6]){
 if(!NPI.scope.active||NPI.scope.body!=body||NPI.scope.world!=world()||
    rd32(NPI.scope.car+0x3c)!=body||!ni_ready(NPI.scope.car))return 0;
 uint32_t info=rd32(body+0x68);
 if(info!=NPI.scope.info){NPI.scope.fault=1;return 0;}
 for(int k=0;k<3;k++){before[k]=MRF32(info+0x70+4*k);before[k+3]=MRF32(info+0x80+4*k);}
 for(int k=0;k<6;k++)if(!__builtin_isfinite(before[k])){NPI.scope.fault=1;return 0;}
 return info;
}
void npi_after(uint32_t body,uint32_t info,const float before[6]){
 if(!NPI.scope.active||NPI.scope.body!=body)return;
 if(NPI.scope.world!=world()||rd32(NPI.scope.car+0x3c)!=body||!ni_ready(NPI.scope.car)||rd32(body+0x68)!=info){NPI.scope.fault=1;return;}
 for(int k=0;k<3;k++){
  float dp=MRF32(info+0x70+4*k)-before[k],dl=MRF32(info+0x80+4*k)-before[k+3];
  if(!__builtin_isfinite(dp)||!__builtin_isfinite(dl)){NPI.scope.fault=1;return;}
  NPI.scope.dp[k]+=dp;NPI.scope.dl[k]+=dl;
  if(!__builtin_isfinite(NPI.scope.dp[k])||!__builtin_isfinite(NPI.scope.dl[k])){NPI.scope.fault=1;return;}
 }
}
void ni_solve(void *pair,ni_solver_fn original){
 __typeof__(NPI.scope) saved=NPI.scope;__typeof__(NIP.scope) traffic=NIP.scope;
 memset(&NPI.scope,0,sizeof NPI.scope);memset(&NIP.scope,0,sizeof NIP.scope);
 uint32_t object=(uint32_t)(uintptr_t)pair;int handled=0;int64_t now=roam_plat_now_us();
 if(g_s.conf.player_contact&&ok(object,0x18)){
  uint32_t a=ni_car(rd32(object+0xc)),b=ni_car(rd32(object+0x10)),me=player_car();
  uint32_t car=a==me?b:b==me?a:0,peer=car?ni_remote_player(car):0;
  if(peer){
   handled=1;npi_peer_info p;
   if(g_s.conf.world_host&&npi_contact_ready(peer,now)&&npi_peer(peer,now,&p)){
    int slot=-1;
    for(int k=0;k<16;k++)if(!NPI.pending[k].active&&slot<0)slot=k;
    if(!NPI.token_at){NPI.token_at=now;NPI.tokens=8;}
    float elapsed=now>=NPI.token_at?(float)(now-NPI.token_at)/1000000.f:0;
    NPI.tokens+=elapsed*20;if(NPI.tokens>8)NPI.tokens=8;NPI.token_at=now;
    if(slot>=0&&NPI.tokens>=1){
     rc_player_impulse h={0};h.to=peer;h.source=g_s.conf.extension_nonce;h.destination=p.nonce;
     h.host_world=NPI.token;h.guest_world=p.scene.world;h.run=NPI.run;h.event=NPI.event;h.physics=p.scene.physics;
     h.op=RC_IMPULSE_DATA;h.t_us=(uint32_t)now;rd3(car+0x60,h.target_p);rd3(me+0x60,h.host_p);
     uint32_t old_class=rd32(car+0x38),record=rd32(car+0x14),ride=rd32(car+0x10),body=rd32(car+0x3c);
     NPI.scope.body=body;NPI.scope.car=car;NPI.scope.world=world();NPI.scope.info=rd32(body+0x68);NPI.scope.active=1;
     /* The native impulse/inertia methods branch on Car+38. Borrow the
      * owner's exact small native contact class for this pair only. */
     MR32(car+0x38)=p.scene.contact_class;NPI.tokens-=1;
     original(pair);
     if(world()==NPI.scope.world&&ok(car,0x9e0)&&rd32(car+0x14)==record&&rd32(car+0x10)==ride)MR32(car+0x38)=old_class;
     else NPI.scope.fault=1;
     if(NPI.scope.fault||nw_len2(NPI.scope.dp)>100000000.f||nw_len2(NPI.scope.dl)>10000.f){
      NPI.fault=1;NPI.dropped++;rlog("[player-impulse] native contact changed identity/state; disabling contacts until restart\n");
     }
     else if(nw_len2(NPI.scope.dp)+nw_len2(NPI.scope.dl)>.0000001f){
      memcpy(h.dp,NPI.scope.dp,sizeof h.dp);memcpy(h.dl,NPI.scope.dl,sizeof h.dl);h.seq=++NPI.seq;
      NPI.pending[slot].hit=h;NPI.pending[slot].active=1;NPI.pending[slot].born=now;NPI.pending[slot].next=now;NPI.captured++;
     }else NPI.tokens+=1;
    }else NPI.dropped++;
   }
   /* Guest and unnegotiated/stale pairs never run a second local solver. */
  }
 }
 if(!handled)ni_traffic_solve(pair,original);
 NPI.scope=saved;NIP.scope=traffic;
}
static void npi_ack(uint32_t peer,const rc_player_impulse *h,int64_t now){
 rc_player_impulse ack=*h;ack.op=RC_IMPULSE_ACK;ack.to=peer;ack.source=h->destination;ack.destination=h->source;
 roam_plat_lock();now=roam_plat_now_us();rc_send_player_impulse(&g_s,&ack,now);roam_plat_unlock();
}
static void npi_receive(CPU *c,uint32_t from,const rc_player_impulse *h,int64_t now){
 NPI.rejected++;npi_peer_info peer;
 if(g_s.conf.world_host||!npi_peer(from,now,&peer)||!peer.position||!NPI.active||NPI.world!=world()||
    h->to!=g_s.conf.self_id||h->source!=peer.nonce||h->destination!=g_s.conf.extension_nonce||
    h->guest_world!=NPI.token||h->host_world!=peer.scene.world||h->run!=NPI.run||h->event!=NPI.event||h->physics!=NPI.physics)return;
 int32_t age=(int32_t)((uint32_t)now-h->t_us-(uint32_t)peer.offset);if(age< -100000||age>500000)return;
 uint32_t car=player_car(),physics=0;
 if(car!=NPI.car||!ni_ready(car)||!npi_physics(car,&physics)||physics!=h->physics||rd32(car+0x38)>16)return;
 __typeof__(NPI.replay) *r=&NPI.replay;
 if(r->peer!=from||r->nonce!=h->source||r->host_world!=h->host_world||r->guest_world!=h->guest_world){
  memset(r,0,sizeof *r);r->peer=from;r->nonce=h->source;r->host_world=h->host_world;r->guest_world=h->guest_world;
 }
 unsigned ahead=(uint16_t)(h->seq-r->seq);uint64_t bit=1;
 if(r->seen&&ahead>=0x8000u){unsigned behind=(uint16_t)(r->seq-h->seq);if(behind>=64)return;bit=(uint64_t)1<<behind;}
 if(r->seen&&(!ahead||(ahead>=0x8000u&&(r->seen&bit)))){NPI.rejected--;NPI.repeated++;npi_ack(from,h,now);return;}
 float here[3],v[3];rd3(car+0x60,here);rd3(car+0x70,v);
 float tolerance=6.f+rc_sqrtf(nw_len2(v))*(age>0?(float)age/1000000.f:0);
 float peer_tolerance=6.f+rc_sqrtf(nw_len2(peer.remote.v))*(age>0?(float)age/1000000.f:0);
 if(nw_dist2(h->host_p,h->target_p)>64.f||nw_dist2(here,h->target_p)>tolerance*tolerance||
    nw_dist2(peer.remote.p,h->host_p)>peer_tolerance*peer_tolerance)return;
 uint32_t body=rd32(car+0x3c),remote=ni_peer_car(from),other=rd32(remote+0x3c);
 if(!remote||ni_car(other)!=remote)return;
 /* Publish the same native contact notification for the real owner, before
  * applying its measured momentum. Never run the client's pair solver. */
 gcall1(c,0x5938b0,body,other);
 /* The callback may replace/destroy a body or change scenes. Do not
  * apply this contact to either a retired body or its new replacement. */
 if(world()!=NPI.world||player_car()!=car||rd32(car+0x3c)!=body||
    ni_peer_car(from)!=remote||rd32(remote+0x3c)!=other||
    !ni_ready(car)||!npi_physics(car,&physics)||physics!=h->physics||rd32(car+0x38)>16)return;
 uint32_t info=rd32(body+0x68);float before[6],result[6];
 for(int k=0;k<3;k++){
  before[k]=MRF32(info+0x70+4*k);before[k+3]=MRF32(info+0x80+4*k);
  result[k]=before[k]+h->dp[k];result[k+3]=before[k+3]+h->dl[k];
 }
 for(int k=0;k<6;k++)if(!__builtin_isfinite(result[k])||__builtin_fabsf(result[k])>1000000.f)return;
 for(int k=0;k<3;k++){MRF32(info+0x70+4*k)=result[k];MRF32(info+0x80+4*k)=result[k+3];}
 gcall0(c,0x593670,body);
 if(world()!=NPI.world||player_car()!=car||rd32(car+0x3c)!=body||!ni_ready(car)||rd32(body+0x68)!=info){NPI.fault=1;return;}
 float velocity[3],angular[3];rd3(info+0xa0,velocity);rd3(info+0xc0,angular);int valid=1;
 for(int k=0;k<3;k++)if(!__builtin_isfinite(velocity[k])||!__builtin_isfinite(angular[k]))valid=0;
 if(!valid||nw_len2(velocity)>40000.f||nw_len2(angular)>400.f){
  for(int k=0;k<3;k++){MRF32(info+0x70+4*k)=before[k];MRF32(info+0x80+4*k)=before[k+3];}
  gcall0(c,0x593670,body);return;
 }
 if(!r->seen||ahead<0x8000u){r->seen=!r->seen||ahead>=64?1:(r->seen<<ahead)|1;r->seq=h->seq;}
 else r->seen|=bit;
 NPI.rejected--;NPI.applied++;npi_ack(from,h,now);
}
static void npi_frame(CPU *c,int64_t now){
 /* Pose synchronization above sampled the monotonic clock after this
  * frame's original timestamp. Evaluate its lease and queued impulses
  * against a fresh sample, never the earlier frame-start time. */
 now=roam_plat_now_us();
 roam_plat_lock();int enabled=g_s.conf.player_contact&&(g_s.conf.extensions&RC_EXT_PLAYER_IMPULSE)!=0;
 if(!enabled)rc_clear_contact_scene(&g_s,now);roam_plat_unlock();
 if(!enabled){
  for(int k=0;k<16;k++)if(NPI.pending[k].active){NPI.pending[k].active=0;NPI.dropped++;}
  NPI.active=0;NPI.peer=0;return;
 }
 npi_publish(now);rc_player_impulse h;uint32_t from;
 for(int i=0;i<RC_PLAYER_IMPULSE_INBOX;i++){
  roam_plat_lock();int got=rc_take_player_impulse(&g_s,&from,&h);roam_plat_unlock();if(!got)break;
  if(h.op==RC_IMPULSE_DATA)npi_receive(c,from,&h,now);
  else if(h.op==RC_IMPULSE_ACK&&g_s.conf.world_host)for(int k=0;k<16;k++){
   rc_player_impulse *old=&NPI.pending[k].hit;
   if(NPI.pending[k].active==1&&old->to==from&&old->host_world==h.host_world&&old->guest_world==h.guest_world&&
      old->seq==h.seq&&old->source==h.destination&&old->destination==h.source){NPI.pending[k].active=2;NPI.pending[k].after_ack=now;}
  }
 }
 for(int k=0;k<16;k++)if(NPI.pending[k].active){
  rc_player_impulse *old=&NPI.pending[k].hit;
  if(NPI.pending[k].active==2){
   npi_peer_info p;
   if(!NPI.active||old->host_world!=NPI.token||old->to!=NPI.peer||old->destination!=NPI.nonce||
      now<NPI.pending[k].after_ack||now-NPI.pending[k].after_ack>500000||
      (npi_peer(old->to,now,&p)&&p.position&&p.source_at>NPI.pending[k].after_ack))NPI.pending[k].active=0;
   continue;
  }
  if(!NPI.active||old->host_world!=NPI.token||old->to!=NPI.peer||old->destination!=NPI.nonce||
     now<NPI.pending[k].born||now-NPI.pending[k].born>500000){
   rlog("[player-impulse-drop] pending expired/scope: seq=%u active=%d worlds=%u/%u peer=%08x/%08x nonce=%08x/%08x age=%lld\n",old->seq,NPI.active,old->host_world,NPI.token,old->to,NPI.peer,old->destination,NPI.nonce,(long long)(now-NPI.pending[k].born));
   NPI.pending[k].active=0;NPI.dropped++;continue;
  }
  if(now<NPI.next_send||now<NPI.pending[k].next)continue;
  /* npi_publish and packet receive can publish leases newer than the frame
   * timestamp. Read the comparison clock after taking their shared lock. */
  roam_plat_lock();now=roam_plat_now_us();int sent=rc_send_player_impulse(&g_s,old,now);roam_plat_unlock();
  rlog("[player-impulse-send] seq=%u sent=%d age=%lld\n",old->seq,sent,(long long)(now-NPI.pending[k].born));
  NPI.pending[k].next=now+70000;NPI.next_send=now+33334;
 }
 static int64_t next_log;
 if(now>=next_log){
  int pending=0;for(int k=0;k<16;k++)pending+=NPI.pending[k].active!=0;
  rlog("[player-impulse] world=%u active=%d fault=%d captured=%u applied=%u repeated=%u rejected=%u dropped=%u pending=%d\n",NPI.token,NPI.active,NPI.fault,NPI.captured,NPI.applied,NPI.repeated,NPI.rejected,NPI.dropped,pending);
  npi_diagnostic(now);
  next_log=now+5000000;
 }
}
