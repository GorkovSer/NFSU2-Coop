#include <stdio.h>
#include <stdlib.h>
static int city,raw,held;
static float dt;
#define G_WORLDDT 0x865154
#define MRF32(a) dt
static int roaming(void){return city;}
static int nc_pause_raw(int p){return p||raw;}
static int nc_pause_held(void){return held;}
#include "native_menu_gate.inc"
int main(void){
 int checks=0;float values[]={0,.016f,-.01f,__builtin_nanf(""),__builtin_inff()};
 for(int k=0;k<5;k++)for(city=0;city<2;city++)for(raw=0;raw<2;raw++)for(held=0;held<2;held++){
  dt=values[k];checks++;if(nw_frame_allowed()!=(!held&&!(city&&(raw||k!=1))))return 1;
 }
 puts("PASS menu gate: 40 combinations; city pause/zero/nonfinite world dt blocks traffic mutation before co-op acceptance; race loading retained, negotiated race pause respected; mock only");return checks!=40;
}
