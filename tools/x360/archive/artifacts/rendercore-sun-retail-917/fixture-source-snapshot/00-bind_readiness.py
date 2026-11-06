from pathlib import Path
import ast, datetime, hashlib, json, os, shutil, subprocess, sys
ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-sun-retail-917')
OUT=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-sun-retail-917')
ENV=dict(os.environ,GIT_NO_LAZY_FETCH='1')
def git(*args):return subprocess.check_output(['git',*args],cwd=ROOT,env=ENV).decode().strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def put(name,value):
 p=OUT/name
 assert not p.exists(),str(p)
 p.write_text(json.dumps(value,indent=2,sort_keys=True)+'\n',encoding='utf-8')
 return {'path':str(p),'sha256':sha(p)}
def pin(p):p=Path(p);return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
HEAD='2670fcfbddac32882b7dddf1649f5bc713de714f'
assert git('rev-parse','HEAD')==HEAD
assert not git('diff','--name-only') and not git('diff','--cached','--name-only')
base='917521de878436ca131a85c90685d011a00970aa'
particle='5e51d297ffdefb2ae2fc31b13c92e76053feb448'
owned=['src/GameSource/Graphics/BrnSunCorona.h','src/GameSource/Graphics/BrnSunCorona.cpp','src/GameSource/Effects/Particles/ParticleModule.h']
commit_order=[base,'ebc98699b74ce5f57435fe327e414d02279251c3','57e743308c2070fcd87f0884b7210d78f318e9d3','a543433f19245c820692665f76359b4d7029a4ba',particle,HEAD]
for previous,current in zip(commit_order,commit_order[1:]):assert git('rev-parse',current+'^')==previous
commits=[]
for c in commit_order[1:]:
 commits.append({'commit':c,'parent':git('rev-parse',c+'^'),'paths':git('diff-tree','--no-commit-id','--name-only','-r',c).splitlines(),'trailer':git('show','-s','--format=%B',c).splitlines()[-1]})
# Conservative full tracked source/header snapshot. It is deliberately broader than
# the selected compiler include closure; scratch probes are pinned separately.
source_files={}
for rel in git('ls-files').splitlines():
 p=ROOT/rel
 if p.is_file() and p.suffix.lower() in {'.h','.hpp','.inl','.inc','.cpp','.c','.cxx','.cc'}:
  source_files[rel]=sha(p)
source_manifest=put('source-header-snapshot.json',{'commit':HEAD,'scope':'All tracked C/C++ source and header inputs, plus exact untracked probes separately; conservative, not an assertion that every file was compiled.','files':source_files})
# Immutable Math receipt covers exactly the current owner manifest map.
math_manifest=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/vector_abi_sol/layers/v34_homes_v1/PRIVATE_CHECKPOINT.json')
math=json.loads(math_manifest.read_text())
assert all(sha(ROOT/p)==h for p,h in math['paths'].items())
assert len(math['paths'])==33
external_reference=math_manifest.parent/'external-toolchain-inputs-v1.json'
external=json.loads(external_reference.read_text())
external_errors=[p for p,h in external['files'].items() if not Path(p).is_file() or sha(p)!=h]
assert not external_errors,external_errors
external_bound=put('external-toolchain-inputs-bound.json',{'reference':pin(external_reference),'compiler_version':external['compiler_version'],'target':external['target'],'checked_files':len(external['files']),'mismatches':external_errors,'files':external['files']})
# Bind all native compiler artifacts and the one honest initial diagnostic failure.
compile_stages=[]
object_map={}
for folder in ['native1','renderer-callers1','descriptor-probe']:
 p=OUT/folder/'compile-results.json';r=json.loads(p.read_text());assert r['state']=='complete'
 rows=[]
 for row in r['results']:
  entry={'source':row['source'],'passed':row['passed'],'log':pin(row['log']),'command':row['command']}
  if row['passed']:
   entry['object']=pin(row['object']);entry['assembly']=pin(row['assembly']);object_map[row['source']]=entry['object']
  rows.append(entry)
 compile_stages.append({'name':folder,'report':pin(p),'results':rows})
assert sum(x['passed'] for x in compile_stages[0]['results'])==9
assert compile_stages[0]['results'][-1]['source']=='src/GameSource/Graphics/BrnRendererModule.cpp' and not compile_stages[0]['results'][-1]['passed']
assert all(x['passed'] for s in compile_stages[1:] for x in s['results'])
compile_bound=put('native-compiler-bound.json',{'stages':compile_stages,'effective_inputs':len(object_map),'effective_passed':len(object_map),'scope':'Nine actual product CPPs and two generated genuine-header ABI/body probes. Initial Renderer diagnostic remains immutable; approved Particle lane correction is the only source correction before its successful retry.','initial_renderer_particle_header_blob':git('rev-parse','a543433f19245c820692665f76359b4d7029a4ba:src/GameSource/Effects/Particles/ParticleModule.h'),'final_renderer_particle_header_blob':git('rev-parse',HEAD+':src/GameSource/Effects/Particles/ParticleModule.h'),'objects':object_map})
comparison_names=['sun-lifecycle-retail-comparison.json','sun-destruct-retail-comparison.json','sun-projection-retail-comparison.json','sun-draw-retail-comparison.json','projection-caller-retail-comparison.json','sun-draw-caller-retail-comparison.json','renderer-construct-to-sun-comparison.json','renderer-destruct-to-sun-comparison.json']
comparisons=[]
for name in comparison_names:
 p=OUT/'native1'/name;r=json.loads(p.read_text());assert r['errors']==[]
 n=r.get('cases')
 if n is None:
  checks=r['checks'];n=sum(checks.values()) if isinstance(checks,dict) else sum(x['cases'] for x in checks)
 comparisons.append({'report':pin(p),'cases':n,'errors':0,'scope':r['scope'],'limits':r['limits']})
