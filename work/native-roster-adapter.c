/* Real AI adapter with the established synthetic vehicle backend. */
#define ROSTER_ADAPTER
#define main previous_vehicle_adapter_main
#include "vehicle-adapter-offline.c"
#undef main
static int local_cfg_car(uint32_t car,uint8_t *out,int cap,char *name){
 CHECK(car==addr(replica));strcpy(name,remote_cfg.type_name);return rc_carcfg_encode(&remote_cfg,out,cap);
}
#include "native_race_roster.inc"
static void prepare_ai(unsigned run,int host){
 setup(run);NC.host=host;NC.world=world();NC.stream.racers=3;NC.car[2]=addr(replica);NC.slot[2].car=addr(replica);
 g_s.conf.race_roster=1;g_s.peer[0].coop=g_s.peer[0].race_roster=1;
 const uint8_t original_cfg[]={9,8,7};g_s.cfg_len=3;memcpy(g_s.cfg,original_cfg,3);
}
int main(void){
 CHECK(previous_vehicle_adapter_main()==0);CPU cpu={0};
 prepare_ai(10,1);prepare(addr(replica_record)+0x10,&remote_cfg,2);MR32(addr(replica)+0xc)=addr(types)+0x890;
 CHECK(nai_frame(&cpu,1000)==0&&g_s.roster.sealed&&!g_s.roster.peer_ack);
 CHECK(g_s.roster.car[0].complete&&g_s.roster.car[0].length>0);
 ar_roster frozen=g_s.roster;
 MR8(addr(replica_record)+0x10+0x340)^=1;CHECK(nai_frame(&cpu,300000)==-1);
 nai_release(&cpu);CHECK(!NAI.active&&!g_s.roster.active);
 prepare_ai(11,0);CHECK(nai_frame(&cpu,1000)==0&&g_s.roster.active&&!g_s.roster.manifest_seen);
 frozen.self=20;frozen.peer=10;frozen.run=11;frozen.peer_ack=0;frozen.next_control=frozen.next_fragment=0;
 CHECK(ar_control(&g_s.roster,20,AR_MANIFEST,10,11,90,frozen.manifest));
 for(int64_t t=1;!g_s.roster.car[0].complete;t+=100000){ar_fragment f;CHECK(ar_next_fragment(&frozen,t,&f));CHECK(ar_receive(&g_s.roster,20,&f)==1);}
 CHECK(!ar_ready(&g_s.roster));int before=scoped;
 CHECK(nai_frame(&cpu,2000)==0&&NAI_MODELS[2].started&&!NAI_MODELS[2].applied);
 done=1;CHECK(nai_frame(&cpu,3000)==1&&NAI_MODELS[2].applied&&scoped==before+1);
 CHECK(g_s.roster.car[0].loaded&&ar_ready(&g_s.roster));
 CHECK(nv_matches_cfg(&remote_cfg,rd32(addr(replica)+0x10)));CHECK(NC.slot[2].ghost&&NC.slot[2].com[0]>.299f&&NC.slot[2].com[0]<.301f);
 CHECK(g_s.cfg_len==3&&g_s.cfg[0]==9&&g_s.cfg[1]==8&&g_s.cfg[2]==7);
 MRF32(rd32(addr(replica)+0x10)+0x46c)+=1;CHECK(nai_frame(&cpu,4000)==-1);
 nai_release(&cpu);CHECK(!NAI_MODELS[2].started&&!g_s.roster.active&&rd32(rd32(addr(replica)+0xc)+0xd0)==100);
 CHECK(!lock_depth);
 printf("PASS native AI roster adapter: %u combined checks; host freeze/mutation, separate human CFG, async native attach scope, skin/COM refresh, physical proof, late corruption and rollback (mock engine only)\n",checks);return 0;
}
