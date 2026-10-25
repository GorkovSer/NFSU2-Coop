static int pi_roles(const rc_session *s,const rc_peer *p,int op){
 int host=!!s->conf.world_host,peer_host=!!(p->extensions.features&RC_EXT_CONTACT_HOST);
 return host!=peer_host&&(op==RC_IMPULSE_DATA?host:!host);
}
int rc_player_contact_ready(const rc_session *s,const rc_peer *p,int64_t now){
 const rc_contact_scene *a=&s->contact_scene,*b=&p->contact_scene;
 return s->conf.player_contact&&p->player_contact&&
  rc_extension_ready(s,p,RC_EXT_PLAYER_IMPULSE,now)&&!cp_held(&s->pause)&&
  (!!s->conf.world_host!=!!(p->extensions.features&RC_EXT_CONTACT_HOST))&&
  (s->status==RC_ST_ROAM||s->status==RC_ST_EVENT)&&(p->status==RC_ST_ROAM||p->status==RC_ST_EVENT)&&
  a->active&&p->contact_seen&&b->active&&now>=s->contact_frame_at&&now-s->contact_frame_at<=250000&&
  now>=p->contact_at&&now-p->contact_at<350000&&
  a->to==p->id&&a->source==s->conf.extension_nonce&&a->destination==p->extensions.nonce&&
  b->to==s->conf.self_id&&b->source==p->extensions.nonce&&b->destination==s->conf.extension_nonce&&
  a->world==b->peer_world&&b->world==a->peer_world&&a->run==b->run&&a->event==b->event&&a->physics&&b->physics;
}
int rc_publish_contact_scene(rc_session *s,const rc_contact_scene *scene,int64_t now){
 if(!pi_scene_valid(scene)||scene->source!=s->conf.extension_nonce)return 0;
 const rc_contact_scene *old=&s->contact_scene;
 if(old->world==scene->world&&old->to==scene->to&&old->source==scene->source&&old->destination==scene->destination&&
    (old->physics!=scene->physics||old->run!=scene->run||old->event!=scene->event))return 0;
 if(s->contact_scene.world!=scene->world||s->contact_scene.peer_world!=scene->peer_world||s->contact_scene.active!=scene->active||
    s->contact_scene.to!=scene->to||s->contact_scene.destination!=scene->destination||old->contact_class!=scene->contact_class)s->contact_next_tx=0;
 s->contact_scene=*scene;s->contact_frame_at=now;return 1;
}
void rc_clear_contact_scene(rc_session *s,int64_t now){
 if(s->contact_scene.active)s->contact_next_tx=0;
 s->contact_scene.active=0;s->contact_frame_at=now;
}
static int pi_scope(const rc_session *s,const rc_peer *p,const rc_player_impulse *h){
 const rc_contact_scene *host=s->conf.world_host?&s->contact_scene:&p->contact_scene;
 const rc_contact_scene *guest=s->conf.world_host?&p->contact_scene:&s->contact_scene;
 return h->host_world==host->world&&h->guest_world==guest->world&&h->run==host->run&&h->event==host->event&&h->physics==guest->physics;
}
int rc_send_player_impulse(rc_session *s,const rc_player_impulse *h,int64_t now){
 if(!pi_valid(h)||h->source!=s->conf.extension_nonce)return 0;
 for(int i=0;i<RC_TABLE;i++){
  rc_peer *p=&s->peer[i];
  if(p->id!=h->to||h->destination!=p->extensions.nonce||!pi_roles(s,p,h->op)||
     !rc_player_contact_ready(s,p,now)||!pi_scope(s,p,h))continue;
  if(!ci_budget(s,now))return 0;
  rc_msg m;hdr(s,&m,RC_MSG_PLAYER_IMPULSE);m.u.player_impulse=*h;
  uint32_t before=s->tx;tx(s,p,&m);return before!=s->tx;
 }
 return 0;
}
int rc_take_player_impulse(rc_session *s,uint32_t *from,rc_player_impulse *h){
 if(!s->n_player_impulse)return 0;
 *from=s->player_impulse_in[0].from;*h=s->player_impulse_in[0].hit;
 for(int k=1;k<s->n_player_impulse;k++)s->player_impulse_in[k-1]=s->player_impulse_in[k];
 s->n_player_impulse--;return 1;
}
static void pi_receive(rc_session *s,rc_peer *p,const rc_player_impulse *h,int64_t now){
 if(h->to!=s->conf.self_id||h->destination!=s->conf.extension_nonce||h->source!=p->extensions.nonce||
    !rc_player_contact_ready(s,p,now)||!pi_scope(s,p,h)||!pi_roles(s,p,h->op==RC_IMPULSE_DATA?RC_IMPULSE_ACK:RC_IMPULSE_DATA))return;
 for(int k=0;k<s->n_player_impulse;k++){
  const rc_player_impulse *old=&s->player_impulse_in[k].hit;
  if(s->player_impulse_in[k].from==p->id&&old->op==h->op&&old->seq==h->seq&&old->host_world==h->host_world&&
     old->guest_world==h->guest_world&&old->source==h->source)return;
 }
 if(s->n_player_impulse==RC_PLAYER_IMPULSE_INBOX){s->player_impulse_dropped++;return;}
 s->player_impulse_in[s->n_player_impulse].from=p->id;s->player_impulse_in[s->n_player_impulse++].hit=*h;
}
static void pi_scene_receive(rc_session *s,rc_peer *p,const rc_contact_scene *scene,int64_t now){
 if(!s->conf.player_contact||!p->player_contact||!rc_extension_ready(s,p,RC_EXT_PLAYER_IMPULSE,now)||
    scene->to!=s->conf.self_id||scene->source!=p->extensions.nonce||scene->destination!=s->conf.extension_nonce||
    (!!s->conf.world_host==!!(p->extensions.features&RC_EXT_CONTACT_HOST)))return;
 if(p->contact_seen){uint32_t delta=scene->serial-p->contact_scene.serial;if(!delta||delta>=0x80000000u)return;}
 if(p->contact_fault_world==scene->world)return;
 const rc_contact_scene *old=&p->contact_scene;
 if(p->contact_seen&&old->world==scene->world&&(old->physics!=scene->physics||old->run!=scene->run||old->event!=scene->event)){
  p->contact_fault_world=scene->world;p->contact_scene.active=0;p->contact_scene.serial=scene->serial;return;
 }
 p->contact_fault_world=0;
 p->contact_scene=*scene;p->contact_seen=1;p->contact_at=now;
}
static void pi_tick(rc_session *s,rc_peer *p,int64_t now){
 if(now<s->contact_next_tx||!s->conf.player_contact||!p->player_contact||
    !rc_extension_ready(s,p,RC_EXT_PLAYER_IMPULSE,now)||s->contact_scene.to!=p->id||
    s->contact_scene.source!=s->conf.extension_nonce||s->contact_scene.destination!=p->extensions.nonce)return;
 rc_msg m;hdr(s,&m,RC_MSG_CONTACT_SCENE);m.u.contact_scene=s->contact_scene;
 m.u.contact_scene.serial=++s->contact_serial;
 if(now<s->contact_frame_at||now-s->contact_frame_at>250000||cp_held(&s->pause))m.u.contact_scene.active=0;
 tx(s,p,&m);s->contact_next_tx=now+100000;
}