assert sum(x['cases'] for x in comparisons)==2048
comparison_bound=put('retail-comparisons-bound.json',{'cases':2048,'errors':0,'reports':comparisons,'entry_contract':'Original and compiled Sun receive identical retail register state. No paired-GPR to native-vector adaptation remains. Compiled embedded binary addresses are still compared by exact blob identity, as explicitly disclosed in lifecycle reports; that is output relocation normalization, not input ABI adaptation.'})
# Freeze exact fixture sources including transitive local emulator modules.
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
fixture_sources=[]
for n,m in sorted(sys.modules.items()):
 p=getattr(m,'__file__',None)
 if p and ('/Reverse/' in p.replace('\\','/') or '/.codex/' in p.replace('\\','/')) and Path(p).suffix=='.py':
  fixture_sources.append({'module':n,**pin(Path(p).resolve())})
# native_atomic_fixture extracts this class via AST rather than import.
p=Path(F.__file__).parent/'validate_native_strings.py'
fixture_sources.append({'module':'AST-extracted StringCPU',**pin(p)})
snapshot=OUT/'fixture-source-snapshot';assert not snapshot.exists();snapshot.mkdir()
for index,item in enumerate(fixture_sources):
 dst=snapshot/(str(index).zfill(2)+'-'+Path(item['path']).name);shutil.copyfile(item['path'],dst);item['snapshot']=str(dst);assert sha(dst)==item['sha256']
