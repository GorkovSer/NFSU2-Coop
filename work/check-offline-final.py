"""Rebuild/run only standalone synthetic harnesses; never start/install NFS."""
from pathlib import Path
import subprocess,json,time,argparse
from source_provenance import source_identity
source_sha256=source_identity()
parser=argparse.ArgumentParser();parser.add_argument('--only',action='append',default=[]);args=parser.parse_args();selected=set(args.only)
root=Path(__file__).resolve().parent
cc=root/'toolchain/llvm-mingw-20261006-msvcrt-x86_64/bin/i686-w64-mingw32-gcc.exe'
repo=root/'nfsu2-web';rt=repo/'runtime';native=repo/'win/roam_asi'
flags=['-std=gnu11','-O2','-mstackrealign','-ffp-contract=off','-fno-strict-aliasing','-I'+str(rt),'-I'+str(native)]
common=[rt/(x+'.c') for x in ['roam_core','coop_race','coop_stream','reward_sync','vehicle_sync','traffic_sync']]
results=[]
def check(name,extra=(),args=(),sources=None):
 if selected and name not in selected:return
 exe=root/('offline-'+name+'.exe')
 cmd=[str(cc),*flags,*extra,str(root/(name+'.c')),*map(str,common if sources is None else sources),'-o',str(exe)]
 p=subprocess.run(cmd,capture_output=True,text=True)
 if not p.returncode:p=subprocess.run([str(exe),*map(str,args)],capture_output=True,text=True,timeout=50)
 record={'name':name,'passed':p.returncode==0,'output':p.stdout+p.stderr,'source_sha256':source_sha256};results.append(record)
 print(json.dumps(record),flush=True)
for name in ['coop-tests','race-outcome-tests','race-clock-tests','url-series-offline','race-pause-tests','native-race-pause-offline','race-roster-tests','native-roster-adapter','vehicle-tests','vehicle-adapter-offline','reward-tests','reward-adapter-test','world-tests','frame-watch-test','race-smoothness-test','outrun-input-offline','outrun-goals-test','contact-impulse-tests','native-impulse-offline','player-impulse-tests','native-player-impulse-offline','player-contact-offline','native-mode-results-offline']:
 check(name)
check('event-policy-test',args=[root/'native-race-catalog.tsv'])
check('event-available-offline',sources=[])
check('race-start-offline',sources=[])
check('race-result-offline',sources=[])
check('race-names-offline',sources=[])
check('race-positions-offline',sources=[])
check('race-grid-offline',sources=[])
check('rim-resources-offline',sources=[])
check('texture-cache-offline',sources=[])
check('menu-gate-offline',sources=[])
check('event-catalog-tests')
check('event-share-offline')
check('event-hooks-test',extra=['-DEVENT_HOOK_TEST'],args=[root/'game-test-coop/SPEED2.EXE'],sources=[native/'native_event_hooks.c'])
check('race-close-ai-tests')
check('coop-hooks-test',extra=['-DNATIVE_NEXT','-DCOOP_HOOK_TEST'],args=[root/'game-test-coop/SPEED2.EXE',-2],sources=[native/'native_coop_hooks.c'])
check('world-hooks-test',extra=['-DWORLD_HOOK_TEST'],args=[root/'game-test-coop/SPEED2.EXE'],sources=[native/'native_world_hooks.c'])
check('contact-hooks-test',extra=['-DCONTACT_HOOK_TEST'],args=[0],sources=[native/'native_contact_hooks.c'])
if results and results[-1]['name']=='contact-hooks-test' and results[-1]['passed']:
 for arg in [1,-1]:
  p=subprocess.run([str(root/'offline-contact-hooks-test.exe'),str(arg)],capture_output=True,text=True,timeout=10)
  results.append({'name':'contact-hooks-'+str(arg),'passed':p.returncode==0,'output':p.stdout+p.stderr,'source_sha256':source_sha256});print(json.dumps(results[-1]),flush=True)
check('core-golden',sources=[repo/'tests/roam_selftest.c'])
if selected:
 assert selected<={r['name'] for r in results},'Unknown or missing selected check'
 previous=json.loads((root/'offline-final-results.json').read_text())
 replacements={r['name']:r for r in results}
 results=[replacements.pop(r['name'],r) for r in previous]+list(replacements.values())
assert source_identity()==source_sha256,'Sources changed while tests were running; rerun required'
(root/'offline-final-results.json').write_text(json.dumps(results,indent=2),encoding='utf-8')
raise SystemExit(0 if all(r['passed'] for r in results) else 1)
