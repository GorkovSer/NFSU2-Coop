"""Build an isolated, exact-EXE experimental free-roam adapter. Never edits the game."""
from pathlib import Path
import sys, hashlib, re, subprocess, json, zlib, struct
from source_provenance import source_identity
source_sha256=source_identity()
ROOT=Path(__file__).resolve().parent
sys.path.insert(0,str(ROOT/'python-libs'))
import pefile
REPO=ROOT/'nfsu2-web'
WORLD='--next' in sys.argv[2:]
LAB='--lab' in sys.argv[2:]
assert not LAB or WORLD,'--lab requires --next'
COOP='--coop' in sys.argv[2:] or WORLD
OUT=ROOT/('native-lab-build' if LAB else 'native-world-build' if WORLD else 'native-coop-build' if COOP else 'native-build'); OUT.mkdir(exist_ok=True)
EXE=Path(sys.argv[1]); raw=EXE.read_bytes()
HASH='fa940fe56c8af619a62e6c9cba5c552a01832424a7ee2189af9814d3c531a38f'
assert hashlib.sha256(raw).hexdigest()==HASH, 'Different EXE: do not use this adapter'
pe=pefile.PE(data=raw); image=pe.get_memory_mapped_image(); base=pe.OPTIONAL_HEADER.ImageBase
assert base==0x400000 and base+pe.OPTIONAL_HEADER.AddressOfEntryPoint==0x75bc01
def data(va,n):return image[va-base:va-base+n]
assert data(0x5813d5,5)==bytes.fromhex('e8f60ee8ff')
assert data(0x4022d0,1)==b'\xc3'
# Recovered from this EXE: CreateCar's allocation; matching function signatures;
# native LAN car decoder at 0x5f05b2 / 0x5f05e9; encounter-manager signature.
remap={'FN_MALLOC':0x575570,'FN_ROADTRACK':0x5da320,'FN_BODY_DISABLE':0x593060,
       'FN_PERF_SET':0x5ac410,'FN_PHYS_UPDATE':0x5b4f60,'FN_EM_OVER':0x500c90}
src=(REPO/'runtime/roam_game.c').read_text()
for name,va in remap.items():
    src,n=re.subn(r'(#define\s+'+name+r'\s+)0x[0-9a-fA-F]+u',lambda m:m[1]+f'0x{va:08x}u',src)
    assert n==1,name
# This first prototype uses one of the original three parked opponent cars only.
# No extra cars, HUD cloning, AI races, or experimental collision behavior.
src=src.replace('O.max_cars = env_int("NFSU2_ROAM_MAX_CARS", MAX_CARS, 0, MAX_CARS);','O.max_cars = env_int("NFSU2_ROAM_MAX_CARS", 1, 0, 1);')
src=src.replace('O.test_owncars = env_int("NFSU2_ROAM_TEST_OWNCARS", 0, 0, 1);','O.test_owncars = 0;')
src=src.replace('O.collide = env_int("NFSU2_ROAM_COLLIDE", 0, 0, 1);','O.collide = 0;')
if COOP:
    src=src.replace('/* ---- game addresses (v1.2 NA)', '#include "native_coop.h"\nstatic void nc_frame(CPU *,int64_t,int);\n\n/* ---- game addresses (v1.2 NA)',1)
    src=src.replace('cf.discovery = disc && !O.test;', 'cf.coop = nc_enabled();\n    cf.discovery = disc && !O.test;',1)
    # The co-op frame must follow free-roam cleanup; it owns different cars.
    # The insertion below is anchored on the actual frame function ending.
    start=src.index('void roam_game_frame(CPU *c)')
    end=src.index('\n}',start)
    src=src[:end]+'\n    nc_frame(c, host, in_roam);\n'+src[end:]
    src+='\n#include "native_coop.inc"\n'
