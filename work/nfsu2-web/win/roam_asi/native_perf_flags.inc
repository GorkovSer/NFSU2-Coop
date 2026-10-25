/* Native5ac410 sets transmitted one bits, but Clear(5a6380) visits only
 * upgrade entries present in the performance DB. Constructor defaults in
 * other flag slots otherwise survive a transmitted zero. This byte range
 * is precisely the 68-byte flag array consumed by the native decoder. */
static void nv_clear_perf_flags(uint32_t ride){
 for(unsigned i=0;i<68;i++)MR8(ride+0x340u+i)=0;
}
#ifdef NATIVE_ENGINE_LAB
static void nv_lab_flags(const char *where,uint32_t ride,const uint8_t *perf){
 unsigned expected=0,actual=0;
 for(unsigned i=56;i<68;i++){expected|=((perf[2+i/8]>>(7-i%8))&1u)<<(i-56);actual|=(MR8(ride+0x340+i)!=0)<<(i-56);}
 rlog("[lab-perf] %s ride=%08x expected=%03x actual=%03x\n",where,ride,expected,actual);
}
#endif
