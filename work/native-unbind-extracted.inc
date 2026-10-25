static void nc_unbind(CPU *c){
    int restore_world=nc_same_world();NC.bound=0;
    nm_reset();nr_reward_solo_start();nv_race_release(c);
    if(restore_world)for(int k=0;k<NC.stream.racers;k++)if(NC.car[k]&&k!=(NC.host?0:1)&&ok(NC.car[k],0x9e0)){
        body_set(c,NC.car[k],0);gcall1(c,FN_CAR_SETPHYS,NC.car[k],NC.old_phys[k]);body_set(c,NC.car[k],NC.old_phys[k]!=0&&rd32(NC.car[k]+0x28)!=0);
    }
    NC.bound=0;NC.loaded_noted=0;NC.result_applied=0;NC.next_send=0;NC.seq=0;
    memset(NC.car,0,sizeof NC.car);memset(NC.brain,0,sizeof NC.brain);memset(NC.record,0,sizeof NC.record);memset(NC.slot,0,sizeof NC.slot);
}