if WORLD:
    src='#include "native_contact_impulse.h"\n'+src
    pos=src.index('static int build_ride(')
    src=src[:pos]+'#include "native_perf_flags.inc"\n#include "native_rim_resources.inc"\n'+src[pos:]
    target='        gcall1(c, FN_PERF_SET, ride + 0x2f0u, perf);'
    assert src.count(target)==1
    src=src.replace(target,'        nv_clear_perf_flags(ride);\n'+target,1)
    if LAB:
        src=src.replace(target,target+'\n        nv_lab_flags("decoded",ride,pf);',1)
        src=src.replace('    gcall0(c, FN_RIDE_PARTSON, ride);','    gcall0(c, FN_RIDE_PARTSON, ride);\n    nv_lab_flags("assembled",ride,cc->perf);',1)
    src=src.replace('#include "native_coop.h"','#include "native_coop.h"\n#include "native_world.h"\nstatic void nw_frame(CPU *,int64_t);',1)
    src=src.replace('#include "native_world.h"','#include "native_world.h"\nint nr_outruns_enabled(void);\nstatic int nr_take_engage(void);\nstatic int nc_busy(void);\nstatic int nc_pause_held(void);\nstatic void nw_pause_resume(int64_t);\nstatic int nv_cfg_pinned(void);',1)
    # Share the actual equipment serializer, while preserving the human API.
    cfg_signature='static int local_cfg(uint8_t *out, int cap, char *tname){\n    uint32_t car = player_car(), ride = car ? rd32(car + 0x10u) : 0'
    assert src.count(cfg_signature)==1
    src=src.replace(cfg_signature,'static int local_cfg_car(uint32_t car,uint8_t *out,int cap,char *tname){\n    uint32_t ride = car ? rd32(car + 0x10u) : 0',1)
    pos=src.index('/* ---- building a remote ride')
    src=src[:pos]+'static int local_cfg(uint8_t *out,int cap,char *name){return local_cfg_car(player_car(),out,cap,name);}\n\n'+src[pos:]
    src=src.replace('static int nv_cfg_pinned(void);','static int nv_cfg_pinned(void);\nstatic void nai_release(CPU *);',1)
    # Slot is declared later than the forward declarations above.
    contact_decls='int np_contacts_enabled(void);\nstatic int np_prepare_pose(CPU *,Slot *,const rc_pose *);\nstatic void np_update_body(CPU *,Slot *);\nstatic int npi_contact_ready(uint32_t,int64_t);\nstatic int npi_sync_proxy(CPU *,uint32_t);\nstatic void npi_frame(CPU *,int64_t);\n'
    contact_decls+='static void nv_free_setride(CPU *,uint32_t,uint32_t);\nstatic void flush_unloads(CPU *);\nstatic void ntc_frame(void);\nstatic int nc_pause_raw(int);\nstatic int ng_city_suspended(void);\nstatic int nw_frame_allowed(void);\n'
    source_anchor='    uint32_t a3[3] = { ride, 0, 0 };'
    assert src.count(source_anchor)==1
    src=src.replace(source_anchor,'''    if(!nfl_dependency(k,nrim_ready(c,ride),roam_plat_now_us()))return 0;
    if(rd32(G_FLOW)!=6||world()!=s->world||s->car==player_car()||!ok(s->car,0x9e0))return 0;
    s->native_load_car=s->car;s->native_load_record=rd32(s->car+0x14);s->native_load_brain=rd32(s->car+0x2c);
'''+source_anchor,1)
    begin_anchor='static int begin_load(CPU *c, int k, const uint8_t *cfg, int len, uint32_t gen){\n    Slot *s = &g_sl[k];'
    assert src.count(begin_anchor)==1
    src=src.replace(begin_anchor,'#include "native_free_load_guard.inc"\n'+begin_anchor+'\n    if(s->native_quarantined||!nfl_attempt(k,world(),s->peer_id,gen,roam_plat_now_us()))return 0;',1)
    src=src.replace('    int      hidden;', '    int native_quarantined;uint32_t native_load_car,native_load_record,native_load_brain;\n    int      hidden;',1)
    finish_anchor='static int finish_load(CPU *c, int k){\n    Slot *s = &g_sl[k];'
    assert src.count(finish_anchor)==1
    src=src.replace(finish_anchor,finish_anchor+'''
    if(s->native_quarantined)return 0;
    uint32_t load_car=s->native_load_car,load_world=s->world,load_record=s->native_load_record,load_brain=s->native_load_brain;
    if(rd32(G_FLOW)!=6||world()!=load_world||s->car!=load_car||load_car==player_car()||!ok(load_car,0x9e0)||
       rd32(load_car+0x14)!=load_record||rd32(load_car+0x2c)!=load_brain){s->native_quarantined=1;return 0;}
''',1)
    wait_anchor='    gcall1(c, FN_LOADER_WAIT, G_CARLOADER, s->h_new);'
    assert src.count(wait_anchor)==1
    identity_check='''
    if(rd32(G_FLOW)!=6||world()!=load_world||s->car!=load_car||load_car==player_car()||
       !ok(load_car,0x9e0)||rd32(load_car+0x14)!=load_record||rd32(load_car+0x2c)!=load_brain){
        s->native_quarantined=1;return 0;
    }
'''
    src=src.replace(wait_anchor,identity_check+wait_anchor+identity_check+'''
    e=loader_entry(s->h_new);
    done=e?gcall1(c,FN_LOADER_LOADED,G_CARLOADER,e):0;
'''+identity_check+'    if(!done)return 0;\n',1)
    ready_anchor='    uint32_t done = e ? gcall1(c, FN_LOADER_LOADED, G_CARLOADER, e) : 0;'
    ready_pos=src.index(ready_anchor,src.index('static int finish_load('))
    src=src[:ready_pos]+src[ready_pos:].replace(ready_anchor,ready_anchor+identity_check,1)
    activation='    if (!s->activated){ gcall0(c, FN_CAR_ACTIVATE, car); s->activated = 1; }'
    assert src.count(activation)==1
    # Activate legitimately creates a fresh controller. Car/record/world
    # ownership must survive; the new controller is then the expected one.
    src=src.replace(activation,activation+identity_check.replace('||rd32(load_car+0x2c)!=load_brain','')+'    load_brain=s->native_load_brain=rd32(load_car+0x2c);\n',1)
    src=src.replace('        int was = s->st;', '        int was = s->st;\n        if(s->native_quarantined){tried[best]=1;continue;}',1)
    # A quarantined slot retains both handles/buffers until world teardown.
    for signature in ['static void release(CPU *c, int k, const char *why){','static void restore(CPU *c, int k){']:
        src=src.replace(signature,signature+'\n    if(g_sl[k].native_quarantined)return;',1)
    src=src.replace('if (s->world==world() && s->car!=player_car()', 'if (!s->native_quarantined && s->world==world() && s->car!=player_car()',1)
    src=src.replace('            if (!ok(s->car, 0x9e0)) continue;', '            if (s->native_quarantined||!ok(s->car, 0x9e0)) continue;',1)
    target='    gcall1(c, FN_CAR_SETRIDE, car, s->ride_new);'
    assert src.count(target)==1
    src=src.replace(target,'    nv_free_setride(c,car,s->ride_new);'+identity_check,1)
    pos=src.index('static void make_inert(CPU *c, uint32_t car){')
    src=src[:pos]+contact_decls+src[pos:]
    src=src.replace('if (rd32(car + 0x28u) != 0) gcall1(c, FN_CAR_SETPHYS, car, 0);',
                    'if (!np_contacts_enabled() && rd32(car + 0x28u) != 0) gcall1(c, FN_CAR_SETPHYS, car, 0);',1)
    src=src.replace('body_update(c, k);','/* Native contact policy runs in apply_pose. */',1)
    # The upstream park path assumed every proxy was already mode zero.
    # Full-physics contacts invalidate that assumption: withdraw the body
    # before its off-map move, then let the ordinary pose path rebuild it.
    park_anchor='static void soft_park(CPU *c, uint32_t car){'
    assert src.count(park_anchor)==1
    src=src.replace(park_anchor,park_anchor+'\n    body_set(c,car,0);\n    if(rd32(car+0x28))gcall1(c,FN_CAR_SETPHYS,car,0);',1)
    src=src.replace('if (O.collide){','if (np_contacts_enabled() && p->player_contact){',1)
    src=src.replace('#define DUELS           0','#define DUELS           (nr_outruns_enabled())',1)
    src=src.replace('return in_roam && world_runs && pl && ok(race, 0xc44)',
                    'return in_roam && world_runs && !nc_busy() && pl && ok(race, 0xc44)',1)
    src=src.replace('int press = eng && !g_du.engage;',
                    'int native_press=nr_take_engage();\n    int press = (eng && !g_du.engage) || (native_press && in_roam && world_runs);',1)
    src=src.replace('gcall0(c, FN_OUTRUN_START, race);',
                    'MR8(race+0xc30u)=0; /* never leave a queued solo start behind */\n    gcall0(c, FN_OUTRUN_START, race);',1)
    src=src.replace('if (rd32(car + 0x24u) != 0) gcall1(c, FN_CAR_SETDRIVER, car, 0);','uint32_t driver=nr_outruns_enabled()?3u:0u;\n    if (rd32(car + 0x24u) != driver) gcall1(c, FN_CAR_SETDRIVER, car, driver);',1)
    for name,va in [('FN_HUD_OUTRUN',0x4db860),('FN_ENGAGE_DONE',0x5010d0)]:
        src=re.sub(r'(#define\s+'+name+r'\s+)0x[0-9a-fA-F]+u',lambda m:m[1]+f'0x{va:08x}u',src)
    src=src.replace('cf.coop = nc_enabled();','cf.coop = nc_enabled();\n    cf.traffic = nw_enabled();\n    cf.world_host = nw_host();',1)
    src=src.replace('cf.traffic = nw_enabled();','cf.vehicles = cf.coop;\n    cf.vehicle_proof = cf.coop;\n    cf.rewards = cf.coop;\n    cf.traffic = nw_enabled();\n    cf.traffic_contact = cf.traffic && env_int("NFSU2_ROAM_TRAFFIC_FEEDBACK",0,0,1);',1)
    src=src.replace('cf.vehicle_proof = cf.coop;','cf.vehicle_proof = cf.coop;\n    cf.race_roster = cf.coop;\n    cf.player_contact = np_contacts_enabled();\n    cf.race_pause = cf.coop && env_int("NFSU2_ROAM_RACE_PAUSE",0,0,1) && env_int("NFSU2_ROAM_NETTHREAD",1,0,1);',1)
    src=src.replace('cf.traffic_contact = cf.traffic && env_int("NFSU2_ROAM_TRAFFIC_FEEDBACK",0,0,1);',
                    'cf.traffic_contact = cf.traffic && env_int("NFSU2_ROAM_TRAFFIC_FEEDBACK",0,0,1) && ni_hooks_install();',1)
    src=src.replace('cf.player_contact = np_contacts_enabled();','cf.player_contact = np_contacts_enabled() && ni_hooks_install();',1)
    src=src.replace('cf.world_host = nw_host();','cf.world_host = nw_host();\n    cf.extensions=(cf.traffic_contact?RC_EXT_TRAFFIC_IMPULSE:0)|(cf.player_contact?RC_EXT_PLAYER_IMPULSE:0);\n    if(cf.coop&&cf.world_host==env_int("NFSU2_ROAM_COOP_HOST",0,0,1))cf.extensions|=RC_EXT_RACE_MODES;\n    if((cf.player_contact||(cf.extensions&RC_EXT_RACE_MODES))&&cf.world_host)cf.extensions|=RC_EXT_CONTACT_HOST;\n    cf.extension_nonce=cf.extensions?roam_plat_random():0;\n    if(cf.extensions&&!cf.extension_nonce)cf.extension_nonce=1;',1)
    src=src.replace('if (cfgn > 0){','if (cfgn > 0 && !nv_cfg_pinned()){',1)
    src=src.replace('static void flush_unloads(CPU *c){','static void flush_unloads(CPU *c){\n    if(ng_city_suspended())return;',1)
    src=src.replace('nc_frame(c, host, in_roam);','flush_unloads(c);\n    nc_frame(c, host, in_roam);\n    npi_frame(c,host);\n    if(nw_frame_allowed())nw_frame(c, host);\n    flush_unloads(c);\n    ntc_frame();',1)
    # Traffic replicas reuse the pose writer but keep their native physics.
    # In particular, mode zero plus an enabled collision body is invalid.
    src=src.replace('static void apply_pose(CPU *c, Slot *s, const rc_pose *P, float dt){',
                    'static int apply_pose_mode(CPU *c, Slot *s, const rc_pose *P, float dt,int retain_physics){',1)
    src=src.replace('if (rd32(car + 0x28u) != 0){ gcall1', 'if (!retain_physics && rd32(car + 0x28u) != 0){ gcall1',1)
    # New traffic feedback uses exact pair-scoped impulses. Keep legacy
    # source available for offline comparison, but never call its heuristic.
    traffic=(REPO/'win/roam_asi/native_world.inc').read_text()
    assert traffic.count('#include "native_traffic_contact.inc"')==1
    traffic=traffic.replace('#include "native_traffic_contact.inc"','#include "native_contact_math.inc"',1)
    clear_legacy='memset(NWC.sample,0,sizeof NWC.sample);memset(NWC.pending,0,sizeof NWC.pending);'
    assert traffic.count(clear_legacy)==2
    traffic=traffic.replace(clear_legacy,'ni_forget_pending();')
    for old,new in [('nw_host_contacts(now);','ni_frame(c,now);'),
                    ('nw_capture_contact(car,NW.client.car[i].id,index,now);','/* Exact contact hook captures native pair impulses. */'),
                    ('nw_contact_sample(car,NW.client.car[i].id,index,&pose,now);','/* No proximity/velocity heuristic. */'),
                    ('nw_send_contacts(now);','ni_frame(c,now);')]:
        assert traffic.count(old)==1,old
        traffic=traffic.replace(old,new,1)
    (OUT/'native_world_next.inc').write_text(traffic)
    src+='\nstatic void ni_frame(CPU *,int64_t);\nstatic void ni_forget_pending(void);\n#include "native_world_next.inc"\n#include "native_menu_gate.inc"\n#include "native_texture_cache.inc"\n#include "native_contact_impulse.inc"\n#include "native_player_impulse.inc"\n'
    # SECTION may swap the physics interface/body at a streaming boundary.
    # Synchronize its final body, not the one that existed before SECTION.
    state_anchor='    wheels_pose(s, P, dt);'
    assert src.count(state_anchor)==1
    src=src.replace(state_anchor,'    int synchronized=!retain_physics||npi_sync_proxy(c,car);\n    if(!synchronized)body_set(c,car,0);\n'+state_anchor,1)
    assert src.count('    s->pose_frames++;')==1
    src=src.replace('    s->pose_frames++;','    s->pose_frames++;\n    return synchronized;',1)
    # Exact 57a555 indexes all three Cars-pool sizes by track/game mode.
    # The original adapter enlarged index 0 only, leaving race index 1 at
    # 9000 KB (0x8ca000 bytes, confirmed in the 2026-10-27 loader dump).
    pool_begin=src.index('    /* the free-roam car pool:')
    pool_end=src.index('    const char *pl =',pool_begin)
    src=src[:pool_begin]+'''    /* Native allocation consumes this table at the next track load;
     * never resize a live pool or change the neighboring track heap. */
    for(unsigned mode=0;mode<3;mode++){
        uint32_t address=G_CARPOOL_KB+4*mode,previous=rd32(address);
        if(O.carpool_kb&&previous&&previous<(uint32_t)O.carpool_kb){
            MR32(address)=(uint32_t)O.carpool_kb;
            rlog("[roam] car memory pool mode %u: %u -> %d KB at next track load\\n",mode,previous,O.carpool_kb);
        }
    }
'''+src[pool_end:]
    src=src.replace('#include "native_coop.inc"','#include "native_outrun.inc"\n#include "native_outrun_input.inc"\n#include "native_vehicle_verify.inc"\n#include "native_vehicle_load.inc"\n#include "native_race_vehicle.inc"\n#include "native_reward_state.inc"\n#include "native_coop_next.inc"',1)
    # Keep the tested 0.2 coordinator source intact. The next adapter adds a
    # car/config barrier before reporting the native race ready to the peer.
    coop=(REPO/'win/roam_asi/native_coop.inc').read_text()
    coop=coop.replace('int nc_state(uint32_t race,uint32_t state){',
                      '#include "native_race_cinematic.inc"\nint nc_state(uint32_t race,uint32_t state){\n    if(ncp_hold(race,state))return 1;',1)
    coop=coop.replace('||MR32(race)!=3)return 0;', '||!ncp_bind_ready(race))return 0;',1)
    # A held early intro can precede complete native roster allocation.
    # Keep waiting under the existing loading timeout rather than cancelling.
    bind_begin=coop.index('static int nc_bind(void){')
    bind_end=coop.index('static int nc_read_car(',bind_begin)
    coop=coop[:bind_begin]+coop[bind_begin:bind_end].replace('return -1;', 'return MR32(race)<3?0:-1;')+coop[bind_end:]
    if LAB:
        coop='static int NATIVE_LAB_READY,NATIVE_LAB_CANCEL;\n'+coop
        coop=coop.replace('int ready=nc_ready_key(),cancel=nc_cancel_key();',
                          'int ready=nc_ready_key()||NATIVE_LAB_READY,cancel=nc_cancel_key()||NATIVE_LAB_CANCEL;NATIVE_LAB_READY=NATIVE_LAB_CANCEL=0;',1)
    coop='static uint32_t nes_available(uint32_t);\nstatic uint32_t nes_local_available(uint32_t);\nstatic void nes_frame(CPU *,int64_t);\n#include "native_event_policy.inc"\n'+coop
    coop=coop.replace('static int nc_bind(void){',
                      'static void nc_grid_capture(void);\nstatic int nc_grid_frame(CPU *);\nstatic int ni_ready(uint32_t);\nstatic int nc_bind(void){',1)
    coop=coop.replace('    cs_init(&NC.stream,','    nc_grid_capture();\n    cs_init(&NC.stream,',1)
    coop=coop.replace('static void nc_frame(CPU *c,int64_t now,int in_roam){',
                      '#include "native_race_start.inc"\nstatic void nc_frame(CPU *c,int64_t now,int in_roam){',1)
    assert '#include "native_race_start.inc"' in coop
    start_call='rh_call(c,(uint32_t)(uintptr_t)nc_original_start,0,0,0,0);'
    assert coop.count(start_call)==1
    coop=coop.replace(start_call,'nc_engine_start(c);',1)
    coop=coop.replace('MR8(e+0xf)==0','nc_event_supported(e)',1)
    src=src.replace('cf.extensions|=RC_EXT_RACE_MODES;', 'cf.extensions|=RC_EXT_RACE_MODES|RC_EXT_RANK_TABLE|RC_EXT_RANK_CLOCK|RC_EXT_PAUSE_CLOCK;',1)
    coop,n=re.subn(r'static uint32_t nc_available\(uint32_t hash\)\{.*?\n\}',
                       '#include "native_event_available.inc"',coop,count=1,flags=re.S)
    assert n==1,'event availability adapter'
    coop=coop.replace('} NC;','} NC;\nstatic int nc_vehicle_frame(CPU *,int64_t);',1)
    coop=coop.replace('static int nc_vehicle_frame(CPU *,int64_t);','static int nc_vehicle_frame(CPU *,int64_t);\nstatic int nai_frame(CPU *,int64_t);',1)
    coop=coop.replace('static int nai_frame(CPU *,int64_t);','static int nai_frame(CPU *,int64_t);\nstatic void nc_names_frame(void);\nstatic void nc_positions_frame(void);',1)
    coop=coop.replace('static int nc_vehicle_frame(CPU *,int64_t);',
                      'static int nc_vehicle_frame(CPU *,int64_t);\nstatic int nc_pause_frame(int64_t);\nstatic int nc_pause_accept_frame(const rc_race_frame *,int64_t);',1)
    coop=coop.replace('    if(NC.bound&&nc_running()){','    int pause_held=nc_pause_frame(now);\n    if(NC.bound&&nc_running()){',1)
    coop=coop.replace('if(got&&incoming.run==NC.control.run&&incoming.event==NC.control.event)',
                      'if(got&&!pause_held&&incoming.run==NC.control.run&&incoming.event==NC.control.event&&nc_pause_accept_frame(&incoming,now))',1)
    coop=coop.replace('if(NC.control.phase==CC_RUNNING&&cs_stale(&NC.stream,now))',
                      'if(NC.control.phase==CC_RUNNING&&!pause_held&&cs_stale(&NC.stream,now))',1)
    coop=coop.replace('if(now>=NC.next_send){','if(!pause_held&&now>=NC.next_send){',1)
    coop=coop.replace('    if(NC.phase_logged!=NC.control.phase)',
                      '    nc_pause_frame(now);\n    if(NC.phase_logged!=NC.control.phase)',1)
    coop=coop.replace('static int nc_running(void){','static int nc_busy(void){return NC.enabled&&NC.control.phase>=CC_INVITED&&NC.control.phase<=CC_RUNNING;}\nstatic int nc_running(void){',1)
    coop=coop.replace('if(ready&&!NC.host&&NC.control.phase==CC_INVITED&&in_roam&&NC.clock_valid)',
                      'if(ready&&!NC.host&&NC.control.phase==CC_INVITED&&in_roam&&NC.clock_valid&&g_du.st==DU_IDLE)',1)
    coop=coop.replace('int32_t clock_offset;','int32_t clock_offset,peer_rtt;',1)
    coop=coop.replace('NC.clock_offset=p->clock_offset_us;','NC.clock_offset=p->clock_offset_us;NC.peer_rtt=p->clock_rtt_us;',1)
    coop=coop.replace('NC.slot[k].car=car;','NC.slot[k].car=car;NC.slot[k].peer_id=NC.peer;NC.slot[k].ghost=1;',1)
    coop=coop.replace('body_set(c,NC.car[k],1);gcall1(c,FN_CAR_SETPHYS,NC.car[k],NC.old_phys[k]);',
                      'body_set(c,NC.car[k],0);gcall1(c,FN_CAR_SETPHYS,NC.car[k],NC.old_phys[k]);body_set(c,NC.car[k],NC.old_phys[k]!=0&&rd32(NC.car[k]+0x28)!=0);',1)
    coop=coop.replace('if(cs_pose(&NC.stream,k,now,dt,&pose))',
                      'int64_t delay=100000;\n                if(np_contacts_enabled()&&np_source_live(NC.peer,now)){int32_t half=NC.peer_rtt>0?NC.peer_rtt/2:0;delay=-(half<150000?half:150000);}\n                if(!pause_held&&cs_pose_delay(&NC.stream,k,now,dt,delay,&pose))',1)
    # The contact adapter follows the coordinator, whose live race scope it reads.
    coop=coop.replace('static int nc_vehicle_frame(CPU *,int64_t);','static int nc_vehicle_frame(CPU *,int64_t);\nstatic int np_source_live(uint32_t,int64_t);',1)
    coop=coop.replace('static void nc_unbind(CPU *c){','static void nc_unbind(CPU *c){\n    memset(&NCP,0,sizeof NCP);\n    int restore_world=nc_same_world();NC.bound=0;\n    nr_reward_solo_start();nv_race_release(c);',1)
    coop=coop.replace('if(nc_same_world())for(int k=0;k<NC.stream.racers;',
                      'if(restore_world)for(int k=0;k<NC.stream.racers;',1)
    coop=coop.replace('NC.bound=0;}','nv_race_release(c);NC.bound=0;}',1)
    coop=coop.replace('if(!nc_enabled())return;','if(!nc_enabled())return;\n    if(NV_RACE.started&&world()!=NV_RACE.world){nv_race_release(c);NC.bound=0;cc_cancel(&NC.control,CC_ENGINE_ERROR,now);}',1)
    coop=coop.replace('if(!NC.enabled||!NC.connected||!roaming())return 0;','if(!NC.enabled||!NC.connected||!roaming())return 0;\n    nv_publish_local();',1)
    coop=coop.replace('    nv_publish_local();','    if(g_du.st!=DU_IDLE){rlog("[coop] finish or cancel the current player Outrun before starting a career race\\n");return 1;}\n    nv_publish_local();',1)
    coop=coop.replace('||!p->coop)continue;','||!p->coop||!p->vehicles||!p->vehicle_proof||!p->rewards||!p->race_roster)continue;',1)
    coop=coop.replace('if(m.op==RC_COOP_GO&&!NC.clock_valid)continue;','if(m.op==RC_COOP_PRIZE||m.op==RC_COOP_PRIZE_ACK){rw_receive(&NR_REWARD,from,&m);continue;}\n        if(m.op==RC_COOP_VEHICLE||m.op==RC_COOP_VEHICLE_ACK||m.op==RC_COOP_VEHICLE_PHYSICS){vs_receive(&NV_BARRIER,from,&m);continue;}\n        if(m.op==RC_COOP_GO&&!NC.clock_valid)continue;',1)
    coop=coop.replace('if(!event||!in_roam)cc_cancel(&NC.control,CC_ENGINE_ERROR,now);','if(!event||!in_roam||!nv_freeze_race(c,NC.peer,NC.control.run,NC.control.event,now))cc_cancel(&NC.control,CC_ENGINE_ERROR,now);',1)
    coop=coop.replace('if(NC.control.phase==CC_CANCELLED&&NC.bound)nc_unbind(c);','if(NC.control.phase==CC_CANCELLED&&(NC.bound||NV_FROZEN.valid||NV_RACE.started||NV_BARRIER.initialized))nc_unbind(c);',1)
    coop=coop.replace('cc_accept(&NC.control,nc_available(NC.control.event)?NC.control.event:0,now);','{uint32_t nonce=0;if(nm_peer(NC.peer,now,&nonce)){nv_publish_local();cc_accept(&NC.control,nc_available(NC.control.event)?NC.control.event:0,now);}else rlog("[coop] guest cannot accept: update BOTH players to the same start/pause protocol\\n");}',1)
    coop=coop.replace('if(NC.control.phase==CC_LOADING&&NC.stream.seq_init&&!NC.loaded_noted)',
                      'int vehicle_ready=nc_vehicle_frame(c,now),reward_ready=nr_reward_frame(c,now);\n            int roster_ready=nai_frame(c,now);\n            if(vehicle_ready<0||reward_ready<0||roster_ready<0)cc_cancel(&NC.control,CC_ENGINE_ERROR,now);\n            if(NC.control.phase==CC_LOADING&&vehicle_ready>0&&reward_ready>0&&roster_ready>0&&NC.stream.seq_init&&!NC.loaded_noted)',1)
    coop=coop.replace('NC.result_applied=1;','if(!nr_reward_result(NC.control.winner!=0))return;\n    NC.result_applied=1;',1)
    coop=coop.replace('if(actions&CC_RESULT)nc_apply_result(c);','if((actions&CC_RESULT)||(NC.control.phase==CC_DONE&&!NC.result_applied))nc_apply_result(c);',1)
    coop=coop.replace('} NC;','} NC;\nstatic int nm_frame(CPU *,int64_t);\nstatic int nm_apply_result(CPU *);\nstatic int nm_result_ready(void);\nstatic int nm_between(CPU *,int64_t);\nstatic int nm_prepare_event(uint32_t,uint32_t);\nstatic void nm_reset(void);\nstatic int nm_mode(uint32_t);\nstatic int nm_peer(uint32_t,int64_t,uint32_t *);',1)
    coop=coop.replace('    if(!nc_enabled())return;', '    if(!nc_enabled())return;\n    if(nm_between(c,now))return;',1)
    coop=coop.replace('    if(!nc_enabled())return;', '    if(!nc_enabled())return;\n    nes_frame(c,now);\n    nv_owner_cleanup(c);',1)
    coop=coop.replace('if(rd32(rd32(other[j])+0x14)<rd32(rd32(other[i])+0x14))', 'if(nm_mode(rd32(G_CAREER_RACE))==RM_URL?MR16(rd32(other[j])+0xa)<MR16(rd32(other[i])+0xa):rd32(rd32(other[j])+0x14)<rd32(rd32(other[i])+0x14))',1)
    coop=coop.replace('    nr_reward_solo_start();nv_race_release(c);','    nm_reset();nr_reward_solo_start();nv_race_release(c);',1)
    coop=coop.replace('NC.pending_event=hash;return 1;', 'if(nm_mode(e)){uint32_t nonce=0;if(!nm_peer(NC.peer,roam_plat_now_us(),&nonce)){rlog("[race-mode] peer has no compatible result protocol\\n");return 1;}}\n    NC.pending_event=hash;return 1;',1)
    coop=coop.replace('int roster_ready=nai_frame(c,now);','int roster_ready=nai_frame(c,now),mode_ready=nm_frame(c,now);',1)
    coop=coop.replace('int roster_ready=nai_frame(c,now),mode_ready=nm_frame(c,now);',
                      'int roster_ready=nai_frame(c,now),mode_ready=nm_frame(c,now);\n            if(vehicle_ready>0&&roster_ready>0&&NC.control.phase==CC_LOADING)vehicle_ready=nc_grid_frame(c);',1)
    # Downhill Drift has virtual leaderboard opponents, outside the physical
    # motion roster. Their ranks must never invalidate a two-car snapshot.
    coop=coop.replace('outgoing.car[i].position=outgoing.car[i].completion?MR8(NC.record[k]+0xc):0;',
                      'outgoing.car[i].position=outgoing.car[i].completion?MR8(NC.record[k]+0xc):0;\n                    if(outgoing.car[i].position>outgoing.racers)outgoing.car[i].position=0;',1)
    coop=coop.replace('!nv_freeze_race(c,NC.peer,NC.control.run,NC.control.event,now))', '!nv_freeze_race(c,NC.peer,NC.control.run,NC.control.event,now)||!nm_prepare_event(event,NC.control.run))',1)
    coop=coop.replace('            if(vehicle_ready<0', '            if(!NC.bound)return;\n            if(vehicle_ready<0',1)
    coop=coop.replace('vehicle_ready<0||reward_ready<0||roster_ready<0','vehicle_ready<0||reward_ready<0||roster_ready<0||mode_ready<0',1)
    coop=coop.replace('if(vehicle_ready<0||reward_ready<0||roster_ready<0||mode_ready<0)cc_cancel(&NC.control,CC_ENGINE_ERROR,now);',
                      'if(vehicle_ready<0||reward_ready<0||roster_ready<0||mode_ready<0){rlog("[coop] barrier failed: vehicle=%d reward=%d roster=%d mode=%d\\n",vehicle_ready,reward_ready,roster_ready,mode_ready);cc_cancel(&NC.control,CC_ENGINE_ERROR,now);}',1)
    coop=coop.replace('reward_ready>0&&roster_ready>0&&NC.stream.seq_init','reward_ready>0&&roster_ready>0&&mode_ready>0&&NC.stream.seq_init',1)
    coop=coop.replace('if(NC.control.phase==CC_LOADING&&vehicle_ready>0&&reward_ready>0&&roster_ready>0&&mode_ready>0&&NC.stream.seq_init',
                      'if(NC.control.phase==CC_LOADING&&ncp_frame(c,vehicle_ready>0&&roster_ready>0)&&vehicle_ready>0&&reward_ready>0&&roster_ready>0&&mode_ready>0&&NC.stream.seq_init',1)
    intro_line=next(line for line in coop.splitlines(True) if 'if(NC.control.phase==CC_LOADING&&ncp_frame(' in line)
    coop=coop.replace(intro_line,'',1)
    intro_line=intro_line.replace('ncp_frame(c,vehicle_ready>0&&roster_ready>0)', 'ncp_frame(c,vehicle_ready>0&&roster_ready>0&&reward_ready>0&&mode_ready>0&&NC.stream.seq_init)')
    pose_call='apply_pose(c,&NC.slot[k],&pose,(float)dt/1000000.f);'
    assert coop.count(pose_call)==1
    coop=coop.replace(pose_call,'{uint32_t before=NC.slot[k].pose_frames;apply_pose(c,&NC.slot[k],&pose,(float)dt/1000000.f);if(NC.slot[k].pose_frames!=before)ncp_pose_ready((unsigned)k);}',1)
    send_anchor='            if(!pause_held&&now>=NC.next_send){'
    assert coop.count(send_anchor)==1
    coop=coop.replace(send_anchor,intro_line+send_anchor,1)
    coop=coop.replace('    if(!nr_reward_result(NC.control.winner!=0))return;', '    if(nm_mode(rd32(G_CAREER_RACE))){int mode_ready=nm_frame(c,roam_plat_now_us());if(mode_ready<=0||!nm_result_ready())return;}\n    if(!nr_reward_result(NC.control.winner!=0))return;',1)
    coop=coop.replace('    NC.result_applied=1;', '    int special=nm_apply_result(c);if(special<0)return;\n    NC.result_applied=1;if(special)return;',1)
    coop+='\n'+(REPO/'win/roam_asi/native_race_vehicle_frame.inc').read_text()
    coop+='\n'+(REPO/'win/roam_asi/native_reward.inc').read_text()
    coop+='\n#include "native_player_contact.inc"\n'
    coop+='\n#include "native_race_pause.inc"\n'
    coop+='\n#include "native_race_roster.inc"\n'
    coop+='\n#include "native_race_modes.inc"\n'
    coop=coop.replace('    if(NC.phase_logged!=NC.control.phase)', '    nc_names_frame();\n    nc_positions_frame();\n    if(NC.phase_logged!=NC.control.phase)',1)
    coop+='\n#include "native_race_names.inc"\n'
    coop+='\n#include "native_race_positions.inc"\n'
    coop+='\n#include "native_race_grid.inc"\n'
    coop+='\n#include "native_event_share.inc"\n'
    src='int nes_hooks_install(void);\n'+src
    src=src.replace('cf.extension_nonce=cf.extensions?roam_plat_random():0;', 'if(cf.coop&&nes_hooks_install())cf.extensions|=RC_EXT_CAREER_MAP;\n    cf.extension_nonce=cf.extensions?roam_plat_random():0;',1)
    coop=coop.replace('outgoing.car[i].position=outgoing.car[i].completion?MR8(NC.record[k]+0xc):0;',
                      'outgoing.car[i].position=outgoing.car[i].completion?MR8(NC.record[k]+0xc):NC.host?MR8(NC.record[k]+0xa):0;',1)
    coop,n=re.subn(r'static void nc_apply_result\(CPU \*c\)\{.*?\n\}',
                       '#include "native_race_result.inc"',coop,count=1,flags=re.S)
    assert n==1,'race result adapter'
    # Once the authoritative result is known, native finish/teardown owns the
    # cars. Do not rebuild retired physics bodies from the last network pose.
    anchor='if(!NC.enabled||!nc_running()||!nc_same_world())return 0;'
    assert coop.count(anchor)==2
    coop=coop.replace(anchor,'if(!NC.enabled||!nc_running()||NC.control.phase==CC_DONE||!nc_same_world())return 0;')
    anchor='if(NC.bound&&nc_running()){'
    assert coop.count(anchor)==1
    coop=coop.replace(anchor,'if(NC.bound&&nc_running()&&NC.control.phase!=CC_DONE){',1)
    (OUT/'native_coop_next.inc').write_text(coop)
    src=src.replace('static int build_ride(CPU *c,','#include "native_vehicle.inc"\nstatic int nv_build_ride_raw(CPU *c,',1)
    begin=src.index('        /* the physics pass (')
    finish=src.index('*/',begin)+2
    src=src[:begin]+'        /* Final native physics calculation follows all equipment/appearance assembly. */'+src[finish:]
    src=src.replace('gcall1(c, FN_RIDE_STOCK, ride, 0);','gcall1(c, FN_RIDE_STOCK, ride, 0);\n    for(int i=0;i<4;i++)MR8(ride+4u+(uint32_t)i)=cc->ride_b[i];',1)
    pos=src.index('/* ---- paint slots past')
    wrapper='''static int build_ride(CPU *c,const rc_carcfg *cfg,uint32_t ride,uint32_t skin,char *why,int whylen){
    int result=nv_build_ride_raw(c,cfg,ride,skin,why,whylen);
    if(result<0)return result;
    if(!nv_physics(c,ride)){snprintf(why,(size_t)whylen,"invalid native physics curve");return -1;}
    return result;
}

'''
    src=src[:pos]+wrapper+src[pos:]
    # Definition must precede all existing callers.
    pos=src.index('\n/* ---- names over friends')
    src=src[:pos]+'\nstatic int nc_name_scope(void);\nstatic Slot *nc_name_slot(int);\n'+src[pos:]
    src=src.replace('O.labels && g_in_roam && MR8(G_SHOW_NAMES)', 'O.labels && (g_in_roam||nc_name_scope()) && MR8(G_SHOW_NAMES)',1)
    src=src.replace('Slot *s = &g_sl[k];\n        if (s->st != SL_ACTIVE', 'Slot *s = nc_name_scope()?nc_name_slot(k):&g_sl[k];\n        if (!s || (!nc_name_scope() && s->st != SL_ACTIVE)',1)
    src=src.replace('const Slot *s = &g_sl[t[i].k];', 'const Slot *s = nc_name_scope()?nc_name_slot(t[i].k):&g_sl[t[i].k];',1)
    pos=src.index('\n/* ---- names over friends')
    src=src[:pos]+'\nstatic void apply_pose(CPU *c,Slot *s,const rc_pose *p,float dt){\n    int physical=np_prepare_pose(c,s,p);\n    apply_pose_mode(c,s,p,dt,physical);\n    np_update_body(c,s);\n    memcpy(s->drawn,p->p,sizeof s->drawn);\n}\n'+src[pos:]
