#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include "roam_core.h"
typedef struct {uint32_t car;char name[RC_NAME_MAX+1];} Slot;
static struct {int enabled,bound,host;uint32_t peer;Slot slot[6];} NC;
static rc_session g_s;
static int live=1,running=1,locked,writes,checks;
#define CHECK(x) do{checks++;if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);exit(1);}}while(0)
static int nc_running(void){return running;}
static int nc_same_world(void){return live;}
static void roam_plat_lock(void){CHECK(!locked);locked=1;}
static void roam_plat_unlock(void){CHECK(locked);locked=0;}
static void set_name(Slot *s){CHECK(!locked);CHECK(s==&NC.slot[0]||s==&NC.slot[1]);writes++;}
#include "native_race_names.inc"
int main(void){
 NC.enabled=NC.bound=1;NC.peer=200;NC.slot[0].car=10;NC.slot[1].car=11;
 strcpy(g_s.conf.name,"OwnName");g_s.peer[3].state=RC_PS_EST;g_s.peer[3].id=200;strcpy(g_s.peer[3].name,"PeerName");
 g_s.peer[0].state=RC_PS_EST;g_s.peer[0].id=300;strcpy(g_s.peer[0].name,"Unrelated");
 strcpy(NC.slot[2].name,"Bot");
 for(int host=0;host<=1;host++){
  NC.host=host;nc_names_frame();
  CHECK(!strcmp(NC.slot[host?0:1].name,"OwnName"));CHECK(!strcmp(NC.slot[host?1:0].name,"PeerName"));
  CHECK(nc_name_slot(0)==&NC.slot[host?1:0]);CHECK(!nc_name_slot(1));CHECK(!strcmp(NC.slot[2].name,"Bot"));
 }
 int before=writes;live=0;nc_names_frame();CHECK(writes==before&&!nc_name_slot(0));live=1;
 running=0;nc_names_frame();CHECK(writes==before);running=1;
 NC.bound=0;nc_names_frame();CHECK(writes==before);
 printf("PASS native race names: %d checks; canonical human slots, session ID, both roles, local/peer labels, bot preservation and lifetime guards; mock only\n",checks);
}
