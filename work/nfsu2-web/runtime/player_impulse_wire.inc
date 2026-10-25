static int pi_scene_valid(const rc_contact_scene *s){
 return s->to&&s->source&&s->destination&&s->world&&s->active<=1&&
  ((s->run==0)==(s->event==0))&&(!s->active||s->physics)&&s->contact_class<=16;
}
static int pi_valid(const rc_player_impulse *h){
 if(!h->to||!h->source||!h->destination||!h->host_world||!h->guest_world||!h->physics||
    ((h->run==0)!=(h->event==0))||(h->op!=RC_IMPULSE_DATA&&h->op!=RC_IMPULSE_ACK))return 0;
 float p2=0,l2=0;
 for(int k=0;k<3;k++){
  if(!__builtin_isfinite(h->target_p[k])||!__builtin_isfinite(h->host_p[k])||
     !__builtin_isfinite(h->dp[k])||!__builtin_isfinite(h->dl[k]))return 0;
  float bound=k==2?2000.f:20000.f;
  if(h->target_p[k]<-bound||h->target_p[k]>bound||h->host_p[k]<-bound||h->host_p[k]>bound)return 0;
  p2+=h->dp[k]*h->dp[k];l2+=h->dl[k]*h->dl[k];
 }
 return p2<=100000000.f&&l2<=10000.f;
}
static void pi_scene_put(rc_w *w,const rc_contact_scene *s){
 rc_put8(w,1);rc_put8(w,s->active);rc_put16(w,0);
 rc_put32(w,s->to);rc_put32(w,s->source);rc_put32(w,s->destination);rc_put32(w,s->serial);
 rc_put32(w,s->world);rc_put32(w,s->peer_world);rc_put32(w,s->run);rc_put32(w,s->event);
 rc_put32(w,s->physics);rc_put32(w,s->contact_class);
}
static int pi_scene_get(rc_r *r,rc_contact_scene *s){
 unsigned v=rc_get8(r);s->active=(uint8_t)rc_get8(r);unsigned reserved=rc_get16(r);
 s->to=rc_get32(r);s->source=rc_get32(r);s->destination=rc_get32(r);s->serial=rc_get32(r);
 s->world=rc_get32(r);s->peer_world=rc_get32(r);s->run=rc_get32(r);s->event=rc_get32(r);
 s->physics=rc_get32(r);s->contact_class=rc_get32(r);
 return v==1&&!reserved&&!r->err&&pi_scene_valid(s);
}
static void pi_put(rc_w *w,const rc_player_impulse *h){
 rc_put8(w,1);rc_put8(w,h->op);rc_put16(w,h->seq);
 rc_put32(w,h->to);rc_put32(w,h->source);rc_put32(w,h->destination);
 rc_put32(w,h->host_world);rc_put32(w,h->guest_world);rc_put32(w,h->run);rc_put32(w,h->event);
 rc_put32(w,h->physics);rc_put32(w,h->t_us);
 for(int k=0;k<3;k++)rc_putf(w,h->target_p[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->host_p[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->dp[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->dl[k]);
}
static int pi_get(rc_r *r,rc_player_impulse *h){
 unsigned v=rc_get8(r);h->op=(uint8_t)rc_get8(r);h->seq=(uint16_t)rc_get16(r);
 h->to=rc_get32(r);h->source=rc_get32(r);h->destination=rc_get32(r);
 h->host_world=rc_get32(r);h->guest_world=rc_get32(r);h->run=rc_get32(r);h->event=rc_get32(r);
 h->physics=rc_get32(r);h->t_us=rc_get32(r);
 for(int k=0;k<3;k++)h->target_p[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->host_p[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->dp[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->dl[k]=rc_getf(r);
 return v==1&&!r->err&&pi_valid(h);
}
