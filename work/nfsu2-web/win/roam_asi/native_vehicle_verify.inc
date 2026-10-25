/* Read-only verification of the exact local EXE's RideInfo. Skin texture
 * hashes at +584..58c are deliberately not part of the network signature:
 * every native racer must retain its own DUMMY_SKIN/WHEEL/SPINNER slot. */
#include "native_physics_signature.inc"
static int nv_matches_equipment(const rc_carcfg *cfg,uint32_t ride,int include_tuning){
 uint32_t types=rd32(G_CARTYPES),owners=rd32(G_PARTOWNERS);
 if(!cfg||!cfg->has_perf||!ok(ride,0x8f0)||!types||!owners||MR32(ride)>=256)return 0;
 uint32_t ti=types+MR32(ride)*0x890;
 if(!ok(ti,0xd4)||MR32(ti+0xd0)!=cfg->type_hash||memcmp(GPTR(ride+4),cfg->ride_b,4))return 0;
 uint8_t perf[RC_PERF_BYTES]={0};
 for(int i=0;i<10;i++)if(MR32(ride+0x318+4*i))perf[i>>3]|=(uint8_t)(0x80u>>(i&7));
 for(int i=0;i<68;i++)if(MR8(ride+0x340+i))perf[2+(i>>3)]|=(uint8_t)(0x80u>>(i&7));
 memcpy(perf+11,GPTR(ride+0x398),41);if(memcmp(perf,cfg->perf,include_tuning?sizeof perf:11))return 0;
 int expected[RC_PART_SLOTS];for(int i=0;i<RC_PART_SLOTS;i++)expected[i]=-1;
 for(int i=0;i<cfg->n_parts;i++){
  unsigned slot=cfg->part[i].slot;if(slot>=RC_PART_SLOTS||expected[slot]>=0)return 0;
  expected[slot]=i;
 }
 for(int slot=0;slot<RC_PART_SLOTS;slot++){
  uint32_t part=rd32(ride+0x590+4*slot);int i=expected[slot];
  if(i<0){if(part)return 0;continue;}
  if(!ok(part,0xe))return 0;
  uint32_t key=(uint32_t)MR16(part+2)<<16|MR16(part);
  if(key!=cfg->part[i].key||rd32(owners+4u*MR8(part+7))!=cfg->part[i].owner)return 0;
 }
 return 1;
}
static int nv_matches_cfg(const rc_carcfg *cfg,uint32_t ride){return nv_matches_equipment(cfg,ride,1);}
