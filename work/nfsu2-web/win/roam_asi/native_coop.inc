/* Included by roam_game.c only in the exact-EXE experimental build.
 * All state below belongs to the game thread; session access is locked. */
#include "coop_race.h"
#include "coop_stream.h"
#include "native_coop.h"
static struct {
    int enabled,checked,host,connected,clock_valid,bound,loaded_noted,result_applied,phase_logged;
    uint32_t peer,next_run,pending_event,world,race,car[6],brain[6],record[6],old_phys[6];
    int32_t clock_offset;
    int64_t last_frame,next_send;
    uint16_t seq;
    cc_race control;
    cs_stream stream;
    Slot slot[6];
} NC;
static int nc_running(void){return NC.control.phase>=CC_LOADING&&NC.control.phase<=CC_DONE;}
int nc_enabled(void){
    if(!NC.checked){
        NC.checked=1;NC.host=env_int("NFSU2_ROAM_COOP_HOST",0,0,1);
        if(env_int("NFSU2_ROAM_COOP",0,0,1)){
            NC.enabled=nc_hooks_install();NC.next_run=roam_plat_random();
            rlog("[coop] experimental Circuit support: %s, role %s; F7 accept, F8 cancel\n",NC.enabled?"hooks verified":"OFF: hook mismatch",NC.host?"host":"guest");
        }
    }
    return NC.enabled;
}
static uint32_t nc_available(uint32_t hash){
    for(unsigned i=0;i<64;i++){
        uint32_t e=rd32(0x85ad40u+0x71c8u+4*i);
        /* First engine integration supports conventional circuit only. */
        if(ok(e,0x87)&&MR32(e+8)==hash&&MR8(e+0xf)==0)return e;
    }
    return 0;
}
static int nc_same_world(void){
    return NC.bound&&rd32(G_FLOW)==6&&rd32(G_WORLD)==NC.world&&rd32(G_RACE)==NC.race&&
           rd32(G_CAREER_RACE)&&rd32(rd32(G_CAREER_RACE)+8)==NC.control.event;
}
int nc_remote_car(uint32_t car){
    if(!NC.enabled||!nc_running()||!nc_same_world())return 0;
    for(int i=0;i<NC.stream.racers;i++)if(NC.car[i]==car&&i!=(NC.host?0:1))return NC.host?i==1:1;
    return 0;
}
int nc_remote_brain(uint32_t brain){
    if(!NC.enabled||!nc_running()||!nc_same_world())return 0;
    for(int i=0;i<NC.stream.racers;i++)if(NC.brain[i]==brain&&nc_remote_car(NC.car[i]))return 1;
    return 0;
}
int nc_start(void){
    if(!NC.enabled||!NC.connected||!roaming())return 0;
    if(!NC.host){rlog("[coop] guest: host selects the event; F7 accepts its invitation\n");return 1;}
    if(NC.control.phase>=CC_INVITED&&NC.control.phase<=CC_RUNNING)return 1;
    uint32_t e=rd32(0x863448u),hash=ok(e,0x87)?MR32(e+8):0;
    if(!rd32(0x863444u)||!nc_available(hash)){
        rlog("[coop] event not supported by the experimental Circuit adapter; ordinary start\n");return 0;
    }
    NC.pending_event=hash;return 1;
}
int nc_state(uint32_t race,uint32_t state){
    if(!NC.enabled||state!=4)return 0;
    return race==rd32(G_RACE)&&NC.control.phase>=CC_LOADING&&NC.control.phase<=CC_COUNTDOWN;
}
int nc_finish_block(uint32_t race){
    return NC.enabled&&!NC.host&&!NC.result_applied&&race==NC.race&&nc_same_world()&&nc_running();
}
void nc_finished(uint32_t race,uint32_t index){
    if(!NC.enabled||!NC.host||NC.control.phase!=CC_RUNNING||race!=NC.race||!nc_same_world()||index>=6)return;
    uint32_t rec=rd32(race+0xbd0u+4*index);
    if(!ok(rec,0x20)||MR16(rec+0x10)!=1)return;
    unsigned pos=MR8(rec+0xc);
    for(int k=0;k<NC.stream.racers;k++)if(NC.record[k]==rec){
        if(k<2)cc_confirm_finish(&NC.control,k?NC.peer:g_s.conf.self_id,(uint8_t)pos,roam_plat_now_us());
        else if(pos==1)cc_bot_won(&NC.control,roam_plat_now_us());
        return;
    }
}
static void nc_unbind(CPU *c){
    if(nc_same_world())for(int k=0;k<NC.stream.racers;k++)if(NC.car[k]&&k!=(NC.host?0:1)&&ok(NC.car[k],0x9e0)){
        body_set(c,NC.car[k],1);gcall1(c,FN_CAR_SETPHYS,NC.car[k],NC.old_phys[k]);
    }
    NC.bound=0;NC.loaded_noted=0;NC.result_applied=0;NC.next_send=0;NC.seq=0;
    memset(NC.car,0,sizeof NC.car);memset(NC.brain,0,sizeof NC.brain);memset(NC.record,0,sizeof NC.record);memset(NC.slot,0,sizeof NC.slot);
}
static int nc_bind(void){
    uint32_t race=rd32(G_RACE),me=player_car();
    if(rd32(G_FLOW)!=6||!ok(race,0xc44)||rd32(G_CAREER_RACE)==0||rd32(rd32(G_CAREER_RACE)+8)!=NC.control.event||MR32(race)!=3)return 0;
    unsigned n=MR32(race+0x24);if(n<2||n>6)return -1;
    uint32_t mine=0,other[5];unsigned count=0;
    for(unsigned i=0;i<n;i++){
        uint32_t rec=race+0x30u+0x1f0u*i,car=rd32(rec);
        if(!ok(car,0x9e0)||!ok(rd32(car+0x2c),0x7b3))return -1;
        if(car==me){if(mine)return -1;mine=rec;}
        else {
            if(MR32(car+0x24)!=3||rd32(rd32(car+0x2c))!=0x785370u)return -1;
            other[count++]=rec;
        }
    }
    if(!mine||count!=n-1)return -1;
    /* Sort by the stable game racer record, never by current race position. */
    for(unsigned i=0;i<count;i++)for(unsigned j=i+1;j<count;j++)if(rd32(rd32(other[j])+0x14)<rd32(rd32(other[i])+0x14)){
        uint32_t tmp=other[i];other[i]=other[j];other[j]=tmp;
    }
    NC.record[NC.host?0:1]=mine;NC.record[NC.host?1:0]=other[0];
    for(unsigned i=1;i<count;i++)NC.record[i+1]=other[i];
    NC.race=race;NC.world=world();
    for(unsigned k=0;k<n;k++){
        uint32_t car=NC.car[k]=rd32(NC.record[k]);NC.brain[k]=rd32(car+0x2c);NC.old_phys[k]=rd32(car+0x28);NC.slot[k].car=car;
        float delta[3],f[3],l[3],u[3];rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);
        for(int axis=0;axis<3;axis++)delta[axis]=MRF32(car+0x60+4*axis)-MRF32(car+0x50+4*axis);
        for(int axis=0;axis<3;axis++){NC.slot[k].com[0]+=delta[axis]*f[axis];NC.slot[k].com[1]+=delta[axis]*l[axis];NC.slot[k].com[2]+=delta[axis]*u[axis];}
    }
    cs_init(&NC.stream,g_s.conf.self_id,NC.peer,NC.control.run,NC.control.event,NC.host,(int)n);
    NC.bound=1;rlog("[coop] run %08x bound %u racers; original AI brains retained\n",NC.control.run,n);return 1;
}
static int nc_read_car(uint32_t car,rc_state *s,int64_t now){
    if(!ok(car,0x9e0))return 0;
    memset(s,0,sizeof *s);s->t_us=(uint32_t)now;
    float f[3],l[3],u[3];rd3(car+0x60,s->p);rd3(car+0x70,s->v);rd3(car+0x110,s->w);
    rd3(car+0x90,f);rd3(car+0xa0,l);rd3(car+0xb0,u);rc_m2q(f,l,u,s->q);
    s->rpm=MRF32(car+0x440);s->speed=MRF32(car+0x42c);s->gear=(int8_t)MR32(car+0x434);
    uint32_t ctl=rd32(car+0x30);if(ok(ctl,0x220)){
        s->throttle=MRF32(ctl+0x20c);s->brake=MRF32(ctl+0x214);s->steer=(float)(int16_t)MR16(ctl+0x1e0)/32767.f;
    }
    for(int i=0;i<3;i++)if(!__builtin_isfinite(s->p[i])||!__builtin_isfinite(s->v[i])||!__builtin_isfinite(s->w[i]))return 0;
    return __builtin_isfinite(s->rpm)&&__builtin_isfinite(s->speed)&&__builtin_isfinite(s->steer)&&__builtin_isfinite(s->throttle)&&__builtin_isfinite(s->brake);
}
static void nc_apply_result(CPU *c){
    if(NC.result_applied||!nc_same_world())return;
    NC.result_applied=1;
    uint32_t rec=NC.record[NC.host?0:1];
    if(!ok(rec,0x20)||MR16(rec+0x10))return;
    for(unsigned i=0;i<(unsigned)NC.stream.racers;i++)if(rd32(NC.race+0xbd0+4*i)==rec){
        /* Cooperative victory: both local human profiles take first place,
         * through the original finish routine, after the host verified a human
         * crossing. No direct writes to career rewards or completion flags. */
        uint32_t args[5]={NC.race,i,NC.control.winner?1u:0u,0,NC.control.winner?1u:3u};
        rh_call(c,(uint32_t)(uintptr_t)nc_original_finish,0,0,5,args);break;
    }
}
static void nc_frame(CPU *c,int64_t now,int in_roam){
    if(!nc_enabled())return;
    int64_t dt=NC.last_frame?now-NC.last_frame:0;NC.last_frame=now;if(dt<0||dt>100000)dt=0;
    int active=NC.control.phase>=CC_INVITED&&NC.control.phase<=CC_RUNNING;
    uint32_t chosen=0;NC.connected=0;NC.clock_valid=0;
    roam_plat_lock();
    for(int i=0;i<RC_TABLE;i++){
        rc_peer *p=&g_s.peer[i];if(p->state!=RC_PS_EST||p->lost||!p->coop)continue;
        if(active&&p->id!=NC.peer)continue;
        if(chosen){chosen=0;break;}chosen=p->id;
        NC.clock_valid=p->clock_valid&&now-p->clock_sample_at<10000000&&p->clock_rtt_us<=200000;NC.clock_offset=p->clock_offset_us;
    }
    roam_plat_unlock();
    if(chosen){
        NC.connected=1;
        if(NC.peer!=chosen){NC.peer=chosen;cc_init(&NC.control,g_s.conf.self_id,chosen,NC.host);}
    }
    if(NC.pending_event){
        uint32_t event=NC.pending_event;NC.pending_event=0;
        if(NC.connected&&in_roam){nc_unbind(c);if(++NC.next_run==0)++NC.next_run;cc_offer(&NC.control,NC.next_run,event,now);}
    }
    uint32_t from;rc_coop_msg m;
    for(int k=0;k<RC_COOP_INBOX;k++){
        roam_plat_lock();int got=rc_take_coop(&g_s,&from,&m);roam_plat_unlock();if(!got)break;
        if(m.op==RC_COOP_GO&&!NC.clock_valid)continue;
        cc_receive(&NC.control,from,&m,now,NC.clock_offset);
    }
    int ready=nc_ready_key(),cancel=nc_cancel_key();
    if(cancel)cc_cancel(&NC.control,CC_WITHDRAWN,now);
    if(ready&&!NC.host&&NC.control.phase==CC_INVITED&&in_roam&&NC.clock_valid)
        cc_accept(&NC.control,nc_available(NC.control.event)?NC.control.event:0,now);
    cc_step(&NC.control,now,NC.connected);
    uint32_t actions=cc_take_actions(&NC.control);
    if(actions&CC_INVITATION){nc_unbind(c);rlog("[coop] invitation to %08x: F7 accepts, F8 declines\n",NC.control.event);}
    if(actions&CC_LOAD_GAME){
        uint32_t event=nc_available(NC.control.event);
        if(!event||!in_roam)cc_cancel(&NC.control,CC_ENGINE_ERROR,now);
        else {MR32(0x863444)=1;MR32(0x863448)=event;rh_call(c,(uint32_t)(uintptr_t)nc_original_start,0,0,0,0);}
    }
    if(NC.control.phase==CC_LOADING&&!NC.bound){int bound=nc_bind();if(bound<0)cc_cancel(&NC.control,CC_UNSUPPORTED,now);}
    if(NC.bound&&nc_running()){
        if(!nc_same_world()){if(NC.control.phase!=CC_DONE)cc_cancel(&NC.control,CC_ENGINE_ERROR,now);NC.bound=0;}
        else {
            rc_race_frame incoming;roam_plat_lock();int got=rc_take_race(&g_s,NC.peer,&incoming);roam_plat_unlock();
            if(got&&incoming.run==NC.control.run&&incoming.event==NC.control.event){
                if(incoming.racers!=NC.stream.racers)cc_cancel(&NC.control,CC_MISMATCH,now);
                else cs_receive(&NC.stream,NC.peer,&incoming,now);
            }
            if(NC.control.phase==CC_LOADING&&NC.stream.seq_init&&!NC.loaded_noted){NC.loaded_noted=1;cc_loaded(&NC.control,now);}
            if(NC.control.phase==CC_RUNNING&&cs_stale(&NC.stream,now))cc_cancel(&NC.control,CC_DISCONNECTED,now);
            if(NC.control.phase==CC_COUNTDOWN&&MR32(NC.race)==3)MR32(NC.race+8)=MR32(0x86518c);
            if(actions&CC_RELEASE_GAME){uint32_t args[2]={NC.race,4};rh_call(c,(uint32_t)(uintptr_t)nc_original_state,0,0,2,args);}
            for(int k=0;k<NC.stream.racers;k++)if(nc_remote_car(NC.car[k])){
                body_set(c,NC.car[k],0);
                rc_pose pose;if(cs_pose(&NC.stream,k,now,dt,&pose))apply_pose(c,&NC.slot[k],&pose,(float)dt/1000000.f);
            }
            /* Free-roam statistics describe different interpolation buffers.
             * Log the actual race buffers so packet starvation, corrections
             * and frame stalls can be distinguished in a real two-PC test. */
            static int64_t next_stats;
            if(NC.control.phase==CC_RUNNING&&now>=next_stats){
                next_stats=now+5000000;
                for(int k=0;k<NC.stream.racers;k++)if(nc_remote_car(NC.car[k])){
                    rc_interp *m=&NC.stream.motion[k];
                    rlog("[coop-motion] slot %d buffered=%d source-gap=%lld us frame=%lld us interp=%llu extrap=%llu hold=%llu snaps=%llu max-correction=%.3f m\n",
                         k,m->n,(long long)(m->n>=2?m->t[m->n-1]-m->t[m->n-2]:0),(long long)dt,
                         (unsigned long long)m->n_interp,(unsigned long long)m->n_extrap,(unsigned long long)m->n_hold,
                         (unsigned long long)m->n_snap,(double)m->err_max);
                }
            }
            if(now>=NC.next_send){
                rc_race_frame outgoing;memset(&outgoing,0,sizeof outgoing);outgoing.to=NC.peer;outgoing.run=NC.control.run;outgoing.event=NC.control.event;
                outgoing.seq=NC.seq++;outgoing.racers=(uint8_t)NC.stream.racers;int complete=1;
                for(int k=0;k<NC.stream.racers;k++)if(NC.host?k!=1:k==1){
                    int i=outgoing.n++;outgoing.car[i].slot=(uint8_t)k;
                    outgoing.car[i].completion=(uint8_t)MR16(NC.record[k]+0x10);
                    outgoing.car[i].position=outgoing.car[i].completion?MR8(NC.record[k]+0xc):0;
                    if(!nc_read_car(NC.car[k],&outgoing.car[i].state,now))complete=0;
                }
                if(complete){roam_plat_lock();rc_send_race(&g_s,&outgoing);roam_plat_unlock();}NC.next_send=now+50000;
            }
        }
    }
    if(actions&CC_RESULT)nc_apply_result(c);
    if(NC.control.phase==CC_CANCELLED&&NC.bound)nc_unbind(c);
    if(cc_next_packet(&NC.control,now,&m)){roam_plat_lock();rc_send_coop(&g_s,&m);roam_plat_unlock();}
    if(NC.phase_logged!=NC.control.phase){NC.phase_logged=NC.control.phase;rlog("[coop] run %08x phase %d reason %u\n",NC.control.run,NC.control.phase,NC.control.cancel_reason);}
}
