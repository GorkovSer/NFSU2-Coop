/* Included by roam_core.c; public portable reducer shared by native adapter. */
int rm_value_valid(const rm_value *v){
 return v->ticks<=RM_MAX_TICKS&&v->score<=RM_MAX_SCORE&&v->wall<=RM_MAX_TICKS&&
 (v->status==RM_LIVE||v->status==RM_FINISH||v->status==RM_TOTALLED||v->status==RM_BLOWN||v->status==RM_TIMEOUT);
}
static int rm_timing_valid(const rm_value *car,const uint32_t *elapsed,int racers,int mode){
 for(int k=0;k<racers;k++){
  if(!car[k].status)return 0;
  if(mode==RM_DRIFT&&k>=2&&!car[k].wall){if(elapsed[k])return 0;continue;}
  if(!car[k].wall||!elapsed[k]||elapsed[k]>car[k].wall)return 0;
  for(int j=0;j<k;j++){
   if(!car[j].wall)continue;
   uint32_t a=car[j].wall,b=car[k].wall,x=elapsed[j],y=elapsed[k];
   if(a>b){uint32_t t=a;a=b;b=t;t=x;x=y;y=t;}
   if(y<x||y-x>b-a)return 0; /* One nondecreasing pause-excluding clock. */
  }
 }
 for(int k=racers;k<RM_CARS;k++)if(elapsed[k])return 0;
 return 1;
}
int rm_packet_valid(const rm_packet *p){
 if(!p->to||!p->source||!p->destination||!p->run||!p->event||!p->layout||p->op<RM_TERMS||p->op>RM_BOARD_ACK||
 p->mode<RM_URL||p->mode>RM_RANK||p->racers<2||p->racers>RM_CARS||!p->rounds||p->rounds>RM_ROUNDS||p->round>=p->rounds||p->final>1)return 0;
 if(p->mode!=RM_URL&&(p->round||p->rounds!=1))return 0;
 for(int k=0;k<RM_CARS;k++){
  if(!rm_value_valid(&p->car[k]))return 0;
  if((k>=p->racers||(p->op!=RM_OWNER&&p->op!=RM_BOARD)||(p->op==RM_OWNER&&k!=1))&&
     (p->car[k].ticks||p->car[k].score||p->car[k].status||p->car[k].wall))return 0;
  if(p->mode!=RM_DRIFT&&p->car[k].score)return 0;
  if(p->car[k].status&&!p->car[k].wall&&(p->mode!=RM_DRIFT||k<2))return 0;
  if((p->op!=RM_BOARD||!p->final)&&p->elapsed[k])return 0;
 }
 if(p->op==RM_BOARD&&p->final&&!rm_timing_valid(p->car,p->elapsed,p->racers,p->mode))return 0;
 return p->op==RM_BOARD?1:!p->final;
}
int rm_init(rm_outcome *s,uint32_t run,uint32_t event,uint32_t layout,int mode,int round,int rounds,int racers,int host){
 memset(s,0,sizeof *s);
 if(mode<RM_URL||mode>RM_RANK||round!=0||rounds<1||rounds>RM_ROUNDS||racers<2||racers>RM_CARS){s->fault=1;return 0;}
 rm_packet p;memset(&p,0,sizeof p);p.to=p.source=p.destination=1;p.run=run;p.event=event;p.layout=layout;
 p.op=RM_TERMS;p.mode=(uint8_t)mode;p.round=(uint8_t)round;p.rounds=(uint8_t)rounds;p.racers=(uint8_t)racers;
 if(!rm_packet_valid(&p)){s->fault=1;return 0;}
 s->run=run;s->event=event;s->layout=layout;s->mode=mode;s->round=round;s->rounds=rounds;s->racers=racers;s->host=!!host;s->initialized=1;return 1;
}
static int rm_scope(const rm_outcome *s,const rm_packet *p){
 return s->initialized&&!s->fault&&p->run==s->run&&p->event==s->event&&p->layout==s->layout&&p->mode==s->mode&&
 p->round==s->round&&p->rounds==s->rounds&&p->racers==s->racers;
}
static int rm_changed(const rm_value *a,const rm_value *b){return a->ticks!=b->ticks||a->score!=b->score||a->status!=b->status||a->wall!=b->wall;}
static int rm_update(rm_outcome *s,int k,const rm_value *v){
 rm_value *old=&s->car[k];
 if(!rm_value_valid(v)||k>=s->racers||(s->mode!=RM_DRIFT&&v->score))return 0;
 if(v->status&&!v->wall&&(s->mode!=RM_DRIFT||k<2))return 0;
 if(old->status){if(rm_changed(old,v)){s->fault=1;return 0;}return 1;}
 if(v->ticks<old->ticks||v->score<old->score||v->wall<old->wall)return 0;
 if(rm_changed(old,v)){*old=*v;s->next_tx=0;}return 1;
}
int rm_set(rm_outcome *s,int k,const rm_value *v){
 if(!s->initialized||s->fault||s->final||k<0||k>=s->racers||(s->host?k==1:k!=1))return 0;
 return rm_update(s,k,v);
}
int rm_ready(const rm_outcome *s){return s->initialized&&!s->fault&&(s->host?s->peer_agreed:s->agreed);}
static int rm_before(const rm_outcome *s,int a,int b){
 const rm_value *x=&s->car[a],*y=&s->car[b];int xf=x->status==RM_FINISH,yf=y->status==RM_FINISH;
 if(xf!=yf)return xf;
 if(xf&&s->mode==RM_DRIFT&&x->score!=y->score)return x->score>y->score;
 if(x->wall!=y->wall)return xf?x->wall<y->wall:x->wall>y->wall;
 return a<b; /* frozen canonical roster breaks exact ties identically */
}
static void rm_rank(rm_outcome *s){
 for(int k=0;k<s->racers;k++){int rank=1;for(int j=0;j<s->racers;j++)if(j!=k&&rm_before(s,j,k))rank++;s->rank[k]=(uint8_t)rank;}
}
/* Both humans terminal: preserve genuine AI finishes and close only still
 * live host entries. One common policy for all physical race modes. */