if LAB:src+='\n#include "native_lab_race.inc"\n'
(OUT/'roam_game.c').write_text(src)
asi=(REPO/'win/roam_asi/roam_asi.c').read_text()
asi=asi.replace('0x00581475u','0x005813d5u').replace('0x004022C0u','0x004022D0u')
asi=asi.replace('"roam_fp.h"','"native_fp.h"')
if COOP:
    asi=asi.replace('{ "NFSU2_ROAM_LOG",', '{ "NFSU2_ROAM_COOP", "Coop", "0" },\n    { "NFSU2_ROAM_COOP_HOST", "CoopHost", "0" },\n    { "NFSU2_ROAM_LOG",',1)
if WORLD:
    asi='#include "native_crash.h"\n'+asi
    # The call wrapper precedes the frame-fault guard's original declarations.
    asi=asi.replace('static volatile LONG g_guard;                     /* inside the frame step */\nstatic DWORD  g_guard_tid;','/* Frame guard declarations moved before the native call wrapper. */',1)
    asi=asi.replace('uint32_t roam_win_call(uint32_t fn,', 'static volatile LONG g_guard;\nstatic DWORD g_guard_tid;\nuint32_t roam_win_call(uint32_t fn,',1)
    trace_before='    uint16_t sw0, sw1;'
    assert asi.count(trace_before)==1
    asi=asi.replace(trace_before,trace_before+'\n    if(g_guard&&GetCurrentThreadId()==g_guard_tid)ncr_native_call(fn,ecx,(DWORD)g_frame,0);',1)
    trace_after='    g_x87_popped += (uint32_t)k;'
    assert asi.count(trace_after)==1
    asi=asi.replace(trace_after,trace_after+'\n    if(g_guard&&GetCurrentThreadId()==g_guard_tid)ncr_native_call(fn,ecx,(DWORD)g_frame,1);',1)
    anchor='    if (!exe_check()){ roam_log("[roam] free-roam multiplayer stays OFF; the game runs as without the plugin\\n"); return; }'
    assert asi.count(anchor)==1
    asi=asi.replace(anchor,anchor+'\n    ncr_install(g_self);',1)
    boot_anchor='/* ---- the hook '
    assert asi.count(boot_anchor)==1
    asi=asi.replace(boot_anchor,'#include "native_quickstart.inc"\n\n'+boot_anchor,1)
    asi=asi.replace('void roam_asi_frame(void);','static void nq_frame(void);\nvoid roam_asi_frame(void);',1)
    asi=asi.replace('    ncr_install(g_self);','    ncr_install(g_self);\n    nq_init();',1)
    asi=asi.replace('    roam_win_poll();','    nq_frame();\n    roam_win_poll();',1)
    asi=asi.replace('    g_guard = 0;\n    g_fault_code = code;', '    g_guard = 0;\n    ncr_report(ep,"guarded multiplayer frame fault");\n    g_fault_code = code;',1)
    asi=asi.replace('    if (g_dead || !g_ready) return;', '    if (g_dead || !g_ready) return;\n    ncr_prepare();',1)
    asi=asi.replace('    } else if (reason == DLL_PROCESS_DETACH){','    } else if (reason == DLL_PROCESS_DETACH){\n        ncr_remove();',1)
    asi=asi.replace('{ "NFSU2_ROAM_LOG",','{ "NFSU2_ROAM_RACE_PAUSE", "RacePause", "0" },\n    { "NFSU2_ROAM_LOG",',1)
    asi=asi.replace('{ "NFSU2_ROAM_LOG",','{ "NFSU2_ROAM_PLAYER_CONTACT", "PlayerContact", "0" },\n    { "NFSU2_ROAM_LOG",',1)
    asi=asi.replace('{ "NFSU2_ROAM_LOG",','{ "NFSU2_ROAM_COOP_RACE_MODES", "CoopRaceModes", "1" },\n    { "NFSU2_ROAM_OUTRUNS", "Outruns", "0" },\n    { "NFSU2_ROAM_NATIVE_SELFTEST", "NativeSelfTest", "0" },\n    { "NFSU2_ROAM_TRAFFIC", "TrafficSync", "0" },\n    { "NFSU2_ROAM_TRAFFIC_FEEDBACK", "TrafficFeedback", "0" },\n    { "NFSU2_ROAM_WORLD_HOST", "WorldHost", "0" },\n    { "NFSU2_ROAM_LOG",',1)
    asi='#include "native_world.h"\n'+asi
    asi=asi.replace('g_test_fault = opt_int(', 'if (opt_int("NFSU2_ROAM_TRAFFIC",0)||opt_int("NFSU2_ROAM_COOP",0)) roam_log("[traffic] early model pool 22 -> 96: %s\\n",nw_pool_prepare()?"verified":"unchanged (late or mismatch)");\n    g_test_fault = opt_int(',1)
