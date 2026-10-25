/* Host selects an unlocked career event. A guest can join that exact offered
 * event from its own native catalog even when its career has not unlocked it.
 * Never accept a foreign pointer or silently fall back to another event. */
static uint32_t nc_available(uint32_t hash){
 if(!hash)return 0;
 for(unsigned i=0;i<64;i++){
  uint32_t e=rd32(0x85ad40u+0x71c8u+4*i);
  if(ok(e,0x88)&&MR32(e+8)==hash&&nc_event_supported(e))return e;
 }
 uint32_t local=nes_local_available(hash);if(local)return local;
 uint32_t shared=nes_available(hash);if(shared)return shared;
 if(NC.host||NC.control.event!=hash||NC.control.phase<CC_INVITED||NC.control.phase>CC_DONE)return 0;
 uint32_t table=rd32(0x8383e0),count=rd32(0x8383e4);
 if(!count||count>4096||!ok(table,count*0x88u))return 0;
 for(unsigned i=0;i<count;i++){
  uint32_t e=table+i*0x88u;
  if(MR32(e+8)==hash&&nc_event_supported(e))return e;
 }
 return 0;
}
