from contextlib import redirect_stdout
from io import StringIO
from pathlib import Path
import hashlib,json,struct,subprocess
d=Path(__file__).resolve().parent
wave=d.parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
base='fa84cd2a4423d5258a88eff385c5ad61e1707a88'
head='717bd5556277d1fe9394504ed09e31ce86e4ce42'
branch='codex/experimental-x360-r29-combined-preflight'
loading=d/'LOADING_INPUT.json'
patch=wave/'r29-root-preflight/r29-preflight.patch'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
assert git('rev-parse','HEAD')==head and git('rev-parse','HEAD^')==base
assert subprocess.check_output(['git','diff','--binary','--no-ext-diff',base,head],cwd=w)==patch.read_bytes()
proof=json.loads(loading.read_text())
assert proof['head']==head and all(sha(Path(p))==h for p,h in proof['generated'].items())
s=(d/'compile.py').read_text()
tail=s[s.index("report=d/'native/compile-results.json'"):]
old="assert any(str(generated).lower() in line.lower() and '.generated.inc' in line for line in Path(row['log']).read_text(errors='replace').splitlines())"
assert tail.count(old)==1
new="""traces=[Path(line.split('including file:',1)[1].strip()).resolve()
        for line in Path(row['log']).read_text(errors='replace').splitlines() if 'including file:' in line and '.generated.inc' in line]
assert traces==[(generated/'BrnLoadingScreenTextures.generated.inc').resolve()]
"""
exec(compile(tail.replace(old,new),str(d/'verify.py'),'exec'))