asi=asi.replace('v1.2 (North America)','exact local EXE fa940fe5').replace('v1.2 North America', 'supported local EXE')
# The first 27 game calls in the upstream fingerprint are used by the native
# free-roam path; later HUD/extra-car calls are unreachable with the above cap.
fn=[]
up=(REPO/'win/roam_asi/roam_fp.h').read_text()
for m in re.finditer(r'\{\s*(0x[0-9A-F]+)u,\s*(\d+),\s*RF_FN\s*,\s*0x[0-9A-F]+u,\s*"([^"]+)"',up,re.I):
    name=m[3].split(' ')[0];va=remap.get(name,int(m[1],16));n=int(m[2]);fn.append((va,n,1,name))
# Protect each relevant global with actual instruction references in this EXE.
# Existing structure offsets remain a runtime verification item.
refs=[]
from capstone import Cs, CS_ARCH_X86, CS_MODE_32
from capstone.x86 import X86_OP_MEM, X86_OP_IMM
md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True
text=next(s for s in pe.sections if s.Name.startswith(b'.text'))
lo=base+text.VirtualAddress; code=data(lo,text.Misc_VirtualSize)
globals={name:int(v,16) for name,v in re.findall(r'#define\s+(G_\w+)\s+(0x[0-9a-fA-F]+)u',src[:src.index('/* ---- guest access')])}
for name,va in globals.items():
    target={0x863470:0x85ad40,0x828164:0x828154}.get(va,va)
    p=code.find(struct.pack('<I',target));found=None
    while p>=0 and found is None:
        pos=lo+p
        for start in range(pos-1,pos-8,-1):
            ins=next(md.disasm(data(start,16),start,1),None)
            if not ins or not(start<pos and pos+4<=start+ins.size):continue
            if any((op.type==X86_OP_MEM and op.mem.base==0 and (op.mem.disp&0xffffffff)==target) or
                   (op.type==X86_OP_IMM and (op.imm&0xffffffff)==target and ins.mnemonic in ('mov','push')) for op in ins.operands):
                found=(ins.address,ins.size,2,name);break
        p=code.find(struct.pack('<I',target),p+1)
    assert found is not None,'No reference for '+name
    refs.append(found)
