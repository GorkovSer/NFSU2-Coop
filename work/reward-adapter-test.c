/* Exercises the actual game-thread adapter against mock profile/bank reads. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "coop_race.h"
#include "reward_sync.h"
typedef struct {int unused;} CPU;
#define G_PROFILE_NAME 0x83a9e0
#define G_CAREER_RACE 0x863470
static uint32_t bank,native_prize;
static uint8_t event_record[0x88];
static uint16_t contract[4];
static int has_contract;
static char profile_name[17]="PlayerOne";
#define MR32(a) (*bank_pointer(a))
static uint32_t *bank_pointer(uint32_t a){if(a!=0x861e74)abort();return &bank;}
static uint32_t rd32(uint32_t a){
 if(a==G_CAREER_RACE)return (uint32_t)(uintptr_t)event_record;
 if(a==(uint32_t)(uintptr_t)event_record+0x30)return native_prize;
 if(a==0x85ad40+0x7af8)return 0;
 if(a==0x85ad40+0x73c0)return has_contract?(uint32_t)(uintptr_t)contract:0;
 return *bank_pointer(a);
}
#define MR8(a) (*(uint8_t*)(uintptr_t)(a))
#define MR16(a) (*(uint16_t*)(uintptr_t)(a))
static int ok(uint32_t a,unsigned n){return (a==(uint32_t)(uintptr_t)event_record&&n<=sizeof event_record)||(a==(uint32_t)(uintptr_t)contract&&n<=sizeof contract);}
static void cstr(uint32_t a,char *out,size_t n){if(a!=G_PROFILE_NAME)abort();snprintf(out,n,"%s",profile_name);}
static void roam_plat_lock(void){}
static void roam_plat_unlock(void){}
#define rlog(...) ((void)0)
static struct {int bound,host;uint32_t peer;cc_race control;} NC;
static rc_session g_s;
static rw_reward NR_REWARD;
static int nc_same_world(void){return NC.bound;}
#include "native_reward.inc"
#define CHECK(x) do{if(!(x)){printf("FAIL %d %s\n",__LINE__,#x);return 1;}}while(0)
int main(void){
 CPU cpu={0};g_s.conf.self_id=10;NC.bound=1;NC.host=1;NC.peer=20;NC.control.run=1;NC.control.event=90;NC.control.phase=CC_LOADING;
 bank=1000;native_prize=700;CHECK(!nr_reward_scope(0x85ad40,90));CHECK(nr_reward_frame(&cpu,0)==0);CHECK(NR_REWARD.amount==700);
 native_prize=900;CHECK(nr_reward_frame(&cpu,10000)==0&&NR_REWARD.amount==700);
 rc_coop_msg ack={RC_COOP_PRIZE_ACK,10,1,90,700};CHECK(rw_receive(&NR_REWARD,20,&ack));CHECK(nr_reward_frame(&cpu,20000)==1);
 CHECK(nr_reward_scope(0x85ad40,90));CHECK(!nr_reward_scope(0x85ad40,91));CHECK(!nr_reward_scope(0x85ad44,90));
 bank-=50; /* private purchase/expense */
 for(int i=0;i<100;i++)CHECK(nr_reward_result(1));CHECK(bank==1650);
 bank+=17;CHECK(nr_reward_result(1)&&bank==1667); /* unrelated income is retained */
 strcpy(profile_name,"AnotherPlayer");CHECK(!nr_reward_scope(0x85ad40,90));strcpy(profile_name,"PlayerOne");
 nr_reward_solo_start();CHECK(!nr_reward_scope(0x85ad40,90));CHECK(!NR_REWARD.initialized);
 /* Guest uses the host's amount, regardless of its local sponsor quote. */
 NC.host=0;NC.peer=10;g_s.conf.self_id=20;NC.control.run=2;bank=20000;native_prize=1500;
 CHECK(nr_reward_frame(&cpu,30000)==0);rc_coop_msg prize={RC_COOP_PRIZE,20,2,90,700};CHECK(rw_receive(&NR_REWARD,10,&prize));
 CHECK(nr_reward_frame(&cpu,40000)==1);CHECK(nr_reward_result(1)&&bank==20700);
 nr_reward_solo_start();NC.control.run=3;CHECK(nr_reward_frame(&cpu,50000)==0);prize.run=3;CHECK(rw_receive(&NR_REWARD,10,&prize));
 CHECK(nr_reward_result(0)&&bank==20700);CHECK(nr_reward_result(0)&&bank==20700);
 nr_reward_solo_start();NC.host=1;g_s.conf.self_id=10;NC.peer=20;NC.control.run=4;has_contract=1;contract[1]=1300;event_record[0x34]=1;
 CHECK(nr_reward_frame(&cpu,60000)==0&&NR_REWARD.amount==1300);
 puts("PASS actual reward adapter: host quote frozen, guest sponsor ignored, independent balances, once-only credits, unrelated income retained, correct profile, solo reset, and no team-loss payout");return 0;
}
