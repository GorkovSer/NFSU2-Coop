static int nw_pending_basic_controller(uint32_t brain){
 /* Native785428:+68 is41e7c0, which immediately dereferences +6fc.
  * A controller created for an inactive pool car has no route goal yet. */
 return ok(brain,0x79c)&&rd32(brain)==0x785428u&&!rd32(brain+0x6fc);
}
int nw_brain_owned(uint32_t brain){
 if(nw_pending_basic_controller(brain))return 1;
 if(!nw_same())return 0;
 for(int i=0;i<RC_TRAFFIC_CARS;i++)if(NW.replica[i].car&&ok(NW.replica[i].car,0x9e0)&&rd32(NW.replica[i].car+0x2c)==brain)return 1;
 /* The complete pool is ours, including unassigned/inactive cars. Those
  * controllers are not present in the host-to-replica mapping. */
 uint32_t pool[RC_TRAFFIC_CARS];unsigned n=nw_pool(pool);
 for(unsigned i=0;i<n;i++)if(rd32(pool[i]+0x2c)==brain)return 1;
 return 0;
}