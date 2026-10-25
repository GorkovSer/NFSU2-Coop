static int ci_valid(const rc_impulse *h){
 if(!h->to||!h->source||!h->destination||!h->world||!h->id||
    (h->op!=RC_IMPULSE_DATA&&h->op!=RC_IMPULSE_ACK))return 0;
 float j2=0,r2=0;
 for(int k=0;k<3;k++){
  if(!__builtin_isfinite(h->p[k])||!__builtin_isfinite(h->player[k])||
     !__builtin_isfinite(h->impulse[k])||!__builtin_isfinite(h->arm[k]))return 0;
  float limit=k==2?2000.f:20000.f;
  if(h->p[k]<-limit||h->p[k]>limit||h->player[k]<-limit||h->player[k]>limit)return 0;
  j2+=h->impulse[k]*h->impulse[k];r2+=h->arm[k]*h->arm[k];
 }
 return j2<=1.e12f&&r2<=144.f;
}
static void ci_put(rc_w *w,const rc_impulse *h){
 rc_put8(w,1);rc_put8(w,h->op);rc_put16(w,0);
 rc_put32(w,h->to);rc_put32(w,h->source);rc_put32(w,h->destination);
 rc_put32(w,h->world);rc_put32(w,h->context);rc_put32(w,h->id);rc_put32(w,h->t_us);
 rc_put16(w,h->seq);rc_put16(w,0);
 for(int k=0;k<3;k++)rc_putf(w,h->p[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->player[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->impulse[k]);
 for(int k=0;k<3;k++)rc_putf(w,h->arm[k]);
}
static int ci_get(rc_r *r,rc_impulse *h){
 unsigned version=rc_get8(r);h->op=(uint8_t)rc_get8(r);unsigned reserved=rc_get16(r);
 h->to=rc_get32(r);h->source=rc_get32(r);h->destination=rc_get32(r);
 h->world=rc_get32(r);h->context=rc_get32(r);h->id=rc_get32(r);h->t_us=rc_get32(r);
 h->seq=(uint16_t)rc_get16(r);reserved|=rc_get16(r);
 for(int k=0;k<3;k++)h->p[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->player[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->impulse[k]=rc_getf(r);
 for(int k=0;k<3;k++)h->arm[k]=rc_getf(r);
 return version==1&&!reserved&&!r->err&&ci_valid(h);
}
