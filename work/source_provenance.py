"""Conservative identity of production sources and their offline checks."""
from pathlib import Path
import hashlib

ROOT = Path(__file__).resolve().parent

def source_identity():
    files = set()
    for folder in (ROOT/'nfsu2-web/runtime', ROOT/'nfsu2-web/win/roam_asi', ROOT/'nfsu2-web/tests'):
        files.update(p for p in folder.rglob('*') if p.suffix in {'.c', '.h', '.inc'})
    for pattern in ('*.py', '*.c', '*.h', '*.inc', '*.tsv'):
        files.update(ROOT.glob(pattern))
    files.update((ROOT/'coop-launcher.ps1', ROOT/'COOP-README-RU.txt'))
    digests = {p.relative_to(ROOT).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
               for p in sorted(files)}
    payload = ''.join(f'{name}\0{digest}\n' for name, digest in digests.items()).encode()
    return hashlib.sha256(payload).hexdigest()
