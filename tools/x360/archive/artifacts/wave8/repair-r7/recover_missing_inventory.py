"""Continue only unrecorded R7 inventory after an interrupted compiler owner.

The old running-state report and every old artifact remain untouched.  The new
report is a diagnostic composite, never an invented exit of the interrupted run.
"""
from datetime import datetime, timezone
import hashlib, importlib.util, json, os, struct, subprocess, sys
from pathlib import Path
sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
CONFIG = json.loads((HERE/'campaign.json').read_text())
WORK = Path(CONFIG['workspace'])
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
RAW = HERE/'full-stage1/compile-results.json'
META = HERE/'native-parallel-wave8-r7-full-stage1-patch.json'
OUT = HERE/'full-missing-stage1'
TEMP = HERE/'full-missing-stage1-compiler-temp'
HEAD = '129afef95856e383366a55fb8f30a66ceb74de42'
def now(): return datetime.now(timezone.utc).isoformat()
def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,j):
    with Path(p).open('x',encoding='utf-8',newline='\n') as f: json.dump(j,f,indent=2); f.write('\n')
def load(p): return json.loads(Path(p).read_text(encoding='utf-8'))
def module(name,path):
    s=importlib.util.spec_from_file_location(name,path); m=importlib.util.module_from_spec(s); s.loader.exec_module(m); return m
e=module('r7_recovery_evidence',HERE.parent.parent/'wave5-delivery/wave5_delivery.py'); e.BASE=CONFIG['base']
meta=load(META); old=load(RAW); original_sha=sha(RAW)
assert meta['head']==HEAD and old['state']=='running' and old['requested']==3615
assert len(old['results'])==1700 and old['passed']==1512 and old['failed']==188
assert not (HERE/'full-stage1-exit.json').exists() and not OUT.exists() and not TEMP.exists()
e.check_current(meta)
plan=load(CONFIG['current_plan'])
assert e.git('ls-files','--stage','b5-decomp',cwd=PARENT).decode().strip()==plan['parent_gitlink_index']
assert e.git('rev-parse','HEAD',cwd=e.CANONICAL).decode().strip()==CONFIG['published_base']
assert not e.git('status','--porcelain=v1',cwd=e.CANONICAL).strip()
assert len(meta['source_snapshot']['hashes'])==6299
inventory=meta['native_inventory']; assert len(inventory)==3615
assert inventory==meta['selected_sources']
assert [r['source'] for r in old['results']]==inventory[:1700]
missing=inventory[1700:]; assert len(missing)==1915
driver=WORK/'tools/build_x360.py'; command=load(HERE/'full-stage1-command.json')
assert sha(driver)==command['driver_sha256']
d=module('actual_missing_native_driver',driver)
sdk=Path(old['sdk']).resolve(); compiler=sdk/'bin/win32/cl.exe'
includes=[WORK/p for p in d.INCLUDES if (WORK/p).is_dir()]+[sdk/'include/xbox']
canonical=str(e.CANONICAL).replace('\\','/').lower()
extra=[a for a in old['results'][0]['command'] if a.replace('\\','/').lower().startswith('/i'+canonical+'/vendor/')]
flags=d.FLAGS+['/I'+str(p) for p in includes]+extra
def row_artifacts(rows,output):
    result={}
    for r in rows:
        source=Path(r['source']); key=source.stem+'-'+hashlib.sha256(source.as_posix().lower().encode()).hexdigest()[:8]
        expected=[str(compiler)]+flags+['/Fo'+str(output/(key+'.obj')),'/Fa'+str(output/(key+'.asm')),str(source)]
        assert r['command']==expected, r['source']
        assert r['object']==str(output/(key+'.obj')) and r['assembly']==str(output/(key+'.asm')) and r['log']==str(output/(key+'.log'))
        item={'log_sha256':sha(r['log'])}
        if r['passed']:
            assert r['returncode']==0 and r['machine']==0x1f2
            assert struct.unpack('<H',Path(r['object']).read_bytes()[:2])[0]==0x1f2
            assert Path(r['assembly']).stat().st_size>0
            item.update(object_sha256=sha(r['object']),assembly_sha256=sha(r['assembly']))
        else: assert r['returncode']!=0 or r['machine']!=0x1f2
        result[r['source']]=item
    return result
old_artifacts=row_artifacts(old['results'],RAW.parent)
external=e.external_inputs(old); before=load(HERE/'full-stage1-external-inputs-before.json')['inputs']
for k in ('include_trees','submodules','compiler_and_runtime'): assert external[k]==before[k],k
old_files={str(p):sha(p) for p in sorted(RAW.parent.iterdir()) if p.is_file()}
unrecorded=sorted(set(old_files)-{r[k] for r in old['results'] for k in ('log','object','assembly')}-{str(RAW)})
save(HERE/'full-stage1-interrupted-checkpoint.json',{
    'head':HEAD,'state':'interrupted; no process exit observed','old_unified_handle':86018,'old_python_pid':19080,
    'handle_observation':'write_stdin reported Unknown process id 86018; root independently observed PID19080 and owned cl absent',
    'report':str(RAW),'report_sha256':original_sha,'original_metadata_sha256':sha(META),
    'recorded_rows':1700,'passed':1512,'failed':188,'requested':3615,'missing_sources':missing,
    'completed_artifact_manifest':old_artifacts,'all_original_files':old_files,
    'unrecorded_files_not_reused':unrecorded,'current_6299_snapshot_equal':True,
    'external_tool_submodule_context_equal_to_original_before':True,'driver_sha256':sha(driver),
    'scope':'Only completed rows in the saved report are reusable; stray original artifacts are preserved but not promoted. Original running state is immutable, not a terminal result.',
    'created_utc':now()})