checks=fn+refs+[(0x5813da,8,2,'frame hook continuation')]
if WORLD: checks += [(0x413470,32,1,'native RideInfo copy'),(0x4db860,32,1,'native outrun HUD'),(0x5010d0,24,1,'native Engage reset'),(0x607180,32,1,'native outrun start'),(0x5a63d0,32,1,'native upgrade setter (test only)')]
if WORLD: checks += [(0x532da0,32,1,'native current race prize'),(0x541380,32,1,'native career win (cash scope)')]
if WORLD: checks += [(0x42f840,32,1,'native traffic spawn selection'),(0x418270,32,1,'native observer traffic density'),(0x42e660,32,1,'native event traffic placement'),(0x42ea30,32,1,'native city traffic placement'),(0x4035b0,24,1,'native city traffic eligibility'),(0x5f0fb0,32,1,'native traffic observer position distance'),(0x5f12a0,32,1,'native traffic observer road distance')]
if WORLD: checks += [(0x417be0,32,1,'native traffic pool recycling eligibility')]
if WORLD: checks += [(0x5f1160,32,1,'native second-observer 3D distance')]
if WORLD: checks += [(0x527710,128,1,'native available event from road marker'),(0x4ffc40,64,1,'native road marker by event key'),(0x4cb070,55,1,'native map descriptor refresh'),(0x533260,128,1,'native city marker cache construction')]
if WORLD: checks += [(0x61b500,28,1,'native rim manufacturer attribute getter'),(0x494180,37,1,'native geometry file reference acquisition'),(0x48ce10,76,1,'native geometry resource lookup'),(0x4933b0,96,1,'native file pin reference and bounded name'),(0x633650,368,1,'native aftermarket wheel model constructor')]
if WORLD: checks += [(0x638190,213,1,'native CarRenderInfo three texture remap table layouts'),(0x490560,174,1,'native texture remap minus-one cache lookup and swap'),(0x488dd0,63,1,'native texture remap swap restoration')]
if WORLD: checks += [(0x429a00,64,1,'native traffic controller constructor'),(0x785518,108,2,'native traffic controller virtual methods'),(0x5f46a0,240,1,'native streamed-cell physics fallback policy')]
if WORLD: checks += [(0x4265c0,64,1,'native basic traffic controller constructor'),(0x785428,120,2,'native basic traffic controller virtual methods'),(0x429dd0,64,1,'native basic traffic controller update'),(0x5f6d50,196,1,'native Activate record kind3 selects controller4')]
if WORLD: checks += [(0x5f6e20,70,1,'native Deactivate replaces the controller with NullController')]
if WORLD: checks += [(0x41e7c0,11,1,'native basic-controller goal query requires nonnull goal')]
if WORLD: checks += [(0x5b52f0,32,1,'native installed ride physics pointer'),(0x413400,32,1,'native embedded curve copy/rebase')]
if WORLD: checks += [(0x639ce0,32,1,'native vehicle performance finalization'),(0x6399d0,32,1,'native AI automatic upgrade replacement')]
if WORLD: checks += [(0x500d70,16,1,'native Engage outrun eligibility')]
if WORLD: checks += [(0x593830,32,1,'native collision physics references'),(0x5939b0,32,1,'native collision RideInfo access')]
if WORLD: checks += [(0x5ea320,32,1,'native world pause query'),(0x601a60,32,1,'native simulation time calculation'),(0x58137d,39,1,'native paused world maintenance branch'),(0x58b183,22,1,'native pause flag setters')]
if WORLD: checks += [(0x601ae8,64,1,'native wall-clock anchor update'),(0x57a050,64,1,'native simulation clock advances only for positive dt')]
if WORLD: checks += [(0x5b25b0,48,1,'native paired contact wrapper before promotion/projection'),(0x5af060,48,1,'native paired contact solver'),(0x5a0a40,224,1,'native body impulse application'),(0x59bcc0,307,1,'native physical momentum impulse/clamps'),(0x5af8f2,43,1,'native paired impulse dispatch'),(0x593670,16,1,'native body derived-physics refresh')]
if WORLD: checks += [(0x5410c0,176,1,'native career attempt counter before series construction')]
if WORLD: checks += [(0x603630,40,1,'native flow transition queues exit enter and update'),(0x601020,71,1,'native flow action queue and dispatcher'),(0x606bfb,28,1,'native menu race-start tail requests deferred loader')]
if WORLD: checks += [(0x532240,707,1,'native career parameters from scoped event'),(0x526050,944,1,'native AI grid population and NPC descriptor stride'),(0x56da4b,200,1,'native downhill table scores and virtual entry label'),(0x802228,4,2,'native AI drift scoring enabled')]
if WORLD: checks += [(0x532df0,803,1,'native career series initialization and persistent participant IDs'),(0x5414e0,91,1,'native series results copy and stage advancement'),(0x54cbe0,480,1,'native race result builder'),(0x515d10,142,1,'native result rank stable ID completion and elapsed time'),(0x513f00,63,1,'native next-stage availability query')]
if WORLD: checks += [(0x528730,480,1,'native career outcome and drift leaderboard selection'),(0x56d670,91,1,'native drift terminal score flush'),(0x56dd30,304,1,'native live drift leaderboard refresh'),(0x7f7b8c,24,2,'native URL point schedule')]
if WORLD: checks += [(0x5938b0,64,1,'native pair collision notification and physics-link rebuild')]
if WORLD: checks += [(0x534420,536,1,'native existing profile deserializer for launcher quickstart'),(0x51bad0,80,1,'native frontend screen record lookup'),(0x5805d3,5,2,'startup SkipMovies assignment')]
if WORLD: checks += [(0x4af260,61,1,'CareerCrib native save-check and queued career entry'),(0x57a555,28,1,'native three-mode Cars-pool allocation table')]
if WORLD: checks += [(0x59b8f0,325,1,'native physical state from pose/velocity/angular velocity'),(0x5939b0,224,1,'native contact-class inertia scaling')]
header=['#include <stdint.h>','#define RF_ENTRY_VA 0x0075bc01u','enum { RF_FN = 1, RF_REF = 2 };',
 'static const struct { uint32_t va; uint16_t len; uint8_t kind; uint32_t crc; const char *what; } RF[] = {']
