static int rm_roles(const rc_session *s,const rc_peer *p,int op){
 int host=!!(s->conf.extensions&RC_EXT_CONTACT_HOST),remote=!!(p->extensions.features&RC_EXT_CONTACT_HOST);
 int host_op=op==RM_TERMS||op==RM_BOARD;
 return s->conf.coop&&p->coop&&host!=remote&&host==host_op;
}
int rc_send_outcome(rc_session *s,const rm_packet *p,int64_t now){
 if(!rm_packet_valid(p)||p->source!=s->conf.extension_nonce)return 0;
 for(int k=0;k<RC_TABLE;k++){
  rc_peer *peer=&s->peer[k];
  if(peer->id!=p->to||peer->extensions.nonce!=p->destination||!rc_extension_ready(s,peer,RC_EXT_RACE_MODES,now)||!rm_roles(s,peer,p->op))continue;
  if(!rc_extension_ready(s,peer,RC_EXT_RANK_CLOCK,now))continue;
  if(p->mode==RM_RANK&&!rc_extension_ready(s,peer,RC_EXT_RANK_TABLE,now))continue;
  if(!ci_budget(s,now))return 0;
  rc_msg m;hdr(s,&m,RC_MSG_OUTCOME);m.u.outcome=*p;uint32_t before=s->tx;tx(s,peer,&m);return before!=s->tx;
 }
 return 0;
}
int rc_take_outcome(rc_session *s,uint32_t *from,rm_packet *p){
 if(!s->n_outcome)return 0;
 *from=s->outcome_in[0].from;*p=s->outcome_in[0].message;
 for(int k=1;k<s->n_outcome;k++)s->outcome_in[k-1]=s->outcome_in[k];s->n_outcome--;return 1;
}
static void rm_packet_receive(rc_session *s,rc_peer *p,const rm_packet *m,int64_t now){
 if(!rc_extension_ready(s,p,RC_EXT_RACE_MODES,now)||m->to!=s->conf.self_id||m->source!=p->extensions.nonce||m->destination!=s->conf.extension_nonce)return;
 if(!rc_extension_ready(s,p,RC_EXT_RANK_CLOCK,now))return;
 if(m->mode==RM_RANK&&!rc_extension_ready(s,p,RC_EXT_RANK_TABLE,now))return;
 int host=!!(s->conf.extensions&RC_EXT_CONTACT_HOST),peer_host=!!(p->extensions.features&RC_EXT_CONTACT_HOST);
 if(!s->conf.coop||!p->coop||host==peer_host||peer_host!=(m->op==RM_TERMS||m->op==RM_BOARD))return;
 for(int k=0;k<s->n_outcome;k++)if(s->outcome_in[k].from==p->id&&s->outcome_in[k].message.serial==m->serial&&s->outcome_in[k].message.source==m->source&&s->outcome_in[k].message.run==m->run&&s->outcome_in[k].message.round==m->round)return;
 if(s->n_outcome==RM_INBOX)return;
 s->outcome_in[s->n_outcome].from=p->id;s->outcome_in[s->n_outcome++].message=*m;
}
