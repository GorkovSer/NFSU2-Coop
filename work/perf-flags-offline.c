#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static uint8_t memory[0x8f0];
#define MR8(a) memory[a]
#include "native_perf_flags.inc"
int main(void){
 unsigned checks=0;
 for(unsigned seed=0;seed<256;seed++){
  memset(memory,0xa5,sizeof memory);uint8_t wanted[68],wire[9]={0};
  for(unsigned i=0;i<68;i++){wanted[i]=((seed*73+i*37)>>(i%8))&1;wire[i/8]|=wanted[i]<<(7-i%8);}
  nv_clear_perf_flags(0);
  /* Exact set-only semantics of the native decoder after its DB-scoped
   * Clear: zeros do not write and must not preserve constructor defaults. */
  for(unsigned i=0;i<68;i++)if((wire[i/8]>>(7-i%8))&1)memory[0x340+i]=1;
  for(unsigned i=0;i<sizeof memory;i++){
   uint8_t expected=i>=0x340&&i<0x384?wanted[i-0x340]:0xa5;
   checks++;if(memory[i]!=expected){fprintf(stderr,"FAIL seed%u byte%x\n",seed,i);return 1;}
  }
 }
 printf("PASS native performance flags: %u checks; all 68 transmitted zeros/ones replace constructor defaults, adjacent ride fields unchanged; mock native decoder\n",checks);
}