save(HERE/'full-missing-stage1-external-inputs-before.json',{'head':HEAD,'inputs':external,'created_utc':now()})
TEMP.mkdir(); os.environ['TEMP']=os.environ['TMP']=str(TEMP)
arguments=[str(driver),'--sdk',str(sdk),'--out',str(OUT),'--jobs','8','--timeout','180','--quiet']
arguments+=['--compiler-arg='+x for x in extra]
for s in missing: arguments+=['--source',s]
save(HERE/'full-missing-stage1-command.json',{'head':HEAD,'argv':arguments,'driver_sha256':sha(driver),'launcher_sha256':sha(__file__),
    'private_temp':str(TEMP),'original_report_sha256':original_sha,'missing_sources':missing,'created_utc':now()})
ownerpath=PARENT/'scratch/x360_sdk/parallel-native/INTEGRATION_OWNER.json'; owner=load(ownerpath)
assert owner['owner']=='/root/wave5_integrator_sol'
owner.update(phase='wave8_repair_r7_missing_inventory_native_build',frozen_head=HEAD,live_process_id=os.getpid(),live_session=None,
    live_report=str(OUT/'compile-results.json'),original_interrupted_report=str(RAW),source_freeze_status='FROZEN; missing1915 only',
    instruction='Recovery of only1915 absent recorded sources after interrupted original1700; same frozen inputs, jobs8, D outputs. No full replay/source mutation. Poll SAME recovery handle.')
ownerpath.write_text(json.dumps(owner,indent=2)+'\n',encoding='utf-8')
print('RECOVERY_FROZEN',HEAD,'verified_original',1700,'missing',len(missing),'inputs',6299,'PID',os.getpid(),flush=True)
sys.argv=arguments; code=d.main()
newpath=OUT/'compile-results.json'; new,idx,artifacts=e.common.report_check(newpath)
assert set(idx)==set(missing) and new['requested']==1915
assert sha(RAW)==original_sha and row_artifacts(old['results'],RAW.parent)==old_artifacts
assert {str(p):sha(p) for p in sorted(RAW.parent.iterdir()) if p.is_file()}==old_files
e.check_current(meta)
after=e.external_inputs(new)
for k in ('include_trees','submodules','compiler_and_runtime'): assert external[k]==after[k],k
save(HERE/'full-missing-stage1-external-inputs-after.json',{'head':HEAD,'inputs':after,'created_utc':now()})
save(HERE/'full-missing-stage1-exit.json',{'head':HEAD,'exit_code':code,'report':str(newpath),'report_sha256':sha(newpath),'created_utc':now()})
combined=dict(old); combined.update(state='complete',results=old['results']+new['results'])
combined['passed']=sum(r['passed'] for r in combined['results']); combined['failed']=3615-combined['passed']
combined['provenance']={'kind':'Interrupted native inventory completed from valid recorded rows plus missing-inventory recovery',
    'head':HEAD,'original_report':str(RAW),'original_report_sha256':original_sha,'original_state':'running/interrupted','original_exit_observed':False,
    'retained_recorded_rows':1700,'missing_report':str(newpath),'missing_report_sha256':sha(newpath),'missing_exit_code':code,
    'newly_compiled_missing_rows':1915,'all_inventory_sources_once':True,'not_a_restart_or_second_full':True,
    'inputs_unchanged_6299':True,'same_flags_and_external_tool_context':True,'checkpoint_sha256':sha(HERE/'full-stage1-interrupted-checkpoint.json')}
assert len({r['source'] for r in combined['results']})==3615
destination=HERE/'full-recovered'; destination.mkdir()
save(destination/'compile-results.json',combined)
newmeta=dict(meta); newmeta.update(recovery=combined['provenance'],created_utc=now())
save(HERE/'native-parallel-wave8-r7-full-recovered-patch.json',newmeta)
save(HERE/'full-recovered-external-inputs-before.json',{'head':HEAD,'inputs':external,'created_utc':now()})
save(HERE/'full-recovered-external-inputs-after.json',{'head':HEAD,'inputs':after,'created_utc':now()})
save(HERE/'full-recovered-exit.json',{'head':HEAD,'exit_code':1 if combined['failed'] else 0,'kind':'Aggregate diagnostic status; NOT original process exit',
    'original_exit_observed':False,'missing_process_exit_code':code,'report':str(destination/'compile-results.json'),'report_sha256':sha(destination/'compile-results.json'),'created_utc':now()})
print('RECOVERY_COMPLETE',combined['passed'],combined['failed'],'retained1700+new1915',flush=True)
raise SystemExit(code)
