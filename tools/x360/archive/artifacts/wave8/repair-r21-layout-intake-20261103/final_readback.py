"""Bounded terminal readback; no compile or repeated product hashing."""
from pathlib import Path
import hashlib,json,subprocess
from datetime import datetime,timezone
H=Path(__file__).resolve().parent;read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
C=read(H/'campaign.json');completion=read(H/'completion.json');seal=read(completion['diagnostic_file']);operation=read(H/'operation.json');W=Path(C['workspace']);CAN=Path(C['canonical']);head=completion['head']
assert operation['owned_pid']is None and operation['last_operation_pid']==seal['operation_pid']and seal['status']==completion['status']
for file,key in [(Path(completion['diagnostic_file']),'diagnostic_sha256'),(H/'composite-active-native.json','report_sha256'),(H/'composite-artifact-audit.json','audit_sha256')]:assert sha(file)==completion[key]
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd,text=True).strip()
assert git('rev-parse','HEAD')==head and not git('status','--porcelain=v1')and not git('branch','--show-current')
assert git('rev-parse','HEAD',cwd=CAN)==C['published_base']and not git('status','--porcelain=v1',cwd=CAN)
index=git('ls-files','--stage','b5-decomp',cwd=CAN.parent);assert index==seal['parent_gitlink_index']
if completion['local_checkpoint']:assert git('rev-parse',completion['local_checkpoint'])==head
ps="Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -eq "+str(seal['operation_pid'])+" -or (($_.Name -in @('cl.exe','link.exe')) -and $_.CommandLine -like '*repair-r21-layout-intake-20261103*') -or ($_.Name -like 'python*' -and $_.CommandLine -like '*r21_native_pipeline.py*') } | Select-Object ProcessId,ParentProcessId,Name,CommandLine | ConvertTo-Json -Compress"
raw=subprocess.check_output(['powershell','-NoProfile','-Command',ps],text=True).strip();live=json.loads(raw)if raw else[]
if isinstance(live,dict):live=[live]
assert not live,live
source='src/GameSource/Game/BrnGameModule.cpp';snapshot=read(H/'source-metadata.json')['source_snapshot'];baseline=read(C['baseline_artifact_manifest'])['source_snapshot'];text=(W/source).read_text(encoding='utf-8-sig');report=read(H/'native179/compile-results.json');game=next(r for r in report['results']if r['source']==source)
assert snapshot['hashes'][source]==baseline['hashes'][source]==sha(W/source)
assert '/D_XBOX'in game['command']and 'NativeGameModuleSize[sizeof(BrnGameModule) == 0x9A1300 ? 1 : -1]'in text and 'NativeNetworkOffset[offsetof(BrnGameModule, mNetworkModule) == 0x8C3600 ? 1 : -1]'in text
save(H/'original-Game-guards-proof.json',{'head':head,'source':source,'source_sha256':snapshot['hashes'][source],'source_identical_to_R20':True,'actual_compile_XBOX_defined':True,'original_size_assertion':'0x9A1300','original_Network_assertion':'0x8C3600','all_other_original_offset_assertions_unchanged':True,'fresh_native_PASS':game['passed'],'actual_log':game['log'],'actual_log_sha256':sha(game['log']),'actual_report_sha256':sha(H/'native179/compile-results.json'),'no_extra_compile':True})
value={'head':head,'status':completion['status'],'owned_operation_pid':seal['operation_pid'],'owned_operation_pid_live':False,'owned_compiler_or_linker_children':live,'compiler_exit':completion['compiler_exit'],'binder_exit':completion['binder_exit'],'intake_clean':True,'intake_detached':True,'checkpoint':completion['local_checkpoint'],'canonical_head':C['published_base'],'canonical_clean':True,'parent_index_gitlink':index,'parent_working_gitlink_dirty_expected':True,'seal_sha256':completion['diagnostic_sha256'],'report_sha256':completion['report_sha256'],'audit_sha256':completion['audit_sha256'],'original_Game_guard_proof_sha256':sha(H/'original-Game-guards-proof.json'),'created_utc':datetime.now(timezone.utc).isoformat()};save(H/'terminal-process-and-checkpoint-readback.json',value)
text=f'''# R21 layout native result

**{completion['native_passed']} PASS / {completion['native_failed']} FAIL / 3616 active native units**, plus 2 separate SDK references (3618 broad dispositions). Status: `{completion['status']}`.

The actual integrated gate compiled 179 sources: {completion['fresh_passed']} PASS, {completion['fresh_failed']} FAIL. The composite retains 3437 verified untouched rows with their real prior build origins. All {completion['prior_PASS_retained']} prior passes survived; {completion['old_failure_repairs']} old failure was repaired. No new provider or denominator change. This was a finite gate, not a fresh full sweep.

Frozen clean detached HEAD: `{head}`. Local checkpoint: `{completion['local_checkpoint']}`. Four exact layout originals changed 22 paths (10 CPP/12 headers); inventory remains 6305 native text inputs. All nine vendor fallbacks, `/showIncludes`, frozen source/header/external/tool context and D-only outputs/TEMP were used. The seven unaffected named safeguards were retained with input proofs; no new named-guard compile was run. Original Game size `0x9A1300` and Network-offset `0x8C3600` assertions stayed active in the fresh source compile.

Seal SHA-256: `{completion['diagnostic_sha256']}`

Report SHA-256: `{completion['report_sha256']}`

Audit SHA-256: `{completion['audit_sha256']}`

The operation/compiler/binder are terminal. Canonical remains clean at `3534f7f16103a58ff904bbd1604e18e9934868f4`; the parent index gitlink remains `209fb3b94f7266506dff4ffd569e8553f1b3af93`. Its working gitlink difference is expected.

Opaque native storage identities and behavior limits remain explicit in the author receipts and review. No wave17 link-repair source, game link, runtime, whole-class parity, ledger, production, main/dev/origin or publication change is included.
'''
with(H/'README.md').open('x',encoding='utf-8',newline='\n')as f:f.write(text)
save(H/'final-artifact-bindings.json',{'completion_sha256':sha(H/'completion.json'),'terminal_readback_sha256':sha(H/'terminal-process-and-checkpoint-readback.json'),'README_sha256':sha(H/'README.md'),'original_Game_guard_proof_sha256':sha(H/'original-Game-guards-proof.json'),**completion});print(json.dumps(value,indent=2))
