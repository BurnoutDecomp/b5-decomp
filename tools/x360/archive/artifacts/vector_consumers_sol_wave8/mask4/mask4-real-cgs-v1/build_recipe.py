"""Frozen actual Mask4 SDK/header and retained Debug/Sound production consumers."""
from pathlib import Path
import hashlib,json,os,subprocess,sys
PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');WORK=Path('D:/codex-native/vector-consumers-w8');NOTES=Path(__file__).resolve().parent
LAYER=NOTES/'layers/mask4_wave8_v1';LAYER.mkdir(exist_ok=True)
tag=sys.argv[1]
OUT=Path('D:/Reverse/BP-X360-Experimental-Artifacts/vector_consumers_sol_wave8/mask4')/tag;assert not OUT.exists()
sources=['build/consumer-wave8/mask4.cpp',
         'src/GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugManager.cpp',
         'src/GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.cpp',
         'src/GameShared/GameClasses/Sound/CgsSoundUtils.cpp']
if len(sys.argv)>2:sources+=sys.argv[2:]
tracked=subprocess.check_output(['git','ls-files'],cwd=WORK).decode().splitlines()
extra=subprocess.check_output(['git','ls-files','--others','--exclude-standard'],cwd=WORK).decode().splitlines()
inputs=set(sources+[p for p in tracked+extra if p.endswith(('.h','.hpp','.inl')) and (WORK/p).is_file()])
inputs.update(subprocess.check_output(['git','diff','--name-only'],cwd=WORK).decode().splitlines())
frozen={p:hashlib.sha256((WORK/p).read_bytes()).hexdigest() for p in sorted(inputs) if (WORK/p).is_file()}
OUT.mkdir(parents=True);(OUT/'temp').mkdir()
(OUT/'source-inputs.json').write_text(json.dumps(frozen,indent=2)+'\n',encoding='utf8')
(OUT/'candidate.diff').write_bytes(subprocess.check_output(['git','diff','--binary'],cwd=WORK))
(OUT/'mask4.cpp').write_bytes((WORK/'build/consumer-wave8/mask4.cpp').read_bytes())
(OUT/'build_recipe.py').write_bytes(Path(__file__).read_bytes())
env=dict(os.environ);env['TEMP']=env['TMP']=str(OUT/'temp')
cmd=[sys.executable,'tools/build_x360.py','--sdk',str(PARENT/'b5-decomp/build/x360/sdk-6534.4/portable/XDK'),'--out',str(OUT),'--jobs','1','--timeout','180','--compiler-arg=/Z7']
for s in sources:cmd+=['--source',s]
for suffix in ['EABase/include/Common','EASTL/include','EAThread/include']:cmd+=['--compiler-arg=/I'+str(PARENT/'b5-decomp/vendor'/suffix)]
run=subprocess.run(cmd,cwd=WORK,env=env)
for p,h in frozen.items():assert hashlib.sha256((WORK/p).read_bytes()).hexdigest()==h,p
report=json.loads((OUT/'compile-results.json').read_text())
for row in report['results']:row['sha256']={k:hashlib.sha256(Path(row[k]).read_bytes()).hexdigest() for k in ['object','assembly','log'] if Path(row[k]).is_file()}
report.update(base=subprocess.check_output(['git','rev-parse','HEAD'],cwd=WORK).decode().strip(),source_inputs=frozen,
              recipe_snapshot=str(OUT/'build_recipe.py'),recipe_snapshot_sha256=hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              status='PRIVATE_GENUINE_MASK4_CORE_AND_RETAINED_CONSUMERS')
p=LAYER/(tag+'-bound.json');assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n',encoding='utf8')
sys.exit(run.returncode)
