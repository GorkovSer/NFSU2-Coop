int rc_send_event_catalog(rc_session *s,const rc_event_catalog *v,int64_t now){
 if(!rc_event_catalog_valid(v)||v->source!=s->conf.extension_nonce||!s->conf.coop)return 0;
 for(int k=0;k<RC_TABLE;k++){
  rc_peer *p=&s->peer[k];
  if(p->id!=v->to||!p->coop||p->extensions.nonce!=v->destination||!rc_extension_ready(s,p,RC_EXT_CAREER_MAP,now))continue;
  rc_msg m;hdr(s,&m,RC_MSG_EVENT_CATALOG);m.u.event_catalog=*v;
  uint32_t before=s->tx;tx(s,p,&m);return s->tx!=before;
 }
 return 0;
}
static void rc_event_catalog_receive(rc_session *s,rc_peer *p,const rc_event_catalog *v,int64_t now){
 if(!s->conf.coop||!p->coop||!rc_extension_ready(s,p,RC_EXT_CAREER_MAP,now)||
    v->to!=s->conf.self_id||v->source!=p->extensions.nonce||v->destination!=s->conf.extension_nonce)return;
 if(p->event_catalog_seen&&p->event_catalog.source==v->source&&
    (int32_t)(v->serial-p->event_catalog.serial)<=0)return;
 p->event_catalog=*v;p->event_catalog_seen=1;p->event_catalog_at=now;
}
