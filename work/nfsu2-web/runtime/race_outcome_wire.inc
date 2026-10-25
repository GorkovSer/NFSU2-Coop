static void rm_put(rc_w *w,const rm_packet *p){
 rc_put8(w,2);rc_put8(w,p->op);rc_put8(w,p->mode);rc_put8(w,p->round);
 rc_put8(w,p->rounds);rc_put8(w,p->racers);rc_put8(w,p->final);rc_put8(w,0);
 rc_put32(w,p->to);rc_put32(w,p->source);rc_put32(w,p->destination);rc_put32(w,p->run);rc_put32(w,p->event);rc_put32(w,p->layout);rc_put32(w,p->serial);
 for(int k=0;k<RM_CARS;k++){rc_put8(w,p->car[k].status);rc_put8(w,0);rc_put16(w,0);rc_put32(w,p->car[k].ticks);rc_put32(w,p->car[k].score);}
 for(int k=0;k<RM_CARS;k++){rc_put32(w,p->car[k].wall);rc_put32(w,p->elapsed[k]);}
}
static int rm_get(rc_r *r,rm_packet *p){
 unsigned version=rc_get8(r);p->op=(uint8_t)rc_get8(r);p->mode=(uint8_t)rc_get8(r);p->round=(uint8_t)rc_get8(r);
 p->rounds=(uint8_t)rc_get8(r);p->racers=(uint8_t)rc_get8(r);p->final=(uint8_t)rc_get8(r);unsigned reserved=rc_get8(r);
 p->to=rc_get32(r);p->source=rc_get32(r);p->destination=rc_get32(r);p->run=rc_get32(r);p->event=rc_get32(r);p->layout=rc_get32(r);p->serial=rc_get32(r);
 for(int k=0;k<RM_CARS;k++){p->car[k].status=(uint8_t)rc_get8(r);reserved|=rc_get8(r);reserved|=rc_get16(r);p->car[k].ticks=rc_get32(r);p->car[k].score=rc_get32(r);}
 memset(p->elapsed,0,sizeof p->elapsed);
 for(int k=0;k<RM_CARS;k++){p->car[k].wall=rc_get32(r);p->elapsed[k]=rc_get32(r);}
 return version==2&&!reserved&&!r->err&&rm_packet_valid(p);
}
