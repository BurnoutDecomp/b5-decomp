"""Compile only20 changed CPPs plus the unchanged real Fine consumer."""
from datetime import datetime,timezone
import hashlib,importlib.util,json,os,re,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');C=json.loads((HERE/'campaign.json').read_text());WORK=Path(C['workspace']);plan=json.loads(Path(C['current_plan']).read_text());OUT=HERE/'targeted-stage1';TEMP=HERE/'targeted-stage1-compiler-temp'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def module(n,p):
 s=importlib.util.spec_from_file_location(n,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
e=module('r12_native_evidence',HERE.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base']
head=e.git('rev-parse','HEAD').decode().strip();receipts=read(HERE/'repair-r12-receipts.json');assert receipts['state']=='complete'and receipts['head']==head and len(receipts['receipts'])==4
assert not e.git('status','--porcelain=v1').strip()and not OUT.exists()and not TEMP.exists()
assert e.git('rev-parse','HEAD',cwd=e.CANONICAL).decode().strip()==C['published_base']and not e.git('status','--porcelain=v1',cwd=e.CANONICAL).strip()
assert e.git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==plan['parent_gitlink_index']
base_meta=read(C['baseline_metadata']);base_audit=read(C['baseline_artifact_manifest']);baseline=read(C['baseline_report'])
assert sha(C['baseline_metadata'])==C['baseline_metadata_sha256']and sha(C['baseline_report'])==C['baseline_report_sha256']and sha(C['baseline_artifact_manifest'])==C['baseline_artifact_manifest_sha256']
snapshot=e.common.source_snapshot();assert len(snapshot['hashes'])==6302
changed={p for p in set(snapshot['hashes'])|set(base_meta['source_snapshot']['hashes'])if snapshot['hashes'].get(p)!=base_meta['source_snapshot']['hashes'].get(p)}
assert changed==set(plan['unique_paths'])and all(Path(p).suffix.lower()=='.cpp'for p in changed)
native=sorted(p for p in e.common.inventory()if Path(p).suffix.lower()in['.c','.cpp']);assert native==base_meta['native_inventory']and len(native)==3616
incoming={p:sorted(set(children)&changed)for p,children in snapshot['dependencies'].items()if set(children)&changed};assert not incoming,('CPP include dependency prevents safe20-only coverage',incoming)
assert snapshot['nonliteral_includes']==base_meta['source_snapshot']['nonliteral_includes']==[{'path':'vendor/lua/src/lua.h','directive':'LUA_USER_H'}]
assert not any(re.search(r'/[DU]LUA_USER_H(?:[= ]|$)',a,re.I)for r in baseline['results']for a in r['command'])
assert not any(re.search(r'^\s*#\s*define\s+LUA_USER_H\b',(WORK/p).read_text(encoding='utf-8-sig',errors='replace'),re.M)for p in snapshot['hashes'])
external=e.external_inputs(baseline)
for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==base_audit['external_inputs'][k],k
literal_cpp=[];changed_names={Path(p).name.lower()for p in changed}
for p in snapshot['hashes']:
 text=(WORK/p).read_text(encoding='utf-8-sig',errors='replace')
 for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',text,re.M):
  inc=match[1].replace('\\','/')
  if Path(inc).suffix.lower()in['.cpp','.c']:literal_cpp.append({'source':p,'include':inc})
assert not any(Path(r['include']).name.lower()in changed_names for r in literal_cpp)
for directory,tree in external['include_trees'].items():
 for rel in tree['files']:
  p=Path(directory)/rel
  if p.suffix.lower()not in e.common.TEXT_EXT:continue
  for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',p.read_text(encoding='utf-8-sig',errors='replace'),re.M):
   assert Path(match[1].replace('\\','/')).name.lower()not in changed_names,(str(p),match[1])
sources=plan['selected_union_native_sources'];assert len(sources)==20
patch=e.git('diff','--binary',C['base']);meta={'base':C['base'],'head':head,'workspace':str(WORK),'sha256':hashlib.sha256(patch).hexdigest(),'paths':plan['unique_paths'],'source_sha256':{p:sha(WORK/p)for p in changed},
 'source_snapshot':snapshot,'selected_sources':sources,'selection':'targeted20; no full replay','native_inventory':native,'receipt_sha256':sha(HERE/'repair-r12-receipts.json'),'created_utc':datetime.now(timezone.utc).isoformat()}
metapath=HERE/'native-parallel-wave8-r12-targeted-stage1-patch.json';save(metapath,meta)
with(HERE/'native-parallel-wave8-r12-targeted-stage1.diff').open('xb')as f:f.write(patch)
save(HERE/'reuse-input-preconditions-before.json',{'head':head,'base':C['base'],'changed_only20_CPP':sorted(changed),'all_other_source_and_header_hashes_unchanged':True,'native_inventory_equal':True,
 'reverse_edges_into_changed_CPP':incoming,'literal_CPP_C_includes':literal_cpp,'no_literal_or_known_nonliteral_include_of_changed_CPP':True,'nonliteral_includes':snapshot['nonliteral_includes'],
 'nonliteral_LUA_USER_H_not_defined_in_sources_or_commands':True,'external_include_tools_submodules_equal_R11':True,'scope':'This is input precondition evidence only; reuse requires terminal artifact verification as well.'})
save(HERE/'targeted-stage1-external-inputs-before.json',{'head':head,'inputs':external})
canonical=str(e.CANONICAL).replace('\\','/').lower();extra=[a for a in baseline['results'][0]['command']if a.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
driver=WORK/'tools/build_x360.py';assert sha(driver)==read(HERE.parent/'repair-r11/full-stage1-command.json')['driver_sha256']
TEMP.mkdir();os.environ['TEMP']=os.environ['TMP']=str(TEMP)
argv=[str(driver),'--sdk',baseline['sdk'],'--out',str(OUT),'--jobs','8','--timeout','180','--quiet']+['--compiler-arg='+a for a in extra]+['--compiler-arg=/showIncludes']
for p in sources:argv+=['--source',p]
save(HERE/'targeted-stage1-command.json',{'head':head,'argv':argv,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_temp':str(TEMP),'metadata_sha256':sha(metapath)})
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json';o=read(ownerpath);assert o['owner']=='/root/wave5_integrator_sol'
o.update(phase='wave8_repair_r12_targeted20_native_build',frozen_head=head,intake_head=head,live_process_id=os.getpid(),live_session=None,live_report=str(OUT/'compile-results.json'),source_metadata=str(metapath),source_metadata_sha256=sha(metapath),
 source_freeze_status='FROZEN exact4/20 CPP; actual20 rebuilds only',instruction='Poll SAME targeted20 handle; no full replay/source expansion. After all20 pass,prove unchanged input/artifact context and assemble explicit3596 retained+20 fresh composite. No forced link/publication.')
ownerpath.write_text(json.dumps(o,indent=2)+'\n',encoding='utf-8')
print('FROZEN_R12',head,'requested',20,'PID',os.getpid(),'input6302 inventory3616',flush=True)
d=module('r12_actual_native_driver',driver);sys.argv=argv;code=d.main();report=read(OUT/'compile-results.json')
assert e.common.source_snapshot()==snapshot
after=e.external_inputs(report)
for k in ['include_trees','submodules','compiler_and_runtime']:assert external[k]==after[k],k
headers={};all_before={str((WORK/p).resolve()):h for p,h in snapshot['hashes'].items()}
for directory,tree in external['include_trees'].items():
 for rel,h in tree['files'].items():all_before[str((Path(directory)/rel).resolve())]=h
for row in report['results']:
 found={}
 for line in Path(row['log']).read_text(errors='replace').splitlines():
  if 'including file:'in line:
   p=Path(line.split('including file:',1)[1].strip()).resolve();key=str(p);assert key in all_before,('Unfrozen actual include',row['source'],key);assert sha(p)==all_before[key];found[key]=all_before[key]
 headers[row['source']]=found
save(HERE/'targeted-stage1-actual-includes.json',{'head':head,'rows':headers,'unique_actual_headers':len({p for h in headers.values()for p in h}),'all_actual_includes_frozen_before_and_unchanged_after':True})
save(HERE/'targeted-stage1-external-inputs-after.json',{'head':head,'inputs':after})
save(HERE/'targeted-stage1-exit.json',{'head':head,'exit_code':code,'report':str(OUT/'compile-results.json'),'report_sha256':sha(OUT/'compile-results.json')})
raise SystemExit(code)