raw=Path(F.M.V.x360rd.RAW)
aux=[F.M.OUT/'native-math.dll',F.M.OUT/'native-math.map',raw]
fixture_bound=put('fixture-inputs-bound.json',{'python':pin(sys.executable),'fixtures':fixture_sources,'auxiliary_inputs':[pin(p) for p in aux],'raw_original':pin(raw),'note':'Historical native-math DLL/map are imported by common emulator support. Fresh Sun/provider objects, not those historical method bodies, supply this checkpoint\'s tested native code. Raw ARTIST fallback supplies original methods and cache setters.'})
probes={str(p.relative_to(ROOT)):pin(p) for p in sorted((ROOT/'scratch/sun_retail_abi').glob('*.cpp'))}
scripts={p.name:pin(p) for p in sorted(OUT.glob('validate_*.py'))}
# Original caller and exact current symbol/register contracts are immutable receipts.
receipts={name:pin(OUT/name) for name in ['dependency-map.json','device-resolution-blob-map.json','foreign-setter-register-contracts.json','canonical-math-immutability.json','descriptor-probe-provenance.json','actual-renderer-sun-call-windows.json','original-caller-windows.json']}
postmortem=OUT/'POSTMORTEM.md';assert not postmortem.exists()
postmortem.write_text('''The Sun follow-up passes its private native ABI scope. It is held behind the canonical Math cohort and the later combined-source gate.\n\nThe source change has two parts: the approved Particle setter reads the same three scalar lanes through the real SDK mV member, and Sun constructs its two shader rows with the genuine four-float Vector4 constructor. Sun header, layout, embedded shaders, algorithms, descriptor/resource allocation, draw order and lifecycle are unchanged from the separately attested a6e6 source node. All 33 inputs in the current Math owner manifest remain identical.\n\nARTIST remains the behavior authority. Projection entry is r3=this, r4=independent Matrix44 value storage, v1=view and v2=sun direction. Flare entry is r3=this, r4=target, v1=colour, f1/f2=white/brightness and r7=bool. The old paired-GPR and r9-bool slots are poisoned. Both executions receive the same retail state. Typed emitted callers independently check these windows, including all 128 vector bits and the aligned 64-byte Matrix44 copy. The original actual Renderer windows are pinned for all five calls.\n\nThe complete Sun CPP, real program/descriptor/stride/sizer/resource providers and Device CPP compile natively. The whole Renderer CPP compiles after the approved Particle prerequisite. Exact emitted Construct/Destruct call blocks from that real Renderer object execute complete Sun lifecycle/provider paths; they do not represent execution of an entire Renderer algorithm. Visibility and Render source callers remain absent in 917, and genuine-header emitted probes plus raw original caller windows are used honestly for those boundaries.\n\nAll eight comparison reports have zero errors and total 2048 cases. Lifecycle compares five resource lanes, shader identity, constructor outputs and callback-mutated release receivers/device captures. Projection executes native vector math without service hooks. Draw checks compare all 80 vertex bytes, shader constant bank/dirty masks, memory and ordered calls. Linkage-only original cache setter thunks preserve attested argument registers; no source compatibility alias or cast was added.\n\nThe current 917 BlendMaterialState spelling and unused TextureState void* return remain explicit inherited API debt. Exact symbol strings and original callees are recorded. The foreign descriptor-binding source is tested only as an exact-body genuine-header probe; whole Shadow, Flush, low-level GPU state, target Begin/End, controlled SDK calls, allocator/assert services and physical GPU execution are outside the proof. Embedded relocated addresses are compared by exact binary content, and unspecified stack padding is not asserted. There is no strict whole-game link, boot, launch or complete Renderer/foreign-provider approval. The future published3534 plus Math union must rebuild and recheck emitted symbols and provider coverage.\n''',encoding='utf-8')
source_scope={p:{'sha256':sha(ROOT/p),'blob':git('rev-parse',HEAD+':'+p)} for p in owned}
old_sun=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-suncorona-a449/sun-checkpoint.json')
checkpoint={
 'status':'PRIVATE_RETAIL_ABI_PROVEN_COHORT_HELD',
 'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),
 'worktree':str(ROOT),'base':base,'head':HEAD,'git_common_dir':git('rev-parse','--git-common-dir'),
 'commit_order_for_this_private_tree':commit_order,'commits':commits,
 'integration_rule':'Do not replay private ancestry or already-published aliases. The source node a6e6 and two narrow follow-up source commits require the immutable Math cohort and a new combined-source gate. Published3534 already owns factory/Vertex prerequisites. This checkpoint gives no approval to incoming Math/global provider debt.',
 'dependency_map':json.loads((OUT/'dependency-map.json').read_text()),
 'source_scope':source_scope,'new_product_CPPs_in_this_followup':[],
 'math':{'commit':base,'manifest':pin(math_manifest),'paths_checked':33,'all_identical':True,'status':'Foreign private held canonical type dependency; global consumer closure is separate.'},
 'source_header_snapshot':source_manifest,'source_header_files':len(source_files),
 'external_toolchain':external_bound,'native_compiler':compile_bound,'comparisons':comparison_bound,
 'total_comparisons':2048,'comparison_errors':0,'effective_native_inputs':11,'actual_product_CPPs':9,'generated_probes':2,
 'fixtures':fixture_bound,'probes':probes,'validation_scripts':scripts,'receipts':receipts,'postmortem':pin(postmortem),
 'old_sun_receipt':pin(old_sun),
 'supersedes_only':'The old paired-GPR normalized entry proof for Sun projection/flare is superseded by this raw retail ABI proof. Original source, earlier non-ABI evidence and all old artifact hashes remain untouched.',
 'caller_coverage':{'actual_whole_Renderer_CPP':'compile PASS','actual_Construct_Destruct_call_blocks':128,'typed_projection_draw_callers':384,'direct_complete_Sun_methods':1536,'missing_source_callers':['BrnRendererModule::ComputeSunCoronaVisibility','BrnRendererModule::Render'],'original_caller_register_windows':pin(OUT/'original-caller-windows.json')},
 'foreign_provider_limits':json.loads((OUT/'foreign-setter-register-contracts.json').read_text()),
 'held':['Canonical Math global consumer cohort and source union gate','Whole Shadow/unknown VertexProgramState prefix and Flush source providers','Render/Visibility whole source algorithms','Physical GPU, strict whole-game link, boot and launch','GPU storage/globalbuffers/MSAA private drafts were not adopted'],
 'worktree_tracked_clean':True,
}
result=put('SUN_RETAIL_ABI_READINESS.json',checkpoint)
resume=OUT/'CHECKPOINT_RESUME.md';assert not resume.exists()
resume.write_text('''Private Sun retail ABI checkpoint: '''+HEAD+'''\n\nRead SUN_RETAIL_ABI_READINESS.json first. Preserve all original reports and source snapshots. The tracked checkout is clean; scratch contains two genuine-header probes. No build/test remains live or required by this bounded follow-up. New source commits are Particle lane spelling '''+particle+''' then Sun shader-row spelling '''+HEAD+'''; both use the actual Sol trailer. Root owns later cohort intake.\n\nThe dependency base is immutable private Math917. Only approved published factory and Vertex nodes were adopted, with exact original/published/local maps and a named Device blob-resolution receipt. Do not replay private ancestry or source aliases into integration. Whole native Sun/providers and Renderer compile; 2048 current raw retail/caller comparisons pass. Math has 33 manifest paths preserved exactly.\n\nThis is not approval of global Math debt, whole Shadow/Flush, Visibility/Render bodies, GPU storage/MSAA drafts, game link, boot or launch. Later3534+Math source union must rebuild and recheck true provider names/coverage. Root/canonical/intake were untouched.\n''',encoding='utf-8')
print(json.dumps({'checkpoint':result,'resume':pin(resume),'source_header_files':len(source_files),'external_checked':len(external['files']),'math_paths':33,'comparisons':2048,'effective_inputs':11,'head':HEAD},indent=2))
