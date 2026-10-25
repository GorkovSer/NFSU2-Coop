int rc_extension_ready(const rc_session *s,const rc_peer *p,uint32_t feature,int64_t now){
 return !s->closed&&(s->conf.extensions&feature)==feature&&s->conf.extension_nonce&&
  p->state==RC_PS_EST&&!p->lost&&p->core_ver>=4&&p->extension_seen&&
  p->extensions.echo==s->conf.extension_nonce&&
  (p->extensions.features&feature)==feature&&now>=p->extension_at&&now-p->extension_at<1500000;
}
static int ci_roles(const rc_session *s,const rc_peer *p,int op){
 return s->conf.traffic&&s->conf.traffic_contact&&p->traffic&&p->traffic_contact&&
  (op==RC_IMPULSE_DATA?(!s->conf.world_host&&p->world_host):(s->conf.world_host&&!p->world_host));
}
static int ci_budget(rc_session *s,int64_t now){
 if(!s->contact_budget_at){s->contact_budget_at=now?now:1;s->contact_budget=8.f;}
 float dt=now>=s->contact_budget_at?(float)(now-s->contact_budget_at)/1000000.f:0;
 float rate=s->conf.coop&&(s->conf.extensions&RC_EXT_RACE_MODES)?30.f:40.f;
 s->contact_budget+=dt*rate;if(s->contact_budget>8.f)s->contact_budget=8.f;s->contact_budget_at=now?now:1;
 if(s->contact_budget<1.f)return 0;
 s->contact_budget-=1.f;return 1;
}
int rc_send_impulse(rc_session *s,const rc_impulse *h,int64_t now){
 if(!ci_valid(h)||h->source!=s->conf.extension_nonce)return 0;
 for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&s->peer[i];
  if(p->id!=h->to||!rc_extension_ready(s,p,RC_EXT_TRAFFIC_IMPULSE,now)||
     h->destination!=p->extensions.nonce||!ci_roles(s,p,h->op))continue;
  if(!ci_budget(s,now))return 0;
  rc_msg m;hdr(s,&m,RC_MSG_IMPULSE);m.u.impulse=*h;uint32_t before=s->tx;tx(s,p,&m);return before!=s->tx;
 }
 return 0;
}
int rc_take_impulse(rc_session *s,uint32_t *from,rc_impulse *h){
 if(!s->n_impulse)return 0;
 *from=s->impulse_in[0].from;*h=s->impulse_in[0].message;
 for(int i=1;i<s->n_impulse;i++)s->impulse_in[i-1]=s->impulse_in[i];
 s->n_impulse--;return 1;
}
static void ci_extensions_receive(rc_session *s,rc_peer *p,const rc_extensions *e,int64_t now){
 if(p->state!=RC_PS_EST||p->core_ver<4||!s->conf.extension_nonce||e->to!=s->conf.self_id)return;
 /* A serial belongs to this peer connection; duplicates never renew a lease.
  * The destination nonce in data also rejects an earlier local ASI session. */
 if(p->extension_seen){uint32_t d=e->serial-p->extensions.serial;if(!d||d>=0x80000000u)return;}
 if(p->extensions.nonce!=e->nonce||((p->extensions.features^e->features)&(RC_EXT_PLAYER_IMPULSE|RC_EXT_CONTACT_HOST))){p->contact_seen=0;p->contact_fault_world=0;}
 p->extensions=*e;p->extension_at=now;p->extension_seen=1;
}
static void ci_receive(rc_session *s,rc_peer *p,const rc_impulse *h,int64_t now){
 if(!rc_extension_ready(s,p,RC_EXT_TRAFFIC_IMPULSE,now)||h->to!=s->conf.self_id||
    h->destination!=s->conf.extension_nonce||h->source!=p->extensions.nonce)return;
 /* Receive is the opposite of local send authority. */
 if(!ci_roles(s,p,h->op==RC_IMPULSE_DATA?RC_IMPULSE_ACK:RC_IMPULSE_DATA))return;
 for(int i=0;i<s->n_impulse;i++){
  const rc_impulse *old=&s->impulse_in[i].message;
  if(s->impulse_in[i].from==p->id&&old->op==h->op&&old->world==h->world&&
     old->context==h->context&&old->id==h->id&&old->seq==h->seq&&old->source==h->source)return;
 }
 if(s->n_impulse==RC_IMPULSE_INBOX){s->impulse_dropped++;return;}
 s->impulse_in[s->n_impulse].from=p->id;s->impulse_in[s->n_impulse++].message=*h;
}
static void ci_tick(rc_session *s,rc_peer *p,int64_t now){
 if(!s->conf.extensions||!s->conf.extension_nonce||p->core_ver<4||p->state!=RC_PS_EST||p->lost)return;
 if(p->extension_tx&&now-p->extension_tx<500000)return;
 rc_msg m;hdr(s,&m,RC_MSG_EXTENSIONS);
 m.u.extensions=(rc_extensions){p->id,s->conf.extension_nonce,p->extension_seen?p->extensions.nonce:0,s->conf.extensions,++s->extension_serial};
 tx(s,p,&m);p->extension_tx=now?now:1;
}
