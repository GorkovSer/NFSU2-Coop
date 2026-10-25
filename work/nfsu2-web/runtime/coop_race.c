#include "coop_race.h"
#include <string.h>
#define RETRY_US 250000
#define START_DELAY_US 3000000
static int active(const cc_race *r){return r->phase>=CC_INVITED&&r->phase<=CC_RUNNING;}
static int fresh(const cc_race *r,uint32_t run){uint32_t delta=run-r->last_run;return run&&(!r->seen_run||(delta&&delta<0x80000000u));}
static void phase(cc_race *r,int p,int64_t now){r->phase=p;r->since=now;r->next_tx=now;}
static void prepare(cc_race *r,uint32_t run,uint32_t event,int64_t now){
    uint32_t self=r->self,peer=r->peer;int host=r->host;
    cc_init(r,self,peer,host);r->run=r->last_run=run;r->seen_run=1;r->event=event;phase(r,CC_INVITED,now);
}
void cc_init(cc_race *r,uint32_t self,uint32_t peer,int host){memset(r,0,sizeof *r);r->self=self;r->peer=peer;r->host=!!host;}
int cc_offer(cc_race *r,uint32_t run,uint32_t event,int64_t now){
    if(!r->host||active(r)||!event||!r->self||!r->peer||r->self==r->peer||!fresh(r,run))return 0;
    prepare(r,run,event,now);return 1;
}
void cc_cancel(cc_race *r,uint32_t reason,int64_t now){
    if(!active(r))return;
    r->cancel_reason=reason;phase(r,CC_CANCELLED,now);r->terminal_at=now;r->actions=CC_CANCEL;
}
int cc_accept(cc_race *r,uint32_t available_event,int64_t now){
    if(r->host||r->phase!=CC_INVITED)return 0;
    if(available_event!=r->event){cc_cancel(r,CC_MISMATCH,now);return 0;}
    phase(r,CC_WAIT_LOAD,now);return 1;
}
static void arm(cc_race *r,int64_t now){
    if(!r->host||r->phase!=CC_LOADING||!r->local_loaded||!r->peer_loaded)return;
    r->release_at=now+START_DELAY_US;r->go_value=(uint32_t)r->release_at;phase(r,CC_COUNTDOWN,now);
}
int cc_continue_stage(cc_race *r,uint32_t run,int64_t now){
 if(r->phase!=CC_RUNNING||!fresh(r,run)||!r->event)return 0;
 uint32_t event=r->event;prepare(r,run,event,now);phase(r,CC_LOADING,now);return 1;
}
void cc_loaded(cc_race *r,int64_t now){
    if(r->phase!=CC_LOADING||r->local_loaded)return;
    r->local_loaded=1;r->next_tx=now;arm(r,now);
}
static void result(cc_race *r,uint32_t winner,int64_t now){
    r->winner=winner;phase(r,CC_DONE,now);r->terminal_at=now;r->actions|=CC_RESULT;
}
void cc_receive(cc_race *r,uint32_t from,const rc_coop_msg *m,int64_t now,int64_t offset){
    if(from!=r->peer||m->to!=r->self||!m->run||!m->event||m->op<1||m->op>=RC_COOP__N)return;
    if(m->op==RC_COOP_INVITE){
        if(r->host||active(r)||!fresh(r,m->run))return;
        prepare(r,m->run,m->event,now);r->actions=CC_INVITATION;return;
    }
    if(m->run!=r->run||m->event!=r->event)return;
    switch(m->op){
    case RC_COOP_READY:
        if(r->host&&r->phase==CC_INVITED){phase(r,CC_LOADING,now);r->actions|=CC_LOAD_GAME;}break;
    case RC_COOP_LOAD:
        if(!r->host&&r->phase==CC_WAIT_LOAD){phase(r,CC_LOADING,now);r->actions|=CC_LOAD_GAME;}break;
    case RC_COOP_LOADED:
        if(r->host&&r->phase==CC_LOADING){r->peer_loaded=1;arm(r,now);}break;
    case RC_COOP_GO:
        if(!r->host&&r->phase==CC_LOADING&&r->local_loaded){
            uint32_t local_target=m->value+(uint32_t)(uint64_t)offset;
            int32_t delta=(int32_t)(local_target-(uint32_t)(uint64_t)now);
            if(delta<250000||delta>5000000){cc_cancel(r,CC_BAD_CLOCK,now);break;}
            r->release_at=now+delta;r->go_value=m->value;phase(r,CC_COUNTDOWN,now);
        }break;
    case RC_COOP_GO_ACK:
        if(r->host&&(r->phase==CC_COUNTDOWN||r->phase==CC_RUNNING)&&m->value==r->go_value)r->go_acked=1;
        break;
    case RC_COOP_FINISH:
        if(r->host&&r->phase==CC_RUNNING&&m->value>=1&&m->value<=6&&r->claimed_position!=m->value){
            r->claimed_position=(uint8_t)m->value;r->actions|=CC_VERIFY_FINISH;
        }break;
    case RC_COOP_RESULT:
        if(!r->host&&(r->phase==CC_RUNNING||r->phase==CC_COUNTDOWN)&&(m->value==0||m->value==r->self||m->value==r->peer))result(r,m->value,now);
        break;
    case RC_COOP_RESULT_ACK:
        if(r->host&&r->phase==CC_DONE&&m->value==r->winner)r->result_acked=1;break;
    case RC_COOP_CANCEL:
        if(m->value>=CC_WITHDRAWN&&m->value<=CC_BAD_CLOCK)cc_cancel(r,m->value,now);break;
    default:break;
    }
}
void cc_step(cc_race *r,int64_t now,int connected){
    if(!active(r))return;
    if(!connected){cc_cancel(r,CC_DISCONNECTED,now);return;}
    if((r->phase==CC_INVITED||r->phase==CC_WAIT_LOAD)&&now-r->since>=30000000){cc_cancel(r,CC_TIMEOUT,now);return;}
    if(r->phase==CC_LOADING&&now-r->since>=120000000){cc_cancel(r,CC_TIMEOUT,now);return;}
    if(r->phase==CC_COUNTDOWN&&now>=r->release_at){
        if(r->host&&!r->go_acked){cc_cancel(r,CC_TIMEOUT,now);return;}
        phase(r,CC_RUNNING,now);r->actions|=CC_RELEASE_GAME;
    }
}
uint32_t cc_take_actions(cc_race *r){uint32_t actions=r->actions;r->actions=0;return actions;}
void cc_claim_finish(cc_race *r,uint8_t position){if(!r->host&&r->phase==CC_RUNNING&&position>=1&&position<=6){r->claim=position;r->next_tx=0;}}
int cc_confirm_finish(cc_race *r,uint32_t who,uint8_t position,int64_t now){
    if(!r->host||r->phase!=CC_RUNNING||position<1||position>6||(who!=r->self&&who!=r->peer))return 0;
    uint8_t *mine=who==r->self?&r->own_position:&r->peer_position;
    uint8_t other=who==r->self?r->peer_position:r->own_position;
    if(*mine||other==position)return 0;
    *mine=position;
    if(position==1)result(r,who,now);
    else if(r->own_position&&r->peer_position)result(r,0,now);
    return 1;
}
void cc_bot_won(cc_race *r,int64_t now){if(r->host&&r->phase==CC_RUNNING)result(r,0,now);}
int cc_next_packet(cc_race *r,int64_t now,rc_coop_msg *m){
    if(!r->run||now<r->next_tx)return 0;
    uint8_t op=0;uint32_t value=0;
    switch(r->phase){
    case CC_INVITED:if(r->host)op=RC_COOP_INVITE;break;
    case CC_WAIT_LOAD:op=RC_COOP_READY;break;
    case CC_LOADING:op=r->host?RC_COOP_LOAD:r->local_loaded?RC_COOP_LOADED:RC_COOP_READY;break;
    case CC_COUNTDOWN:case CC_RUNNING:
        op=r->host?RC_COOP_GO:RC_COOP_GO_ACK;value=r->go_value;
        if(!r->host&&r->phase==CC_RUNNING&&r->claim){op=RC_COOP_FINISH;value=r->claim;}break;
    case CC_DONE:
        if(now-r->terminal_at>30000000||(r->host&&r->result_acked))return 0;
        op=r->host?RC_COOP_RESULT:RC_COOP_RESULT_ACK;value=r->winner;break;
    case CC_CANCELLED:
        if(now-r->terminal_at>3000000)return 0;
        op=RC_COOP_CANCEL;value=r->cancel_reason;break;
    default:break;
    }
    if(!op)return 0;
    m->op=op;m->to=r->peer;m->run=r->run;m->event=r->event;m->value=value;r->next_tx=now+RETRY_US;return 1;
}
