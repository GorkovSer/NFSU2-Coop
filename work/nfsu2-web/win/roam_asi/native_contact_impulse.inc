/* Exact pair-scoped Body::ApplyImpulse arguments. Optional/default off.
 * Only the local HUMAN/traffic pair is observed on the client. The host
 * invokes the native impulse method once, never adds to a cached velocity. */
static struct {
 struct {uint32_t body,car,peer,id,world,context,source,destination;} scope;
 struct {rc_impulse hit;int active;int64_t born,next;} pending[16];
 struct {uint32_t peer,nonce,world;uint16_t seq;uint64_t seen;float tokens;int64_t at;} replay[RC_TABLE];
 uint16_t seq;float tokens;int64_t token_at,next_send;
 uint32_t captured,applied,repeated,dropped,rejected;
} NIP;
static void ni_forget_pending(void){
 for(int k=0;k<16;k++)if(NIP.pending[k].active){NIP.pending[k].active=0;NIP.dropped++;}
 memset(&NIP.scope,0,sizeof NIP.scope);
 /* Preserve applied identities across pause/resume: clearing replay state
  * would let an old retry apply an already accepted impulse a second time. */
}
typedef struct {uint32_t nonce;int index;int32_t offset;rc_state remote;int position;} ni_peer_info;
static int ni_peer(uint32_t id,int host,int64_t now,ni_peer_info *out){
 int found=0;memset(out,0,sizeof *out);
 roam_plat_lock();
 now=roam_plat_now_us();
 if(!cp_held(&g_s.pause))for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&g_s.peer[i];
  if(p->id!=id||p->world_host!=host||!p->traffic_contact||
     !rc_extension_ready(&g_s,p,RC_EXT_TRAFFIC_IMPULSE,now)||(p->status!=RC_ST_ROAM&&p->status!=RC_ST_EVENT)||
     !p->clock_valid||p->clock_rtt_us>200000||now<p->clock_sample_at||now-p->clock_sample_at>10000000)continue;
  out->nonce=p->extensions.nonce;out->index=i;out->offset=p->clock_offset_us;found=1;
  if(p->status==RC_ST_ROAM&&p->mo.n&&now>=p->last_snap_rx&&now-p->last_snap_rx<500000){out->remote=p->mo.s[p->mo.n-1];out->position=1;}
  break;
 }
 roam_plat_unlock();
 if(found&&NC.bound&&NC.peer==id&&NC.control.phase==CC_RUNNING&&!cs_stale(&NC.stream,now)){
  int slot=NC.host?1:0;rc_interp *motion=&NC.stream.motion[slot];
  if(motion->n){out->remote=motion->s[motion->n-1];out->position=1;}
 }
 return found;
}
static int ni_scene(void){return g_s.conf.traffic_contact&&NW.enabled&&NW.world==world()&&rd32(G_FLOW)==6;}
static uint32_t ni_car(uint32_t body){
 if(!ok(body,0x134)||rd32(body)!=0x7a2478u||MR8(body+0x11)!=0)return 0;
 uint32_t car=rd32(body+0x130);
 return ok(car,0x9e0)&&rd32(car+0x3c)==body?car:0;
}
static int ni_ready(uint32_t car){
 uint32_t body=rd32(car+0x3c),physics=rd32(car+0x34),info=rd32(body+0x68);
 return ni_car(body)==car&&rd32(car+0x28)&&ok(physics,0x28)&&rd32(body+0x5c)&&
  ok(rd32(body+0x64),0x10)&&ok(info,0x1f0);
}
static uint32_t ni_remote_player(uint32_t car){
 if(NC.bound&&nc_same_world()&&car==NC.car[NC.host?1:0])return NC.peer;
 for(int k=0;k<NSLOT;k++)if(g_sl[k].car==car&&g_sl[k].world==world()&&g_sl[k].st==SL_ACTIVE)return g_sl[k].peer_id;
 return 0;
}
static uint32_t ni_peer_car(uint32_t peer){
 if(NC.bound&&NC.peer==peer&&nc_same_world())return NC.car[NC.host?1:0];
 for(int k=0;k<NSLOT;k++)if(g_sl[k].peer_id==peer&&g_sl[k].world==world()&&g_sl[k].st==SL_ACTIVE)return g_sl[k].car;
 return 0;
}
static void ni_traffic_solve(void *pair,ni_solver_fn original){
 /* Save and restore scope, including nested ordinary contacts. */
 __typeof__(NIP.scope) saved=NIP.scope;memset(&NIP.scope,0,sizeof NIP.scope);
 uint32_t object=(uint32_t)(uintptr_t)pair;int suppress=0;int64_t now=roam_plat_now_us();
 /* A native update can replace/withdraw a render attachment after the
  * adapter enabled its body. Recheck at the actual solve boundary. */
 if(ok(object,0x18)){
  uint32_t a=ni_car(rd32(object+0xc)),b=ni_car(rd32(object+0x10));
  if((a&&!nw_contact_visible(a,now))||(b&&!nw_contact_visible(b,now))){
   static int64_t next_log;
   if(now>=next_log){next_log=now+1000000;rlog("[traffic-contact-gate] rejected stale/unassigned/unattached client pair %08x/%08x\n",a,b);}
   NIP.scope=saved;return;
  }
 }
 if(ni_scene()&&ok(object,0x18)){
  uint32_t bodies[2]={rd32(object+0xc),rd32(object+0x10)};
  uint32_t cars[2]={ni_car(bodies[0]),ni_car(bodies[1])};
  for(int side=0;side<2;side++)if(cars[side]&&cars[1-side]){
   uint32_t id=0;
   if(NW.host){for(int k=0;k<RC_TRAFFIC_CARS;k++)if(NW.host_car[k].active&&NW.host_car[k].car==cars[side]){id=NW.host_car[k].id;break;}}
   else if(NW.owned&&now>=NW.client.last_rx&&now-NW.client.last_rx<300000){
    for(int k=0;k<RC_TRAFFIC_CARS;k++)if(NW.replica[k].car==cars[side]){id=NW.replica[k].id;break;}
   }
   if(!id)continue;
   ni_peer_info peer;uint32_t other=ni_remote_player(cars[1-side]);
   /* Its owner evaluates this pair. Do not also solve the proxy pair here. */
   if(other&&ni_peer(other,!NW.host,now,&peer)){suppress=1;break;}
   /* The original pair wrapper promotes a kinematic body before solving.
    * Observe only after its impulse method returns with valid full links. */
   if(!NW.host&&cars[1-side]==player_car()&&ni_peer(NW.peer,1,now,&peer)){
    NIP.scope.body=bodies[side];NIP.scope.car=cars[side];NIP.scope.peer=NW.peer;NIP.scope.id=id;
    NIP.scope.world=NW.client.world;NIP.scope.context=NW.context;
    NIP.scope.source=g_s.conf.extension_nonce;NIP.scope.destination=peer.nonce;break;
   }
  }
 }
 if(!suppress)original(pair);
 NIP.scope=saved;
}
void ni_observe(uint32_t body,const float *impulse,const float *arm){
 int64_t now=roam_plat_now_us();
 if(!NIP.scope.body||body!=NIP.scope.body||!ni_scene()||NW.host||!NW.owned||
    NIP.scope.car!=ni_car(body)||!ni_ready(NIP.scope.car)||NIP.scope.world!=NW.client.world||
    NIP.scope.context!=NW.context||!impulse||!arm)return;
 float j2=0,r2=0;
 for(int k=0;k<3;k++){
  if(!__builtin_isfinite(impulse[k])||!__builtin_isfinite(arm[k]))return;
  j2+=impulse[k]*impulse[k];r2+=arm[k]*arm[k];
 }
 if(j2<.000001f||j2>1.e12f||r2>144.f)return;
 if(!NIP.token_at){NIP.token_at=now;NIP.tokens=8.f;}
 float elapsed=now>=NIP.token_at?(float)(now-NIP.token_at)/1000000.f:0;
 NIP.tokens+=elapsed*20.f;if(NIP.tokens>8.f)NIP.tokens=8.f;NIP.token_at=now;
 int slot=-1;for(int i=0;i<16;i++)if(!NIP.pending[i].active){slot=i;break;}
 if(slot<0||NIP.tokens<1.f){NIP.dropped++;return;}
 rc_impulse *h=&NIP.pending[slot].hit;memset(h,0,sizeof *h);
 h->op=RC_IMPULSE_DATA;h->to=NIP.scope.peer;h->source=NIP.scope.source;h->destination=NIP.scope.destination;
 h->world=NIP.scope.world;h->context=NIP.scope.context;h->id=NIP.scope.id;h->seq=++NIP.seq;h->t_us=(uint32_t)now;
 rd3(NIP.scope.car+0x60,h->p);rd3(player_car()+0x60,h->player);
 memcpy(h->impulse,impulse,sizeof h->impulse);memcpy(h->arm,arm,sizeof h->arm);
 NIP.pending[slot].active=1;NIP.pending[slot].born=now;NIP.pending[slot].next=now;
 NIP.tokens-=1.f;NIP.captured++;
}
static void ni_ack(uint32_t from,const rc_impulse *h,int64_t now){
 rc_impulse ack=*h;ack.op=RC_IMPULSE_ACK;ack.to=from;ack.source=h->destination;ack.destination=h->source;
 roam_plat_lock();now=roam_plat_now_us();rc_send_impulse(&g_s,&ack,now);roam_plat_unlock();
}
static void ni_host_receive(CPU *c,uint32_t from,const rc_impulse *h,int64_t now){
 ni_peer_info peer;NIP.rejected++;
 if(!ni_peer(from,0,now,&peer)||!peer.position||h->source!=peer.nonce||h->destination!=g_s.conf.extension_nonce||
    h->to!=g_s.conf.self_id||h->world!=NW.generation||h->context!=NW.context)return;
 int32_t age=(int32_t)((uint32_t)now-h->t_us-(uint32_t)peer.offset);
 if(age< -100000||age>500000||nw_dist2(h->p,h->player)>42.25f)return;
 float tolerance=8.f+0.5f*rc_sqrtf(nw_len2(peer.remote.v));
 if(nw_dist2(peer.remote.p,h->player)>tolerance*tolerance)return;
 uint32_t car=0,type=0;
 for(int k=0;k<RC_TRAFFIC_CARS;k++)if(NW.host_car[k].active&&NW.host_car[k].id==h->id){car=NW.host_car[k].car;type=NW.host_car[k].type;break;}
 rc_state state;
 if(!car||!ok(car,0x9e0)||!MR8(car+0x550)||!nw_traffic_driver(rd32(car+0x24))||nw_type(car)!=type||!nc_read_car(car,&state,now))return;
 tolerance=8.f+0.5f*rc_sqrtf(nw_len2(state.v));
 if(nw_dist2(state.p,h->p)>tolerance*tolerance||__builtin_fabsf(state.p[2]-h->p[2])>5.f)return;
 __typeof__(NIP.replay[0]) *r=&NIP.replay[peer.index];
 if(r->peer!=from||r->nonce!=h->source||r->world!=NW.generation){memset(r,0,sizeof *r);r->peer=from;r->nonce=h->source;r->world=NW.generation;r->tokens=8.f;r->at=now;}
 unsigned ahead=(uint16_t)(h->seq-r->seq);uint64_t bit=1;
 if(r->seen&&ahead>=0x8000u){unsigned behind=(uint16_t)(r->seq-h->seq);if(behind>=64)return;bit=(uint64_t)1<<behind;}
 if(r->seen&&((!ahead)||(ahead>=0x8000u&&(r->seen&bit)))){NIP.rejected--;NIP.repeated++;ni_ack(from,h,now);return;}
 float tokens=r->tokens+(now>=r->at?(float)(now-r->at)/1000000.f:0)*20.f;if(tokens>8.f)tokens=8.f;
 if(tokens<1.f)return;
 uint32_t body=rd32(car+0x3c);
 if(!ni_car(body)||MR8(body+0x17))return;
 {
  uint32_t remote=ni_peer_car(from),other=rd32(remote+0x3c);
  if(!remote||ni_car(other)!=remote)return;
  uint32_t old_mode=rd32(car+0x28);
  /* Same notification as the original pair wrapper. It lets the traffic
   * brain choose its dynamic mode and rebuilds Body's native physics links.
   * Direct SetPhysics(3) alone can be remapped to a kinematic mode by policy. */
  body_set(c,car,0);gcall1(c,0x5938b0,body,other);
  if(!ni_ready(car)){
   /* A native policy can select a different mode. Restore its old mode
    * instead of leaving an ordinary traffic body disabled after refusal. */
   gcall1(c,FN_CAR_SETPHYS,car,old_mode);body=rd32(car+0x3c);
   if(ni_car(body)==car&&rd32(car+0x28)&&(!rd32(body+0x5c)||ni_ready(car)))body_set(c,car,1);
   return;
  }
  body_set(c,car,1);body=rd32(car+0x3c);
 }
 /* Original method updates native momentum/inertia, including the game's
  * own mass scaling and clamps. Never write Car's cached v/w fields here. */
 ni_apply(body,h->impulse,h->arm);
 /* Same derived-physics refresh used after the native pair solver. */
 gcall0(c,0x593670,body);
 if(!r->seen||ahead<0x8000u){r->seen=!r->seen||ahead>=64?1:(r->seen<<ahead)|1;r->seq=h->seq;}
 else r->seen|=bit;
 r->tokens=tokens-1.f;r->at=now;NIP.rejected--;NIP.applied++;ni_ack(from,h,now);
}
static void ni_frame(CPU *c,int64_t now){
 if(!ni_scene())return;
 rc_impulse h;uint32_t from;
 for(int i=0;i<RC_IMPULSE_INBOX;i++){
  roam_plat_lock();int got=rc_take_impulse(&g_s,&from,&h);roam_plat_unlock();if(!got)break;
  if(NW.host&&h.op==RC_IMPULSE_DATA)ni_host_receive(c,from,&h,now);
  else if(!NW.host&&h.op==RC_IMPULSE_ACK)for(int k=0;k<16;k++){
   rc_impulse *old=&NIP.pending[k].hit;
   if(NIP.pending[k].active&&from==old->to&&h.world==old->world&&h.context==old->context&&
      h.id==old->id&&h.seq==old->seq&&h.source==old->destination&&h.destination==old->source)NIP.pending[k].active=0;
  }
 }
 for(int k=0;k<16;k++)if(NIP.pending[k].active){
  rc_impulse *old=&NIP.pending[k].hit;
  if(NW.host||!NW.owned||old->to!=NW.peer||old->world!=NW.client.world||old->context!=NW.context||
     now<NIP.pending[k].born||now-NIP.pending[k].born>500000){NIP.pending[k].active=0;NIP.dropped++;continue;}
  if(now<NIP.next_send||now<NIP.pending[k].next)continue;
  roam_plat_lock();now=roam_plat_now_us();rc_send_impulse(&g_s,old,now);roam_plat_unlock();
  NIP.pending[k].next=now+70000;NIP.next_send=now+20000;
 }
 static int64_t next_log;
 if(now>=next_log){
  int pending=0;for(int k=0;k<16;k++)pending+=NIP.pending[k].active!=0;
  rlog("[traffic-impulse] captured=%u applied=%u repeated=%u rejected=%u dropped=%u pending=%d\n",NIP.captured,NIP.applied,NIP.repeated,NIP.rejected,NIP.dropped,pending);
  next_log=now+5000000;
 }
}
