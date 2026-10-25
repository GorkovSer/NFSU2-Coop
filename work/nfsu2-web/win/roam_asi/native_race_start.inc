/* 499ae0 prepares Career and queues menu-only actions8/15. A network peer
 * accepts from city state4, whose native handler ignores both actions.
 * Reuse state6's normal action15 tail: enter flow0, request action16. This
 * schedules the engine's loader; never destroy a live World from our hook. */
static void nc_engine_start(CPU *c){
 rh_call(c,(uint32_t)(uintptr_t)nc_original_start,0,0,0,0);
 uint32_t flow=rd32(0x8900d8),event=rd32(G_CAREER_RACE);
 if(!ok(flow,0x50)||rd32(flow)!=4||!ok(event,0x88)||MR32(event+8)!=NC.control.event)return;
 gcall1(c,0x603630,flow,0);
 uint32_t request[2]={16,0};rh_call(c,0x601020,flow,0,2,request);
 rlog("[coop] city acceptance: native deferred race loader requested\n");
}
