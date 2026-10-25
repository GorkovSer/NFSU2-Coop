#include "coop_stream.h"
#include <string.h>
void cs_init(cs_stream *s,uint32_t self,uint32_t peer,uint32_t run,uint32_t event,int host,int racers){
    memset(s,0,sizeof *s);s->self=self;s->peer=peer;s->run=run;s->event=event;s->host=!!host;
    if(racers>=2&&racers<=RC_RACE_CARS)s->racers=racers;
    for(int i=0;i<RC_RACE_CARS;i++)rc_interp_reset(&s->motion[i]);
}
int cs_receive(cs_stream *s,uint32_t from,const rc_race_frame *f,int64_t now){
    if(!s->racers||from!=s->peer||f->to!=s->self||f->run!=s->run||f->event!=s->event||f->racers!=s->racers)return 0;
    if(s->seq_init&&(!(uint16_t)(f->seq-s->seq)||(uint16_t)(f->seq-s->seq)>=0x8000u))return 0;
    /* Complete host roster except the guest; guest sends exactly its car.
     * Validate the whole batch before changing any interpolation buffer. */
    unsigned expected=s->host?2u:((1u<<s->racers)-1u)&~2u,mask=0;
    if(!f->n||f->n>RC_RACE_CARS)return 0;
    for(int i=0;i<f->n;i++){
        unsigned k=f->car[i].slot;
        if(k>=(unsigned)s->racers||(mask&(1u<<k))||f->car[i].position>s->racers||f->car[i].completion>7)return 0;
        mask|=1u<<k;
    }
    if(mask!=expected)return 0;
    s->seq=f->seq;s->seq_init=1;s->last_rx=now;
    for(int i=0;i<f->n;i++){
        int k=f->car[i].slot;
        rc_interp_insert(&s->motion[k],&f->car[i].state,1,1,RC_ST_EVENT,now,1);
        /* A guest's self-reported finishing position is never authoritative. */
        if(!s->host){s->position[k]=f->car[i].position;s->completion[k]=f->car[i].completion;}
    }
    return 1;
}
int cs_pose(cs_stream *s,int k,int64_t now,int64_t dt,rc_pose *p){
    return cs_pose_delay(s,k,now,dt,100000,p);
}
int cs_pose_delay(cs_stream *s,int k,int64_t now,int64_t dt,int64_t delay,rc_pose *p){
    if(k<0||k>=s->racers||!s->seq_init||cs_stale(s,now))return 0;
    if(delay < -150000||delay>500000)return 0;
    return rc_interp_eval(&s->motion[k],now,dt,delay,150000,p);
}
int cs_stale(const cs_stream *s,int64_t now){return !s->seq_init||now-s->last_rx>2000000;}
void cs_reanchor(cs_stream *s,int64_t now){
    /* Retain sequence/run authority but never interpolate through a pause. */
    for(int k=0;k<RC_RACE_CARS;k++)rc_interp_reset(&s->motion[k]);
    s->last_rx=now;
}
