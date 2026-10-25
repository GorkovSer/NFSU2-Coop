/* Included after build_ride and all shared loader helpers. Each slot keeps
 * its two embedded-curve rides for the process lifetime and reuses them. */
typedef struct {
 uint32_t ride,saved,handle,world,car,record,brain,type,skin,physics_digest;
 int applied,started,strict;
 int64_t since;
} nv_slot;
static nv_slot *NV_ATTACHING;
/* Scoped exclusively by the loading-phase human recovery adapter. */
static uint32_t NV_OWNER_CAR;
static struct {uint32_t car,world,record;} NV_FREE_ATTACH;
static void nv_free_setride(CPU *c,uint32_t car,uint32_t ride){
 if(car==player_car()){rlog("[vehicle] refusing remote free-roam attachment to local human car\n");return;}
 uint32_t old_car=NV_FREE_ATTACH.car,old_world=NV_FREE_ATTACH.world,old_record=NV_FREE_ATTACH.record;
 NV_FREE_ATTACH.car=car;NV_FREE_ATTACH.world=world();NV_FREE_ATTACH.record=rd32(car+0x14);
 gcall1(c,FN_CAR_SETRIDE,car,ride);
 NV_FREE_ATTACH.car=old_car;NV_FREE_ATTACH.world=old_world;NV_FREE_ATTACH.record=old_record;
}
int nv_owned_performance(uint32_t ride){
 if(NV_FREE_ATTACH.car&&rd32(G_FLOW)==6&&world()==NV_FREE_ATTACH.world&&
    ok(NV_FREE_ATTACH.car,0x9e0)&&rd32(NV_FREE_ATTACH.car+0x14)==NV_FREE_ATTACH.record&&
    ride==NV_FREE_ATTACH.record+0x10)return 1;
 const nv_slot *s=NV_ATTACHING;
 return s&&s->strict&&s->started&&world()==s->world&&rd32(G_FLOW)==6&&
        ok(s->car,0x9e0)&&rd32(s->car+0x14)==s->record&&ride==s->record+0x10;
}
static int nv_buffers(CPU *c,nv_slot *s){
 if(s->ride)return 1;
 uint32_t size=2*RIDE_SZ,mem=rh_call(c,FN_MALLOC,0,0,1,&size);
 if(!mem)return 0;
 memset(GPTR(mem),0,size);s->ride=mem;s->saved=mem+RIDE_SZ;return 1;
}
static int nv_identity(const nv_slot *s){
 return s->started&&world()==s->world&&rd32(G_FLOW)==6&&
        (s->car!=player_car()||s->car==NV_OWNER_CAR)&&ok(s->car,0x9e0)&&
        rd32(s->car+0x14)==s->record&&rd32(s->car+0x2c)==s->brain;
}
static void nv_reset(CPU *c,nv_slot *s){
 /* Shops can keep the World pointer while its live scene is suspended.
  * Do not unload a model still attached to a retained car/preview. The
  * bounded slot is released after world teardown, or restored on return. */
 if(s->applied&&world()==s->world&&rd32(G_FLOW)!=6)return;
 /* If native ownership changed to the human, neither overwrite that car
  * nor drop the resources still attached to it. Keep the bounded slot
  * pinned until world teardown makes releasing it unambiguous. */
 if(s->applied&&world()==s->world&&s->car==player_car())return;
 /* A controller/record replacement is not proof of model detachment.
  * Retain the handle and buffers until teardown rather than freeing a
  * possibly live render dependency or restoring into a different owner. */
 if(s->applied&&world()==s->world&&!nv_identity(s))return;
 if(s->applied&&world()==s->world&&rd32(G_FLOW)==6&&s->car!=player_car()&&
    ok(s->car,0x9e0)&&rd32(s->car+0x14)==s->record&&rd32(s->car+0x2c)==s->brain){
  body_set(c,s->car,0);gcall1(c,FN_CAR_SETRIDE,s->car,s->saved);
 }
 if(s->handle)unload_later(s->handle);
 uint32_t ride=s->ride,saved=s->saved;memset(s,0,sizeof *s);s->ride=ride;s->saved=saved;
}
static int nv_begin(CPU *c,nv_slot *s,uint32_t car,const rc_carcfg *cfg,uint32_t skin,int64_t now,int performance_required){
 if(s->started||(car==player_car()&&car!=NV_OWNER_CAR)||!ok(car,0x9e0)||!cfg->type_hash||(performance_required&&!cfg->has_perf))return -1;
 /* A retired request still owns its RideInfo until the native loader
  * releases the handle. Drain before overwriting our reusable buffers,
  * including during a race; otherwise swaps fill all cache slots. */
 flush_unloads(c);
 gmem_init(c);if(!g_gmem)return -1;
 if(!gcall1(c,FN_TYPE_BY_HASH,0,cfg->type_hash)||!nv_buffers(c,s))return -1;
 if(performance_required&&(!skin||!skin_textures(c,skin))){rlog("[vehicle] native paint slot %u is unavailable\n",skin);return -1;}
 char why[96]={0};int missing=build_ride(c,cfg,s->ride,skin,why,sizeof why);
 if(missing||why[0]){rlog("[vehicle] %s rejected: %s (%d unavailable parts)\n",cfg->type_name,why,missing);return -1;}
 if(performance_required&&(!nv_matches_cfg(cfg,s->ride)||!nv_physics_signature(s->ride,&s->physics_digest))){
  rlog("[vehicle] rebuild differs from requested equipment/appearance/physics\n");return -1;
 }
 uint32_t old=rd32(car+0x10);if(!ok(old,0x8f0))return -1;
 /* Ordinary traffic has native stock materials, not a player's composited
  * DUMMY_WHEEL1/SKIN1 targets. Never let a traffic load replace those. */
 if(!performance_required)memcpy(GPTR(s->ride+0x584),GPTR(old+0x584),12);
 gcall1(c,0x413470,s->saved,old);
 s->car=car;s->record=rd32(car+0x14);s->world=world();s->brain=rd32(car+0x2c);s->type=cfg->type_hash;s->since=now;
 s->skin=skin;s->strict=performance_required;
 int rims=nrim_ready(c,s->ride);if(rims<0)return -1;
 if(rims){uint32_t args[3]={s->ride,0,0};s->handle=rh_call(c,FN_LOADER_LOAD,G_CARLOADER,0,3,args);if(!s->handle)return -1;}
 s->started=1;rlog("[vehicle] loading %s, actual native performance, car %08x handle %x\n",cfg->type_name,car,s->handle);return 0;
}
static int nv_attached_matches(const nv_slot *s){
 uint32_t ride=rd32(s->car+0x10),physical=0;
 if(rd32(s->car+0x2c)!=s->brain||rd32(rd32(s->car+0xc)+0xd0)!=s->type||!ok(ride,0x8f0)||
    rd32(s->car+0x20)!=ride+0x40||MR32(ride+0x57c)!=ride+0x46c||
    MR32(ride+0x38c)!=ride+0x2f0||MR32(ride+0x390)!=ride+0x40||
    memcmp(GPTR(ride+4),GPTR(s->ride+4),4)||
    memcmp(GPTR(ride+0x46c),GPTR(s->ride+0x46c),256)||
    memcmp(GPTR(ride+0x318),GPTR(s->ride+0x318),0x6c)||
    memcmp(GPTR(ride+0x398),GPTR(s->ride+0x398),41))return 0;
 if(s->strict&&(memcmp(GPTR(ride+0x584),GPTR(s->ride+0x584),12)||
    memcmp(GPTR(ride+0x590),GPTR(s->ride+0x590),RC_PART_SLOTS*4)||
    !nv_physics_signature(ride,&physical)||physical!=s->physics_digest))return 0;
 return 1;
}
static int nv_poll(CPU *c,nv_slot *s,int64_t now){
 if(!nv_identity(s))return -1;
 int rims=nrim_ready(c,s->ride);if(rims<0)return -1;
 if(!nv_identity(s))return -1;
 if(!rims)return now-s->since>30000000?-1:0;
 if(!s->handle){
  uint32_t request[3]={s->ride,0,0};s->handle=rh_call(c,FN_LOADER_LOAD,G_CARLOADER,0,3,request);
  if(!s->handle)return -1;
  if(!nv_identity(s))return -1;
 }
 if(s->applied){
  if(!nv_attached_matches(s))return -1;
  uint32_t retained=loader_entry(s->handle);
  int loaded=retained&&gcall1(c,FN_LOADER_LOADED,G_CARLOADER,retained);
  return loaded&&nv_identity(s)?1:-1;
 }
 uint32_t args[2]={0,0};rh_call(c,FN_LOADER_SERVICE,G_CARLOADER,0,2,args);
 if(!nv_identity(s))return -1;
 uint32_t entry=loader_entry(s->handle);
 if(!entry||!gcall1(c,FN_LOADER_LOADED,G_CARLOADER,entry))return now-s->since>30000000?-1:0;
 if(!nv_identity(s))return -1;
 gcall1(c,FN_LOADER_WAIT,G_CARLOADER,s->handle);
 if(!nv_identity(s))return -1;
 /* Native streaming callbacks may run during Wait. Revalidate the full
  * dependency group (body, parts/wheels and composite textures) before
  * exposing the rebuilt model on the same retained handle. */
 entry=loader_entry(s->handle);
 if(!entry||!gcall1(c,FN_LOADER_LOADED,G_CARLOADER,entry))return now-s->since>30000000?-1:0;
 if(!nv_identity(s))return -1;
 if(s->car!=NV_OWNER_CAR)body_set(c,s->car,0);
 nv_slot *previous=NV_ATTACHING;NV_ATTACHING=s;
 gcall1(c,FN_CAR_SETRIDE,s->car,s->ride);
 NV_ATTACHING=previous;s->applied=1;
 if(!nv_identity(s)||!nv_attached_matches(s))return -1;
 if(s->car!=NV_OWNER_CAR)body_set(c,s->car,0);
 rlog("[vehicle] ready: actual native physics and requested appearance retained, skin %u, signature %08x; AI brain retained\n",s->skin,s->physics_digest);return 1;
}
