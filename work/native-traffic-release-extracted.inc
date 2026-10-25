static void nw_release(CPU *c){
 /* Deactivate replicas through the game, so its manager may safely respawn
  * them with original traffic brains after disconnect or a missing model. */
 /* Mapping rebuilds can fail halfway through. Withdraw the complete native
  * pool before releasing resources; the partially rebuilt map is insufficient. */
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_same()?nw_pool(pool):0;
 nw_withdraw_pool(c,pool,n);
 if(nw_same())for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0))body_set(c,NW.replica[i].car,0);
 /* Restore rides before deactivation replaces the controller identity used
  * by nv_reset's attachment guard, then withdraw any replacement body. */
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW_MODELS[i].started)nv_reset(c,&NW_MODELS[i]);
 if(nw_same()){
  for(unsigned i=0;i<n;i++)nw_park_replica(c,pool[i]);
  for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0)){
   int found=0;for(unsigned k=0;k<n;k++)if(pool[k]==NW.replica[i].car)found=1;
   if(!found)nw_park_replica(c,NW.replica[i].car);
  }
 }
 if(NW.owned)rlog("[traffic] shared roster released; ordinary traffic resumes\n");
 NW.owned=0;memset(NW.replica,0,sizeof NW.replica);
 ni_forget_pending();
}