for va,n,kind,name in checks:header.append(f'{{0x{va:08x}u,{n},{kind},0x{zlib.crc32(data(va,n)):08x}u,"{name}"}},')
header.append('};')
(OUT/'native_fp.h').write_text('\n'.join(header))
(OUT/'roam_asi.c').write_text(asi)
cc=ROOT/'toolchain/llvm-mingw-20261006-msvcrt-x86_64/bin/i686-w64-mingw32-gcc.exe'
flags=['-std=c11','-O2','-march=i686','-msse2','-mfpmath=sse','-mstackrealign','-ffp-contract=off','-fno-strict-aliasing',
 '-ffreestanding','-fno-builtin','-fno-stack-protector','-fno-asynchronous-unwind-tables','-DROAM_HOST_WIN','-DROAM_ASI_VERSION="local-prototype-0.1"',
 '-I'+str(REPO/'win/roam_asi'),'-I'+str(REPO/'runtime'),'-I'+str(OUT)]
sources=[OUT/'roam_asi.c',OUT/'roam_game.c',REPO/'win/roam_asi/roam_win.c',REPO/'runtime/roam_core.c',REPO/'win/roam_asi/roam_libc.c']
if COOP:
    flags=[f.replace('local-prototype-0.1','career-experimental-0.2') for f in flags]
    sources += [REPO/'runtime/coop_race.c',REPO/'runtime/coop_stream.c',REPO/'win/roam_asi/native_coop_hooks.c']
