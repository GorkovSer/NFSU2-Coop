/* Local-profile cash only. Called on the game thread after the original
 * host finish callback / validated authoritative co-op result. */
static uint32_t nr_profile(void){char name[17];cstr(G_PROFILE_NAME,name,sizeof name);return name[0]?rc_fnv1a(name):0;}
void nr_reward_solo_start(void){memset(&NR_REWARD,0,sizeof NR_REWARD);}
int nr_reward_scope(uint32_t career,uint32_t event){
 return career==0x85ad40&&NR_REWARD.initialized&&NR_REWARD.terms&&!NR_REWARD.fault&&
        NR_REWARD.event==event&&nr_profile()==NR_REWARD.profile;
}
static int nr_reward_frame(CPU *c,int64_t now){
 (void)c;
 if(!NC.bound||!nc_same_world())return -1;
 uint32_t profile=nr_profile();if(!profile)return -1;
 if(!NR_REWARD.initialized){
  rw_init(&NR_REWARD,g_s.conf.self_id,NC.peer,NC.control.run,NC.control.event,profile,NC.host);
  if(NC.host){
   /* Native 532da0 first checks whether the event was won; quoting it at
    * the start can return zero. Freeze its verified prize formula directly,
    * without pretending that an unfinished career event is already won. */
   uint32_t event=rd32(G_CAREER_RACE);if(!ok(event,0x88))return -1;
   uint32_t amount=rd32(event+0x30);
   if(MR8(event+0x34)==1){
    uint32_t stage=rd32(0x85ad40+0x7af8);if(stage>=8)return -1;
    uint32_t contract=rd32(0x85ad40+0x73c0+stage*0x104);
    if(contract){if(!ok(contract,4))return -1;amount=MR16(contract+2);}
   }
   if(!rw_offer(&NR_REWARD,amount))return -1;
   rlog("[reward] host prize frozen: %u for EACH player; separate local budgets\n",amount);
  }
 }
 if(NR_REWARD.fault||profile!=NR_REWARD.profile)return -1;
 rc_coop_msg m;
 if(NC.control.phase<=CC_LOADING&&rw_next(&NR_REWARD,now,&m)){roam_plat_lock();rc_send_coop(&g_s,&m);roam_plat_unlock();}
 if(NC.control.phase<=CC_LOADING&&NR_REWARD.terms&&(rd32(0x861e74)>RW_MAX_BALANCE||NR_REWARD.amount>RW_MAX_BALANCE-rd32(0x861e74)))return -1;
 return rw_ready(&NR_REWARD);
}
static int nr_reward_result(int won){
 if(rw_authorize(&NR_REWARD,won)<=0)return 0;
 uint32_t before=rd32(0x861e74),after=before;
 int credit=rw_credit(&NR_REWARD,nr_profile(),before,&after);
 if(credit<0){rlog("[reward] refused credit: profile changed or local balance overflow\n");return 0;}
 if(credit){MR32(0x861e74)=after;rlog("[reward] run %08x event %08x local credit %u, balance %u -> %u (once)\n",NR_REWARD.run,NR_REWARD.event,after-before,before,after);}
 return NR_REWARD.paid;
}
