/* Exact-EXE game-thread dependency adapter. Native CarLoader's wheel model
 * requests assume GEOMETRY_<manufacturer>.BIN headers were loaded by the
 * frontend/world loader. A stock local car does not ensure a peer's brand.
 * Retain one reference per stock brand for this process, never per frame or
 * per car. No model pointers, map flags or local equipment are fabricated. */
static const char *const NRIM_BRANDS[]={"5ZIGEN","ADVAN","AVUS","BBS","DAVIN","DONZ","ENKEI","FOXX","GIANELLE","GIOVANNA","KAIZER","KONIG","LEXANI","LOWENHART","MOMO","NFSU","OASIS","OZ","RACINGHART","ROTA","SPINNER","VOLK","WELDWHEEL","WORK"};
static struct {int requested,failed;char path[80];} NRIM[24];
static uint32_t nrim_hash(const char *s){
 uint32_t h=0xffffffffu;for(;*s;s++){unsigned char ch=(unsigned char)*s;if(ch>='a'&&ch<='z')ch-=32;h=h*33u+ch;}return h;
}
static int nrim_brand_ready(CPU *c,uint32_t brand){
 unsigned index=0;for(;index<24;index++)if(nrim_hash(NRIM_BRANDS[index])==brand)break;
 if(index==24)return -1; /* An add-on manufacturer requires its own catalog adapter. */
 if(NRIM[index].failed)return -1;
 if(!NRIM[index].requested){
  snprintf(NRIM[index].path,sizeof NRIM[index].path,"CARS\\WHEELS\\GEOMETRY_%s.BIN",NRIM_BRANDS[index]);
  uint32_t args[4]={(uint32_t)(uintptr_t)NRIM[index].path,0,0,0};
  if(!rh_call(c,0x494180,0,0,4,args)){
   NRIM[index].failed=1;rlog("[rims] missing geometry pack: %s\n",NRIM[index].path);return -1;
  }
  NRIM[index].requested=1;
  rlog("[rims] retained native manufacturer pack: %s\n",NRIM[index].path);
 }
 /* Find the actual native resource; its parsed model-header group/table
  * must exist. CarLoader will still request/wait for the selected LODs. */
 uint32_t resource=gcall1(c,0x48ce10,0x82dbc8,(uint32_t)(uintptr_t)NRIM[index].path);
 if(!ok(resource,0xbc))return 0;
 uint32_t group=rd32(resource+0xb8);
 if(!ok(group,0x80))return 0;
 uint32_t count=rd32(group+0xc);
 return count>0&&count<=16384&&ok(rd32(group+0x74),8)&&ok(rd32(group+0x78),24);
}
static int nrim_ready(CPU *c,uint32_t ride){
 if(rd32(G_FLOW)!=6||!ok(ride,0x8f0))return -1;
 int ready=1;
 /* Actual RideInfo pointer slots for the front/rear rims. The UI's part
  * enumeration is different (WHEEL enum28 maps to RideInfo slot29). */
 for(unsigned slot=29;slot<=30;slot++){
  uint32_t part=rd32(ride+0x590+4*slot);if(!part)continue;
  if(!ok(part,14))return -1;
  uint32_t args[2]={0xebb03e66u,0}; /* Native wheel manufacturer attribute. */
  uint32_t brand=rh_call(c,0x61b500,part,0,2,args);
  if(!brand)continue; /* Stock rims are embedded in the car model. */
  int result=nrim_brand_ready(c,brand);if(result<0)return -1;if(!result)ready=0;
 }
 return ready;
}