if WORLD:
    flags=[f.replace('career-experimental-0.2','coop-0.5-alpha-garage-race-pool-20261028') for f in flags]
    flags += ['-DNATIVE_NEXT']
    sources += [REPO/'runtime/traffic_sync.c',REPO/'runtime/vehicle_sync.c',REPO/'runtime/reward_sync.c',REPO/'win/roam_asi/native_world_hooks.c',REPO/'win/roam_asi/native_event_hooks.c']
    sources += [REPO/'win/roam_asi/native_contact_hooks.c']
    sources += [REPO/'win/roam_asi/native_crash.c']
if LAB: flags += ['-DNATIVE_ENGINE_LAB']
objs=[]
for source in sources:
    obj=OUT/(source.stem+'.o');subprocess.run([str(cc),*flags,'-c',str(source),'-o',str(obj)],check=True);objs.append(str(obj))
link=['-shared','-nostdlib','-Wl,--subsystem,windows:5.1','-Wl,--no-insert-timestamp','-Wl,--dynamicbase','-Wl,--nxcompat',
 '-Wl,--image-base,0x10000000','-Wl,-e,_DllMain@12','-Wl,--disable-auto-import']
builtins=subprocess.check_output([str(cc),'--print-libgcc-file-name'],text=True).strip()
subprocess.run([str(cc),*link,*objs,'-lkernel32','-lws2_32',*(['-luser32'] if COOP else []),builtins,'-Wl,-Map,'+str(OUT/'NFSU2FreeRoam.map'),'-o',str(OUT/'NFSU2FreeRoam.asi')],check=True)
manifest={'status':'EXPERIMENTAL: not verified in a running game','exe_sha256':HASH,'max_remote_cars':1,'coop_races':False,
 'addresses':remap,'fingerprint_checks':len(checks),'plugin_sha256':hashlib.sha256((OUT/'NFSU2FreeRoam.asi').read_bytes()).hexdigest()}
