from pathlib import Path
import runpy,sys,json
root=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion')
canonical=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp')
sources=sorted(p.relative_to(root).as_posix() for p in (root/'src/SDKs/Packages/Lion/Final').rglob('*.cpp'))
stage=sys.argv[1]
if len(sys.argv)>2:
 sources=[x for x in json.loads(Path(sys.argv[2]).read_text())["sources"] if x.startswith("src/")]
args=['tools/build_x360.py','--sdk',str(canonical/'build/x360/sdk-6534.4/portable/XDK'),'--out',str(canonical/'build/x360/parallel/native_lion'/stage),'--jobs','2','--timeout','180']
for source in sources: args += ['--source',source]
for relative in ['vendor/EABase/include/Common','vendor/EASTL/include','vendor/EAThread/include']: args += ['--compiler-arg=/I'+str(canonical/relative)]
sys.argv=args
runpy.run_path(str(root/'tools/build_x360.py'),run_name='__main__')
