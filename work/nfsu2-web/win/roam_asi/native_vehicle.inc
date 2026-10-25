/* RidePhysicsInfo contains its 64-point engine curve at +0x45c, followed by
 * the lookup descriptor at +0x55c. RideInfo::Init initializes tuning but
 * leaves that descriptor untouched. Zeroing a new RideInfo therefore requires
 * reconstructing it before PhysicsUpdate. The native copy routine 413470 /
 * 413400 copies the embedded points and rebases their descriptor itself.
 * An external curve buffer would calculate correctly but copy as all zero. */
static int nv_physics(CPU *c,uint32_t ride){
 if(!ok(ride,0x8f0))return 0;
 MR32(ride+0x56c)=64;
 MR32(ride+0x57c)=ride+0x46c;
 uint32_t args[3]={1,1,1};rh_call(c,FN_PHYS_UPDATE,ride+0x10,0,3,args);
 if(MR32(ride+0x56c)!=64||MR32(ride+0x57c)!=ride+0x46c||
    MR32(ride+0x38c)!=ride+0x2f0||MR32(ride+0x390)!=ride+0x40)return 0;
 for(unsigned i=0;i<64;i++)if(!__builtin_isfinite(MRF32(ride+0x46c+4*i)))return 0;
 return 1;
}