if COOP: manifest.update(coop_races='experimental-engine-integration-not-playtested',default_enabled=False)
if WORLD: manifest.update(traffic_sync='experimental-native-adapter-not-engine-tested',traffic_model_policy='native-streamed-stock-models',traffic_default_enabled=False,player_vehicles='native-performance-and-race-load-barrier-not-race-tested')
if WORLD: manifest.update(outruns='experimental-proximity-Engage-adapter-default-off-not-engine-tested',race_modes='Circuit default; Sprint/StreetX/URL/Drag/all-Drift opt-in mask63; native adapters not engine tested')
if WORLD: manifest.update(rewards='separate-local-budgets-equal-host-agreed-prize-once-per-live-run-not-two-PC-tested',reward_capability_required=True)
if WORLD: manifest.update(version='0.5',release_channel='Alpha',build_version='coop-0.5-alpha-rims-20261022',rim_resources='native-manufacturer-geometry-header-prerequisite-before-free-roam-and-race-CarLoader-one-process-reference-per-brand-stock24-not-engine-tested',traffic_interest='two-observer-native-2D-and-3D-distance-scopes-and-shared-traffic-spawn-not-engine-tested',race_grid='guest-own-native-physical-state-at-captured-replaced-AI-grid-before-loaded-barrier-not-engine-tested',frontend_lifetime='no-proxy-parking-HUD-or-duel-native-actions-outside-live-flow-pin-attached-models-until-world-teardown-not-engine-tested',traffic_feedback='experimental-proximity-velocity-response-default-off-not-native-contact-callback-not-engine-tested',offline_validation='See work/OFFLINE-VALIDATION.md; this builder compiles but does not execute tests')
if WORLD: manifest.update(player_vehicles='pre-load-equipment-freeze-full-parts-and-native-physics-proof-not-engine-tested',vehicle_physics_proof_required=True)
if WORLD: manifest.update(player_vehicle_retuning='scoped-native-AI-auto-upgrade-bypass-during-network-car-attachment-only-not-tested')
if WORLD: manifest.update(background_free_roam='750ms-frame-watch-paused-HELLO-fresh-clock-resume-offline-validated-not-engine-tested; requires NetThread=1')
if WORLD: manifest.update(outruns='native-Engage-solo-start-interception-and-network-handshake-default-off-not-engine-tested')
if WORLD: manifest.update(player_contact='experimental-native-physics-retention-mutual-capability-and-live-source-guards-default-off-not-shared-impulse-authority-not-engine-tested')
if WORLD: manifest.update(race_pause='negotiated-network-frame-watch-and-two-phase-timed-resume-native-world-and-clock-gates-not-engine-tested',race_pause_default_enabled=False)
if WORLD: manifest.update(ai_roster='host-frozen-fragmented-equipment-and-native-physics-proof-barrier-not-engine-tested',ai_roster_capability_required=True)
if WORLD: manifest.update(traffic_feedback='exact-pair-scoped-native-impulse-and-arm-host-apply-ACK-retry-default-off-not-engine-tested',contact_hooks=2,extension_protocol='core4-message15-nonce-echo-lease-message16-native-traffic-impulse')
if WORLD: manifest.update(player_contact='host-only-native-pair-solver-physical-momentum-delta-ACK-retry-mutual-world-and-physics-barrier-default-off-not-engine-tested',player_contact_protocol='extension2-host-role-message18-world-echo-frame-lease-message17-player-momentum',retained_proxy_physics='native-state-setter-syncs-pose-and-momentum-not-engine-tested')
if WORLD: manifest.update(mode_results='extension4+16-message19-v2-176-byte-owner-finish-epoch-host-pause-ledger-frozen-board-ACK-native-career-outcome-guard' ,mode_result_hooks=5,url_series='native-stage-results-points-new-run-and-load-barrier-prize-once-final-not-engine-tested',downhill_drift='scoped-native-event-copy-two-real-cars-three-stock-virtual-score-opponents-not-engine-tested')
if WORLD: manifest.update(crash_reports='guarded-frame-and-unhandled-exception-text-registers-modules-minidump-localappdata-NFSU2-Coop-Crashes-best-effort-no-new-recovery')
if WORLD: manifest.update(traffic_controller_guard='pre-call-type-vtable-and-basic-goal-check; full-client-manager-and-entire-pool-controller-suppression; reported-696c6e4f-and-41e7c6-paths-refused-offline',proxy_physics_retry='retry-native-mode3-after-mode4-fallback-on-native-streamed-cell-ready; no-forced-cell-flag')
if WORLD: manifest.update(live_ranking='host-native-live-ranks-validated-permutation-guest-HUD-not-engine-tested',ai_finish_policy='both-human-terminal-close-live-bots-as-timeout-preserve-real-finishes-not-engine-tested',loader_lifecycle='retired-handles-drained-in-races-before-reusing-ride-buffers-1000-swap-mock-regression',human_recovery='loading-only-pre-race-human-vehicle-equipment-native-rebuild-and-resource-pin-not-engine-tested',resource_readiness='recheck-complete-native-dependency-group-after-wait-retain-live-handles-stock-traffic-material-isolation-not-engine-tested',traffic_visibility='fresh-written-pose-and-successful-native-physical-state-sync-before-body-enable-not-engine-tested',pause_clock_capability=32)
if WORLD: manifest.update(build_version='coop-0.5-alpha-menu-guard-20261022',texture_cache='live-world-CarRenderInfo-three-native-remap-caches-invalidated-between-frames-native-lookup-minus-one-sentinel-no-active-swap-writes-not-engine-tested',traffic_collision_gate='whole-pool-withdraw-before-rebuild-release-before-resource-reset-final-body-sync-after-section-native-cell-and-render-attach-required-not-engine-tested',shared_career_map='extension64-message20-nonce-serial-lease-catalog-union-local-map-and-unlocked-cross-stage-native-availability-and-cache-rebuild-runtime-marker-cache-only-no-save-unlock-writes-not-engine-tested')
if WORLD: manifest.update(city_menu_guard='suspend-shared-traffic-and-deferred-unloads-on-native-city-pause-or-zero/nonfinite-world-dt-race-loading-retained-not-engine-tested',native_lifetime_trace='cached-game-thread-before-after-resource-controller-calls-64-ring-and-first-observed-poisoned-render-pool-no-recovery')
assert source_identity()==source_sha256,'Sources changed during build'
if WORLD: manifest.update(build_version='coop-0.5-alpha-garage-race-pool-20261028',audit_fixes='three-mode native car-pool budget and client traffic body-identity contact gate; not gameplay verified',cinematic='defer ordinary NIS enter action1 and replay original once after models and poses ready; action3 waits until enter replay; not gameplay verified',quickstart='one-shot native CareerCrib save-check and queued career entry after loaded profile and menu readiness; not gameplay verified',known_open_issues=['far-host native map streaming','eStripSlotPool corrupting write not identified','resource lifetime crashes 4937f2 and 4939e5 remain unresolved','loader-pressure mitigation and invisible-traffic gate not gameplay verified','quickstart native menu transitions and profile selection not gameplay verified'])
manifest.update(source_sha256=source_sha256,lab=LAB)
(OUT/'build.json').write_text(json.dumps(manifest,indent=2))
print(json.dumps(manifest,indent=2))
