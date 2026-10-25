/* Presentation of the host's live ranking. No completion, laps, control,
 * physics or owner finish stamp is synthesized by this adapter. */
static void nc_positions_frame(void){
 if(NC.host||!NC.bound||NC.control.phase!=CC_RUNNING||!nc_same_world()||
    !NC.stream.seq_init||cs_stale(&NC.stream,roam_plat_now_us()))return;
 int n=NC.stream.racers;uint8_t rank[RC_RACE_CARS]={0};unsigned mask=0;
 if(n<2||n>RC_RACE_CARS)return;
 for(int k=0;k<n;k++)if(k!=1){
  unsigned p=NC.stream.position[k];
  if(!p||p>(unsigned)n||(mask&(1u<<p)))return;
  rank[k]=(uint8_t)p;mask|=1u<<p;
 }
 for(int p=1;p<=n;p++)if(!(mask&(1u<<p)))rank[1]=(uint8_t)p;
 /* Verify every native board entry before writing any of the set. */
 unsigned records=0;
 for(int i=0;i<n;i++){
  uint32_t rec=rd32(NC.race+0xbd0+4*i);int k=0;
  for(;k<n&&NC.record[k]!=rec;k++);
  if(k==n||(records&(1u<<k))||!ok(rec,0x20)||rd32(rec)!=NC.car[k]||MR16(rec+0x10))return;
  records|=1u<<k;
 }
 for(int k=0;k<n;k++){
  MR8(NC.record[k]+0xa)=rank[k];
  MR32(NC.race+0xbd0+4u*(rank[k]-1u))=NC.record[k];
 }
}
