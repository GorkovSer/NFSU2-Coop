/* Live map overlay only. Never write the career's unlocked event array,
 * sponsor contract, completion records, stage, budget, or save file. */
static struct {
 uint32_t world,peer,nonce,serial;int64_t next_send,expires;
 unsigned n,nseen;uint32_t hash[64],seen[64];
 struct {uint32_t descriptor,marker,event;} marker[64];
} NES;
static uint32_t nes_catalog(uint32_t hash){
 uint32_t table=rd32(0x8383e0),count=rd32(0x8383e4);
 if(!hash||!count||count>4096||!ok(table,count*0x88u))return 0;
 for(unsigned i=0;i<count;i++){
  uint32_t e=table+0x88u*i;
  if(MR32(e+8)==hash&&nc_event_supported(e))return e;
 }
 return 0;
}
static int nes_contains(uint32_t hash){for(unsigned i=0;i<NES.n;i++)if(NES.hash[i]==hash)return 1;return 0;}
static int nes_live_event(uint32_t hash){
 int live=0;roam_plat_lock();int64_t now=roam_plat_now_us();
 for(int k=0;k<RC_TABLE;k++){
  rc_peer *p=&g_s.peer[k];
  if(p->id!=NES.peer||!p->coop||!rc_extension_ready(&g_s,p,RC_EXT_CAREER_MAP,now)||
     p->extensions.nonce!=NES.nonce||!p->event_catalog_seen||p->event_catalog.source!=NES.nonce||
     (p->status!=RC_ST_ROAM&&p->status!=RC_ST_PAUSED)||
     (p->status!=RC_ST_PAUSED&&(now<p->event_catalog_at||now-p->event_catalog_at>=3000000)))continue;
  for(unsigned i=0;i<p->event_catalog.n&&i<64;i++)if(p->event_catalog.event[i]==hash)live=1;
 }
 roam_plat_unlock();return live;
}
static uint32_t nes_available(uint32_t hash){
 if(!nes_contains(hash))return 0;
 int frozen=NC.peer==NES.peer&&NC.control.event==hash&&NC.control.phase>=CC_LOADING&&NC.control.phase<=CC_DONE;
 /* Native map/UI callbacks can run while the frame adapter is paused.
  * Validate against the receive thread's current lease, not frame age. */
 if(!frozen&&(!roaming()||!nes_live_event(hash)))return 0;
 return nes_catalog(hash);
}
uint32_t nes_marker_event(uint32_t career,uint32_t marker){
 if(career!=0x85ad40||!roaming()||!ok(marker,0x34))return 0;
 uint32_t key=MR32(marker+0x30);
 for(unsigned i=0;i<NES.n;i++){
  uint32_t e=nes_available(NES.hash[i]);if(e&&MR32(e+0x28)==key)return e;
 }
 return 0;
}
static int nes_owned_marker(uint32_t d){
 for(unsigned i=0;i<64;i++)if(NES.marker[i].descriptor==d&&
    rd32(d)==NES.marker[i].marker&&rd32(d+4)==NES.marker[i].event&&rd32(d+0x14)==1)return 1;
 return 0;
}
static uint32_t nes_local_entry(unsigned i){
 uint32_t e;
 if(i<64)e=rd32(0x85ad40+0x71c8+4*i);
 else {
  uint32_t d=0x85ad40+24*(i-64);
  if(!rd32(d)||rd32(d+0x14)!=1||nes_owned_marker(d))return 0;
  e=rd32(d+4);
 }
 uint32_t table=rd32(0x8383e0),count=rd32(0x8383e4);
 if(!count||count>4096||!ok(table,count*0x88u)||e<table||
    (e-table)%0x88u||(e-table)/0x88u>=count)return 0;
 return e;
}
static int nes_local(uint32_t hash){
 for(unsigned i=0;i<128;i++){uint32_t e=nes_local_entry(i);if(e&&MR32(e+8)==hash)return 1;}return 0;
}
static uint32_t nes_local_available(uint32_t hash){return nes_local(hash)?nes_catalog(hash):0;}
int nes_event_unlocked(uint32_t list,uint32_t event){
 if(list!=0x85ad40+0x7148||!roaming()||!ok(event,0x88))return 0;
 return nes_available(MR32(event+8))==event;
}
int nes_block_solo_start(void){
 if(!roaming())return 0;
 uint32_t e=rd32(0x863448),hash=ok(e,0x88)?MR32(e+8):0;
 if(!hash||nes_local(hash))return 0;
 for(unsigned i=0;i<NES.nseen;i++)if(NES.seen[i]==hash){rlog("[career-map] shared event requires the co-op start; refusing solo fallback\n");return 1;}
 return 0;
}
static void nes_refresh(CPU *c,uint32_t descriptor){uint32_t arg=descriptor;rh_call(c,0x4cb070,0,0,1,&arg);}
static void nes_markers(CPU *c){
 uint32_t base=0x85ad40;
 /* Drop only entries whose identity is still exactly ours. A native
  * rebuild/reused slot or an event newly unlocked locally is never erased. */
 for(unsigned i=0;i<64;i++)if(NES.marker[i].descriptor){
  uint32_t d=NES.marker[i].descriptor,e=NES.marker[i].event;
  if(!ok(d,24)||rd32(d)!=NES.marker[i].marker||rd32(d+4)!=e||rd32(d+0x14)!=1){memset(&NES.marker[i],0,sizeof NES.marker[i]);continue;}
  uint32_t hash=ok(e,0x88)?MR32(e+8):0;
  if(nes_local(hash)){memset(&NES.marker[i],0,sizeof NES.marker[i]);continue;}
  if(nes_contains(hash))continue;
  memset(GPTR(d),0,24);nes_refresh(c,d);memset(&NES.marker[i],0,sizeof NES.marker[i]);
 }
 for(unsigned i=0;i<NES.n;i++){
  uint32_t e=nes_catalog(NES.hash[i]);if(!e||nes_local(NES.hash[i]))continue;
  int exists=0;uint32_t free_entry=0;int free_owned=-1;
  for(unsigned j=0;j<64;j++){
   uint32_t d=base+24*j;
   if(rd32(d)&&rd32(d+4)==e&&rd32(d+0x14)==1)exists=1;
   if(!rd32(d)&&!free_entry)free_entry=d;
   if(!NES.marker[j].descriptor&&free_owned<0)free_owned=(int)j;
  }
  if(exists||!free_entry||free_owned<0)continue;
  uint32_t key=MR32(e+0x28),marker=rh_call(c,0x4ffc40,0,0,1,&key);
  if(!ok(marker,0x34)||MR32(marker+0x30)!=key)continue;
  memset(GPTR(free_entry),0,24);MR32(free_entry)=marker;MR32(free_entry+4)=e;MR32(free_entry+0x14)=1;
  NES.marker[free_owned].descriptor=free_entry;NES.marker[free_owned].marker=marker;NES.marker[free_owned].event=e;
  int seen=0;for(unsigned k=0;k<NES.nseen;k++)if(NES.seen[k]==NES.hash[i])seen=1;
  if(!seen&&NES.nseen<64)NES.seen[NES.nseen++]=NES.hash[i];
  nes_refresh(c,free_entry);
  rlog("[career-map] added peer event %08x in native map slot %u\n",NES.hash[i],(free_entry-base)/24);
 }
}
void nes_map_rebuilt(uint32_t career){
 if(career!=0x85ad40||!(g_s.conf.extensions&RC_EXT_CAREER_MAP)||!roaming())return;
 /* Called after the native cache builder on the same game thread. Native
  * rh_call ignores CPU; no network thread touches these descriptors. */
 for(unsigned i=0;i<NES.n;){
  if(!nes_live_event(NES.hash[i])){NES.hash[i]=NES.hash[--NES.n];continue;}i++;
 }
 nes_markers(NULL);
}
static void nes_frame(CPU *c,int64_t now){
 if(!(g_s.conf.extensions&RC_EXT_CAREER_MAP))return;
 uint32_t w=world();
 /* Map descriptors live in the persistent career object, not the World.
  * Preserve ownership across a race so old shared entries cannot be echoed
  * back as locally unlocked races after returning to town. */
 if(NES.world!=w){NES.world=w;NES.n=NES.nseen=0;NES.expires=0;}
 if(!roaming())return;
 unsigned stage=rd32(0x85ad40+0x7af8);if(stage>5)return;
 rc_event_catalog incoming;memset(&incoming,0,sizeof incoming);
 uint32_t peer=0,nonce=0;int usable=0;
 roam_plat_lock();
 now=roam_plat_now_us();
 for(int k=0;k<RC_TABLE;k++){
  rc_peer *p=&g_s.peer[k];if(!p->coop||!rc_extension_ready(&g_s,p,RC_EXT_CAREER_MAP,now))continue;
  if(peer){peer=0;usable=0;break;}peer=p->id;nonce=p->extensions.nonce;
  if(p->event_catalog_seen&&p->event_catalog.source==nonce&&
     ((now>=p->event_catalog_at&&now-p->event_catalog_at<3000000)||p->status==RC_ST_PAUSED)&&
     (p->status==RC_ST_ROAM||p->status==RC_ST_PAUSED)){incoming=p->event_catalog;usable=1;}
 }
 roam_plat_unlock();
 NES.peer=peer;NES.nonce=nonce;NES.n=0;NES.expires=usable?now+750000:0;
 if(usable)for(unsigned i=0;i<incoming.n;i++){
  uint32_t e=nes_catalog(incoming.event[i]);
  if(e&&NES.n<64&&!nes_contains(incoming.event[i]))NES.hash[NES.n++]=incoming.event[i];
 }
 nes_markers(c);
 if(!peer||now<NES.next_send)return;
 NES.next_send=now+500000;
 rc_event_catalog outgoing;memset(&outgoing,0,sizeof outgoing);outgoing.to=peer;
 outgoing.destination=nonce;outgoing.stage=(uint8_t)stage;if(!++NES.serial)++NES.serial;outgoing.serial=NES.serial;
 /* Prefer contract events if the bounded wire catalog fills up. Publish
  * only locally available entries; never enumerate locked catalog events. */
 for(unsigned priority=0;priority<2;priority++)for(unsigned i=0;i<128&&outgoing.n<64;i++){
  uint32_t e=nes_local_entry(i);if(!e||((MR8(e+0x34)==1)?0u:1u)!=priority||!nc_event_supported(e))continue;
  uint32_t hash=MR32(e+8);int duplicate=0;for(unsigned j=0;j<outgoing.n;j++)if(outgoing.event[j]==hash)duplicate=1;
  if(hash&&!duplicate)outgoing.event[outgoing.n++]=hash;
 }
 roam_plat_lock();outgoing.source=g_s.conf.extension_nonce;rc_send_event_catalog(&g_s,&outgoing,now);roam_plat_unlock();
}
