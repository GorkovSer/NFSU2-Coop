"""Package only our patch. Never install it or launch a game."""
from pathlib import Path
import argparse,json,hashlib,zipfile
from source_provenance import source_identity
parser=argparse.ArgumentParser()
parser.add_argument('--name',default='NFSU2-Coop-0.5-Alpha')
parser.add_argument('--unverified',action='store_true',help='Explicit candidate for user testing; do not claim offline verification')
args=parser.parse_args()
if not args.name or Path(args.name).name!=args.name or '/' in args.name or '\\' in args.name:
    parser.error('name must be a single directory name')
root=Path(__file__).resolve().parent
build=root/'native-world-build'
manifest=json.loads((build/'build.json').read_text())
assert manifest.get('source_sha256')==source_identity(), 'Rebuild after source changes'
assert manifest.get('lab') is False, 'Production build required'
if args.unverified:
    manifest['offline_checks']=[]
    manifest['verification_pending']=True
    manifest['status']='UNVERIFIED CANDIDATE: user requested to perform testing themselves'
else:
    offline=json.loads((root/'offline-final-results.json').read_text())
    assert len(offline)==43 and len({r['name'] for r in offline})==43
    assert all(r['passed'] and r.get('source_sha256')==manifest['source_sha256'] for r in offline), 'Fresh full offline suite required'
    manifest['offline_checks']=[{'name':r['name'],'source_sha256':r['source_sha256'],'passed':r['passed']} for r in offline]
asi=(build/'NFSU2FreeRoam.asi').read_bytes()
assert hashlib.sha256(asi).hexdigest()==manifest['plugin_sha256']
assert all(marker not in asi for marker in [b'NativeLabRaceCommand',b'LabBoot.asi',b'real own-body momentum only']), 'Refusing laboratory binary'
out=root/'releases'/args.name
out.mkdir(parents=True,exist_ok=False)
(out/'payload').mkdir(exist_ok=True)
(out/'payload'/'NFSU2FreeRoam.asi').write_bytes(asi)
(out/'build.json').write_text(json.dumps(manifest,indent=2)+'\n')
# Windows PowerShell 5.1 needs a BOM to read Russian text correctly.
(out/'CoopLauncher.ps1').write_text((root/'coop-launcher.ps1').read_text(encoding='utf-8'),encoding='utf-8-sig')
(out/'START.cmd').write_bytes(b'@echo off\r\ncd /d "%~dp0"\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0CoopLauncher.ps1"\r\nif errorlevel 1 pause\r\n')
(out/'LICENSE.txt').write_bytes((root/'nfsu2-web'/'LICENSE').read_bytes())
readme=(root/'COOP-README-RU.txt').read_text(encoding='utf-8')
if args.unverified:
    readme=('КАНДИДАТ ДЛЯ ВАШЕЙ ПРОВЕРКИ. Последние изменения не тестировались по вашей просьбе.\n'
            'Не закрыты: загрузка дальних участков карты на хосте и точная причина повреждения eStripSlotPool.\n\n'+readme)
(out/'README.txt').write_text(readme,encoding='utf-8-sig')
files=sorted(p for p in out.rglob('*') if p.is_file())
hashes=''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(out).as_posix()+'\n' for p in files if p.name!='SHA256SUMS.txt')
(out/'SHA256SUMS.txt').write_text(hashes)
archive=out.with_name(out.name+'.zip')
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED) as z:
    for p in sorted(out.rglob('*')):
        if p.is_file():z.write(p,Path(out.name)/p.relative_to(out))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
expected={'build.json','CoopLauncher.ps1','START.cmd','LICENSE.txt','README.txt','SHA256SUMS.txt','payload/NFSU2FreeRoam.asi'}
assert {p.relative_to(out).as_posix() for p in out.rglob('*') if p.is_file()}==expected
print(json.dumps({'archive':str(archive),'bytes':archive.stat().st_size,'dll_sha256':manifest['plugin_sha256'],'installed':False,'game_launched':False},indent=2))
