#include "traffic_sync.h"
#include <string.h>
void ts_init(ts_client *s,uint32_t self,uint32_t host,int64_t now){
    memset(s,0,sizeof *s);s->self=self;s->host=host;s->scene_since=now;
}
void ts_scene(ts_client *s,uint32_t context,int64_t now){
    memset(s->car,0,sizeof s->car);s->active=0;s->context=context;s->scene_since=now;
}
int ts_receive(ts_client *s,uint32_t from,const rc_traffic_frame *f,int64_t now,int32_t offset){
    if(!s->self||!s->host||from!=s->host||f->to!=s->self||!f->world||f->context!=s->context||f->n>RC_TRAFFIC_CARS)return 0;
    uint32_t stamp=f->t_us+(uint32_t)offset;
    int32_t age=(int32_t)((uint32_t)now-stamp);
    if(age>2000000||age < -100000||(int32_t)(stamp-(uint32_t)s->scene_since)<0)return 0;
    uint32_t delta=f->world-s->world;
    if(s->seen&&(delta>=0x80000000u||(!delta&&(!(uint16_t)(f->seq-s->seq)||(uint16_t)(f->seq-s->seq)>=0x8000u))))return 0;
    for(int i=0;i<f->n;i++){
        if(!f->car[i].id||!f->car[i].type)return 0;
        for(int j=0;j<i;j++)if(f->car[j].id==f->car[i].id)return 0;
        /* A reused ID cannot silently turn a bus into a different vehicle. */
        if(s->seen&&!delta)for(int j=0;j<RC_TRAFFIC_CARS;j++)if(s->car[j].id==f->car[i].id&&s->car[j].type!=f->car[i].type)return 0;
    }
    if(!s->seen||delta)memset(s->car,0,sizeof s->car);
    for(int i=0;i<RC_TRAFFIC_CARS;i++)if(s->car[i].id){
        int exists=0;for(int j=0;j<f->n;j++)if(s->car[i].id==f->car[j].id)exists=1;
        if(!exists)memset(&s->car[i],0,sizeof s->car[i]);
    }
    for(int j=0;j<f->n;j++){
        int slot=-1;for(int i=0;i<RC_TRAFFIC_CARS;i++)if(s->car[i].id==f->car[j].id){slot=i;break;}
        if(slot<0)for(int i=0;i<RC_TRAFFIC_CARS;i++)if(!s->car[i].id){slot=i;break;}
        if(slot<0)return 0; /* full packet validation makes this unreachable */
        s->car[slot].id=f->car[j].id;s->car[slot].type=f->car[j].type;
        rc_interp_insert(&s->car[slot].motion,&f->car[j].state,1,1,RC_ST_ROAM,now,1);
    }
    s->seen=1;s->world=f->world;s->seq=f->seq;s->last_rx=now;s->active=1;return 1;
}
int ts_expire(ts_client *s,int64_t now){
    if(s->active&&now-s->last_rx>2000000){memset(s->car,0,sizeof s->car);s->active=0;return 1;}return 0;
}
int ts_pose(ts_client *s,uint32_t id,int64_t now,int64_t dt,rc_pose *p){
    ts_expire(s,now);if(!s->active||!id)return 0;
    for(int i=0;i<RC_TRAFFIC_CARS;i++)if(s->car[i].id==id)return rc_interp_eval(&s->car[i].motion,now,dt,100000,150000,p);
    return 0;
}