int rm_close_ai(rm_outcome *s,uint32_t ticks,uint32_t wall){
 if(!s->host||!rm_ready(s)||s->final||s->racers<2||!s->car[0].status||!s->car[1].status)return 0;
 if(!wall||wall>RM_MAX_TICKS||ticks>RM_MAX_TICKS)return -1;
 for(int k=2;k<s->racers;k++)if(!s->car[k].status){
  rm_value v={ticks,s->car[k].score,RM_TIMEOUT,wall};
  if(!rm_set(s,k,&v))return -1;
 }
 return 1;
}
int rm_resolve(rm_outcome *s){
 if(!s->host||!rm_ready(s))return 0;
 if(s->final)return 1;
 for(int k=0;k<s->racers;k++)if(!s->car[k].status)return 0;
 if(!rm_timing_valid(s->car,s->elapsed,s->racers,s->mode))return 0;
 rm_rank(s);s->final=1;s->next_tx=0;return 1;
}
int rm_timing(rm_outcome *s,const uint32_t elapsed[RM_CARS]){
 if(!s->initialized||!s->host||s->fault||s->final||!rm_timing_valid(s->car,elapsed,s->racers,s->mode))return 0;
 memcpy(s->elapsed,elapsed,sizeof s->elapsed);return 1;
}
int rm_receive(rm_outcome *s,const rm_packet *p){
 if(!rm_packet_valid(p)||!s->initialized||s->fault||p->run!=s->run||p->event!=s->event)return 0;
 /* Other rounds may still be retrying. They never fault the new round. */
 if(p->round!=s->round)return 0;
 if(!rm_scope(s,p)){s->fault=1;return 0;}
 if(s->seen){uint32_t d=p->serial-s->received_serial;if(!d||d>=0x80000000u)return 0;}
 if(s->host){
  if(p->op!=RM_TERMS_ACK&&p->op!=RM_OWNER&&p->op!=RM_BOARD_ACK)return 0;
  if(p->op==RM_OWNER&&(!s->peer_agreed||s->final))return 0;
  if(p->op==RM_BOARD_ACK&&!s->final)return 0;
 }else if(p->op!=RM_TERMS&&p->op!=RM_BOARD)return 0;
 if(!s->host&&p->op==RM_BOARD){
  if(!s->agreed)return 0;
  /* A final board must contain all terminal entries and preserve the owner's frozen result. */
  if(p->final){for(int k=0;k<s->racers;k++)if(!p->car[k].status)return 0;}
  if(p->final&&(!s->car[1].status||rm_changed(&s->car[1],&p->car[1]))){s->fault=1;return 0;}
  if(s->final){for(int k=0;k<s->racers;k++)if(rm_changed(&s->car[k],&p->car[k])||s->elapsed[k]!=p->elapsed[k]){s->fault=1;return 0;}}
 }
 s->seen=1;s->received_serial=p->serial;
 switch(p->op){
 case RM_TERMS:s->agreed=1;s->next_tx=0;break;
 case RM_TERMS_ACK:s->peer_agreed=1;s->next_tx=0;break;
 case RM_OWNER:if(!rm_update(s,1,&p->car[1]))return 0;break;
 case RM_BOARD:
  s->peer_agreed=1;
  for(int k=0;k<s->racers;k++)if(k!=1||p->final)s->car[k]=p->car[k];
  if(p->final){memcpy(s->elapsed,p->elapsed,sizeof s->elapsed);s->final=1;rm_rank(s);s->next_tx=0;}break;
 case RM_BOARD_ACK:s->board_ack=1;break;
 }
 return 1;
}
int rm_next(rm_outcome *s,int64_t now,rm_packet *p){
 if(!s->initialized||s->fault||now<s->next_tx)return 0;
 memset(p,0,sizeof *p);p->run=s->run;p->event=s->event;p->layout=s->layout;p->mode=(uint8_t)s->mode;
 p->round=(uint8_t)s->round;p->rounds=(uint8_t)s->rounds;p->racers=(uint8_t)s->racers;p->serial=++s->serial;
 if(s->host){
  if(s->final&&s->board_ack)return 0;
  if(!s->peer_agreed)p->op=RM_TERMS;
  else {p->op=RM_BOARD;p->final=(uint8_t)s->final;memcpy(p->car,s->car,sizeof p->car);if(s->final)memcpy(p->elapsed,s->elapsed,sizeof p->elapsed);}
 }else{
  if(!s->agreed)return 0;
  if(s->final)p->op=RM_BOARD_ACK;
  else if(!s->peer_agreed)p->op=RM_TERMS_ACK;
  else {p->op=RM_OWNER;p->car[1]=s->car[1];}
 }
 s->next_tx=now+100000;return 1;
}
int rm_team_winner(const rm_outcome *s){
 if(!s->final||s->fault)return 0;
 if(s->mode==RM_URL&&s->round+1==s->rounds&&s->rounds_committed==s->rounds){
  int best=0;for(int k=1;k<s->racers;k++)if(s->points[k]>s->points[best]||
    (s->points[k]==s->points[best]&&k<2&&best>=2))best=k;
  return s->points[best]&&best<2?best+1:0;
 }
 for(int k=0;k<2;k++)if(s->rank[k]==1&&s->car[k].status==RM_FINISH)return k+1;
 return 0;
}
int rm_commit_round(rm_outcome *s){
 static const uint8_t points[6]={8,6,4,3,2,1};
 if(s->mode!=RM_URL||!s->final||s->fault||s->rounds_committed!=s->round)return 0;
 for(int k=0;k<s->racers;k++){s->points[k]+=points[s->rank[k]-1];s->last_rank[k]=s->rank[k];}
 s->rounds_committed++;return 1;
}
int rm_advance(rm_outcome *s,uint32_t layout){
 if(s->mode!=RM_URL||s->fault||!s->final||s->rounds_committed!=s->round+1||s->round+1>=s->rounds||!layout||
    (s->host&&!s->board_ack))return 0;
 s->round++;s->layout=layout;s->agreed=s->peer_agreed=s->seen=s->final=s->board_ack=0;s->serial=s->received_serial=0;s->next_tx=0;
 memset(s->elapsed,0,sizeof s->elapsed);memset(s->car,0,sizeof s->car);memset(s->rank,0,sizeof s->rank);return 1;
}
