"""Freeze exact R11 source and run only the authorized selected/full native gate."""
from datetime import datetime,timezone
import argparse,hashlib,importlib.util,json,os,subprocess,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;PARENT=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');SCRATCH=PARENT/'scratch/x360_sdk'
C=json.loads((HERE/'campaign.json').read_text());WORK=Path(C['workspace']);plan=json.loads(Path(C['current_plan']).read_text())
p=argparse.ArgumentParser();p.add_argument('--name',required=True);p.add_argument('--mode',choices=['selected','full'],required=True);args=p.parse_args()
def git(*a,cwd=WORK):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(j,f,indent=2);f.write('\n')
def loadmodule(name,p):
 s=importlib.util.spec_from_file_location(name,p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
head=git('rev-parse','HEAD').decode().strip();receipts=json.loads((HERE/'repair-r11-receipts.json').read_text())
assert not git('status','--porcelain=v1').strip()and receipts['head']==head and receipts['state']=='complete'and len(receipts['receipts'])==2
assert {r['source_commit']for r in receipts['receipts']}==set(plan['proposed_originals'])
assert git('rev-parse','HEAD',cwd=Path(C['canonical'])).decode().strip()==C['published_base']and not git('status','--porcelain=v1',cwd=Path(C['canonical'])).strip()
assert git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==plan['parent_gitlink_index']
paths=git('diff','--name-only',C['base']).decode().splitlines();assert set(paths)==set(plan['unique_paths'])
for name,v in plan['planned_source_images'].items():assert git('rev-parse','HEAD:'+name).decode().strip()==v['final']
common=loadmodule('r11_native_input_graph',SCRATCH/'parallel-native/wave4_integrator_sol/wave4_evidence.py');snapshot=common.source_snapshot()
native=sorted(p for p in common.inventory()if Path(p).suffix.lower()in['.c','.cpp'])
assert len(native)==3616 and len(snapshot['hashes'])==6302
if args.mode=='full':
 audit=json.loads((HERE/'selected-stage1-terminal-audit.json').read_text());named=json.loads((HERE/'named-native-gates/gate-results.json').read_text())
 assert audit['head']==named['head']==head and not audit['summary']['regressions']and audit['summary']['R10_prior_passes_in_selection']==plan['selected_R10_passes']
 assert all(r['gate_satisfied']for r in named['rows']);proof=json.loads((HERE/'integrated-getparameters-proof.json').read_text());assert proof['head']==head and proof['exact48'] and proof['code_relocations']==[];sources=native
else:
 sources=plan['selected_union_native_sources'];assert len(sources)==8 and set(sources)<=set(native)
 assert plan['registration_consumer']in sources
patch=git('diff','--binary',C['base']);batch='native-parallel-wave8-r11-'+args.name;output=HERE/args.name;temp=HERE/(args.name+'-compiler-temp');assert not output.exists()and not temp.exists()
meta={'base':C['base'],'head':head,'workspace':str(WORK),'sha256':hashlib.sha256(patch).hexdigest(),'paths':paths,
 'source_sha256':{p:sha(WORK/p)for p in paths},'approved_removed_sources':[],'source_snapshot':snapshot,'selected_sources':sources,'selection':args.mode,'native_inventory':native,
 'receipt_sha256':sha(HERE/'repair-r11-receipts.json'),'plan_sha256':sha(C['current_plan']),'created_utc':datetime.now(timezone.utc).isoformat()}
save(HERE/(batch+'-patch.json'),meta)
with(HERE/(batch+'.diff')).open('xb')as f:f.write(patch)
baseline=json.loads(Path(C['baseline_report']).read_text());canonical=str(Path(C['canonical'])).replace('\\','/').lower()
extra=[a for a in baseline['results'][0]['command']if a.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
e=loadmodule('r11_prepared_external_context',HERE.parent.parent/'wave5-delivery/wave5_delivery.py');e.BASE=C['base'];external=e.external_inputs(baseline)
save(HERE/(args.name+'-external-inputs-before.json'),{'head':head,'inputs':external,'created_utc':datetime.now(timezone.utc).isoformat()})
temp.mkdir();os.environ['TEMP']=os.environ['TMP']=str(temp);driver=WORK/'tools/build_x360.py'
arguments=[str(driver),'--sdk',baseline['sdk'],'--out',str(output),'--jobs','8','--timeout','180','--quiet']+['--compiler-arg='+a for a in extra]
if args.mode=='selected':arguments+=['--compiler-arg=/showIncludes']
for source in sources:arguments+=['--source',source]
save(HERE/(args.name+'-command.json'),{'head':head,'argv':arguments,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),'private_temp':str(temp),'metadata_sha256':sha(HERE/(batch+'-patch.json'))})
ownerpath=SCRATCH/'parallel-native/INTEGRATION_OWNER.json';owner=json.loads(ownerpath.read_text());assert owner['owner']=='/root/wave5_integrator_sol'
owner.update(phase='wave8_repair_r11_'+args.name+'_native_build',frozen_head=head,intake_head=head,live_process_id=os.getpid(),live_session=None,live_report=str(output/'compile-results.json'),
 source_freeze_status='FROZEN exact2/10 R11 during actual '+args.mode+' gate',source_metadata=str(HERE/(batch+'-patch.json')),source_metadata_sha256=sha(HERE/(batch+'-patch.json')),
 instruction='R11 exact2 frozen, no optional source expansion. Poll SAME compiler handle to actual terminal; no restart on observation timeout.8 native workers plus max4 private1,all output/TEMP D; no canonical/index/ledger/publication.')
ownerpath.write_text(json.dumps(owner,indent=2)+'\n',encoding='utf-8')
SCRATCH.joinpath('parallel-native/wave5_integrator_sol/CURRENT_OPERATION.md').write_text(
 '# R11 actual native '+args.mode+' gate\n\nSole intake writer `/root/wave5_integrator_sol`. Frozen head `'+head+'`;exact2 originals/10 paths,7CPP/3headers;2newheaders+1newCPP. Native inventory3616,inputs6302.\n\nStage '+args.name+' requests'+str(len(sources))+' sources,jobs8. Actual PythonPID'+str(os.getpid())+'. Poll SAME handle; do not restart/mutate source. Partial results are not a final count.\n\nR10 completed composite3258/357 and all earlier interruption/recovery history remain immutable. Canonical3534/main/dev/index/ledger untouched. Existing native guards and negatives remain; no geometry/scene/GameModule/link-input changes or optional owner-runtime suites. Whole-native goal ACTIVE; no publication/formal review.\n',encoding='utf-8')
print('FROZEN',head,'mode',args.mode,'requested',len(sources),'PID',os.getpid(),flush=True)
d=loadmodule('r11_actual_native_driver',driver);sys.argv=arguments;code=d.main()
report=json.loads((output/'compile-results.json').read_text());after=e.external_inputs(report)
for key in ['include_trees','submodules','compiler_and_runtime']:assert external[key]==after[key],key
assert common.source_snapshot()==snapshot
if args.mode=='selected':
 headers={};frozen={str((WORK/p).resolve()):h for p,h in snapshot['hashes'].items()}
 for directory,tree in external['include_trees'].items():
  for rel,digest in tree['files'].items():frozen[str((Path(directory)/rel).resolve())]=digest
 for row in report['results']:
  found={}
  for line in Path(row['log']).read_text(errors='replace').splitlines():
   if 'including file:'in line:
    path=Path(line.split('including file:',1)[1].strip()).resolve();key=str(path);assert key in frozen,('Unfrozen include',key);assert sha(path)==frozen[key];found[key]=frozen[key]
  headers[row['source']]=found
 save(HERE/'selected-stage1-actual-includes.json',{'head':head,'rows':headers,'unique_actual_headers':len({p for hs in headers.values()for p in hs}),'all_actual_headers_frozen_before_unchanged_after':True})
save(HERE/(args.name+'-external-inputs-after.json'),{'head':head,'inputs':after,'created_utc':datetime.now(timezone.utc).isoformat()})
save(HERE/(args.name+'-exit.json'),{'head':head,'exit_code':code,'report':str(output/'compile-results.json'),'report_sha256':sha(output/'compile-results.json')})
raise SystemExit(code)
