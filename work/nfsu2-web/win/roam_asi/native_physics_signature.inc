static int nv_physics_signature(uint32_t ride,uint32_t *digest){
 if(!digest||!ok(ride,0x8f0)||MR32(ride+0x56c)!=64||MR32(ride+0x57c)!=ride+0x46c||
    MR32(ride+0x38c)!=ride+0x2f0||MR32(ride+0x390)!=ride+0x40)return 0;
 /* Hash known fields, never raw structs, padding, transient caches or native
  * pointers. Both games calculate the rest with their original physics code. */
 uint8_t bytes[512];rc_w w;rc_w_init(&w,bytes,sizeof bytes);rc_put32(&w,1);
 for(int field=0;field<74;field++){
  uint32_t offset=field<3?0x40+4*field:field<7?0x60+0x30*(field-3):
                  field<71?0x46c+4*(field-7):0x570+4*(field-71);
  float value=MRF32(ride+offset);if(!__builtin_isfinite(value))return 0;
  if(field>=3&&field<7&&(value<=0.05f||value>=2.f))return 0;
  rc_putf(&w,value==0.f?0.f:value);
 }
 for(int i=0;i<10;i++)rc_put8(&w,MR32(ride+0x318+4*i)!=0);
 for(int i=0;i<68;i++)rc_put8(&w,MR8(ride+0x340+i)!=0);
 rc_putb(&w,GPTR(ride+0x398),41);
 if(w.err)return 0;
 *digest=rc_crc32(bytes,w.n);return 1;
}
