/* Preserve vanilla downhill table opponents, add one real network grid car.
 * Never mutate the career catalog. Native constructors receive a scoped copy. */
static struct {uint32_t original,run,event,attempt;int valid;uint8_t copy[0x88];} NMD;
static int nmd_base(void){
 /* Career::Start increments +872c before constructing its series. Keep the
  * same event copy through an in-flight cancellation: changing grid counts
  * half way through native loading would disagree with its series roster.
  * A new native attempt (including a solo retry) can never inherit it. */
 return NMD.valid&&rd32(G_CAREER_RACE)==NMD.original&&rd32(0x85ad40+0x872c)==NMD.attempt;
}
static int nm_prepare_event(uint32_t event,uint32_t run){
 memset(&NMD,0,sizeof NMD);
 if(!ok(event,0x88))return 0;
 if(MR8(event+0xf)!=5||MR16(event+0x18)<0x104e||MR16(event+0x18)>0x1053)return 1;
 if(MR8(event+0x7c)!=0||!NV_FROZEN.valid||NV_FROZEN.run!=run)return 0;
 uint32_t table=rd32(0x8383e0),count=rd32(0x8383e4),source=0;
 if(!count||count>2048||!ok(table,count*0x88))return 0;
 for(uint32_t k=0;k<count;k++){
  uint32_t candidate=table+0x88*k;unsigned type=MR8(candidate+0xf);
  if((type<=2||type==5)&&MR8(candidate+0x7c)>=1&&MR8(candidate+0x7c)<=5&&MR8(candidate+0x42)<=3){source=candidate;break;}
 }
 if(!source)return 0;
 memcpy(NMD.copy,(void*)(uintptr_t)event,0x88);
 memcpy(NMD.copy+0x40,(void*)(uintptr_t)(source+0x40),12);NMD.copy[0x7c]=1;
 NMD.original=event;NMD.run=run;NMD.event=MR32(event+8);NMD.attempt=rd32(0x85ad40+0x872c)+1;NMD.valid=1;return 1;
}
uint32_t nm_downhill_event(uint32_t event){
 return nmd_base()&&event==NMD.original?(uint32_t)(uintptr_t)NMD.copy:event;
}
int nm_downhill_grid(void){
 return nmd_base()&&rd32(0x89e7a0)>=0x104e&&rd32(0x89e7a0)<=0x1053&&rd32(0x89e7c4)==1;
}
static int nmd_entries(uint32_t entry[3]){
 memset(entry,0,3*sizeof *entry);uint32_t head=0x864630,p=rd32(head);int mask=0,real=0;
 for(int k=0;p&&p!=head&&k<16;k++,p=rd32(p)){
  if(!ok(p,0x20))return 0;
  uint32_t id=MR32(p+8);
  if(id==0||id==100){unsigned bit=id==0?1u:2u;if(real&(int)bit)return 0;real|=(int)bit;continue;}
  if(id<101||id>103)return 0;
  int index=(int)id-101;if(mask&(1<<index))return 0;
  float score=MRF32(p+0x10);if(!__builtin_isfinite(score)||score<0||score>(float)RM_MAX_SCORE)return 0;
  entry[index]=p;mask|=1<<index;
 }
 return p==head&&mask==7&&real==3;
}
/* 56d7b0 builds only the human plus three virtual scores on downhill.
 * The added physical car100 needs its own row before 56dd30 runs. Native
 * 56d700/56d7b0 only unlink rows (no free); use one stable owned node. */
static uint32_t NMD_PLAYER_ROW[8];
static char NMD_PLAYER_NAME[32]="Co-op";
void nm_downhill_board_built(void){
 if(!nm_downhill_grid())return;
 const uint32_t head=0x864630;uint32_t p=rd32(head),own=0;unsigned mask=0;
 for(unsigned i=0;p!=head&&i<16;i++,p=rd32(p)){
  if(!ok(p,32))return;
  uint32_t id=MR32(p+8);if(id==100)return;
  if(id==0)own=p;else if(id>=101&&id<=103)mask|=1u<<(id-101);else return;
 }
 if(p!=head||!own||mask!=7)return;
 uint32_t tail=rd32(head+4);if(!ok(tail,32)||rd32(tail)!=head)return;
 /* Keep the real player's row category and replace its ID/name/score.
  * Every rebuild clears native links before this callback is reached. */
 memcpy(NMD_PLAYER_ROW,GPTR(own),sizeof NMD_PLAYER_ROW);
 roam_plat_lock();
 for(int k=0;k<RC_TABLE;k++)if(g_s.peer[k].id==NC.peer){
  snprintf(NMD_PLAYER_NAME,sizeof NMD_PLAYER_NAME,"%s",g_s.peer[k].name);break;
 }
 roam_plat_unlock();
 NMD_PLAYER_ROW[0]=head;NMD_PLAYER_ROW[1]=tail;NMD_PLAYER_ROW[2]=100;
 NMD_PLAYER_ROW[4]=0;NMD_PLAYER_ROW[6]=(uint32_t)(uintptr_t)NMD_PLAYER_NAME;
 uint32_t row=(uint32_t)(uintptr_t)NMD_PLAYER_ROW;
 MR32(tail)=row;MR32(head+4)=row;
 rlog("[downhill] physical player100 score row inserted before native scoring\n");
}
