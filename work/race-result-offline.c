#include <stdint.h>
#include <stdio.h>
#include <assert.h>
typedef void CPU;
#define G_CAREER_RACE 1
static struct{int result_applied,host;uint32_t record[2],race;struct{int racers;}stream;struct{uint32_t winner;}control;}NC;
static int live=1,valid=1,indexed=1,reward=1,finish=1,mode,ready=1,special=1;
static unsigned credits,calls,rank=4,last_position;static uint16_t completed;
#define MR16(p) (completed)
#define MR8(p) (rank)
static int nc_same_world(void){return live;}
static uint32_t rd32(uint32_t p){return p==G_CAREER_RACE?9:p==NC.race+0xbd0&&indexed?100:0;}
static int ok(uint32_t p,unsigned n){return valid&&p==100&&n==0x20;}
static int nm_mode(uint32_t p){(void)p;return mode;}
static int nm_frame(CPU*c,int64_t now){(void)c;(void)now;return ready;}
static int nm_result_ready(void){return ready;}
static int64_t roam_plat_now_us(void){return 1;}
static int nr_reward_result(int won){(void)won;if(reward)credits=1;return reward;}
static int nm_apply_result(CPU*c){(void)c;return special;}
static void nc_original_finish(void){}
static uint32_t rh_call(CPU*c,uint32_t f,uint32_t self,uint32_t edx,int n,uint32_t*a){
 (void)c;(void)f;assert(!self&&!edx&&n==5&&a[0]==NC.race&&a[1]==0&&a[2]>=1&&a[2]<=(unsigned)NC.stream.racers&&a[4]==(NC.control.winner?1u:3u));
 last_position=a[2];
 calls++;if(finish)completed=1;return 0;
}
#include "native_race_result.inc"
int main(void){
 NC.host=1;NC.race=1000;NC.record[0]=100;NC.stream.racers=2;NC.control.winner=5;
 valid=0;nc_apply_result(0);assert(!credits&&!calls&&!NC.result_applied);
 valid=1;indexed=0;nc_apply_result(0);assert(!credits&&!calls&&!NC.result_applied);
 indexed=1;reward=0;nc_apply_result(0);assert(!calls&&!NC.result_applied);
 reward=1;finish=0;nc_apply_result(0);assert(credits==1&&calls==1&&!NC.result_applied);
 finish=1;nc_apply_result(0);assert(credits==1&&calls==2&&NC.result_applied);
 nc_apply_result(0);assert(calls==2);
 NC.result_applied=0;nc_apply_result(0);assert(calls==2&&NC.result_applied);
 NC.result_applied=0;mode=1;ready=0;nc_apply_result(0);assert(!NC.result_applied);
 ready=1;special=-1;nc_apply_result(0);assert(!NC.result_applied);
 special=0;nc_apply_result(0);assert(!NC.result_applied&&calls==2);
 special=1;nc_apply_result(0);assert(NC.result_applied);
 mode=0;NC.control.winner=0;NC.result_applied=completed=0;rank=2;
 nc_apply_result(0);assert(NC.result_applied&&last_position==2);
 NC.result_applied=completed=0;rank=0;nc_apply_result(0);assert(last_position==2);
 NC.result_applied=completed=0;rank=99;nc_apply_result(0);assert(last_position==2);
 puts("PASS race result: deferred/invalid native completion retries, one credit, completed host and special-mode isolation");
}
