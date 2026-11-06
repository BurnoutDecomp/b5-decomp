"""Bounded repaired-R21 terminal readback. No compile or product rehash."""
from pathlib import Path
from datetime import datetime,timezone
import hashlib,json,subprocess
H=Path(__file__).resolve().parent;B=H.parent/'repair-r21-layout-intake-20261103';read=lambda p:json.loads(Path(p).read_text());sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(p,v):
 with Path(p).open('x',encoding='utf-8',newline='\n')as f:json.dump(v,f,indent=2);f.write('\n')
C=read(B/'campaign.json');completion=read(H/'completion.json');seal=read(completion['diagnostic_file']);operation=read(H/'operation.json');W=Path(C['workspace']);CAN=Path(C['canonical']);head=completion['head']
assert operation['owned_pid']is None and operation['last_operation_pid']==seal['operation_pid']and seal['status']==completion['status']
for file,key in [(Path(completion['diagnostic_file']),'diagnostic_sha256'),(H/'composite-active-native.json','report_sha256'),(H/'composite-artifact-audit.json','audit_sha256')]:assert sha(file)==completion[key]
assert sha(H/'original-Game-guards-proof.json')==seal['original_Game_guards_proof_sha256']
def git(*a,cwd=W):return subprocess.check_output(['git','--no-optional-locks',*a],cwd=cwd,text=True).strip()
assert git('rev-parse','HEAD')==head and not git('status','--porcelain=v1')and not git('branch','--show-current')
assert git('rev-parse','HEAD^')==seal['base']and git('rev-parse','codex/experimental-x360-native-r21-checkpoint')==seal['base']
assert git('rev-parse','HEAD',cwd=CAN)==C['published_base']and not git('status','--porcelain=v1',cwd=CAN)
index=git('ls-files','--stage','b5-decomp',cwd=CAN.parent);assert index==seal['parent_gitlink_index']
if completion['local_checkpoint']:assert git('rev-parse',completion['local_checkpoint'])==head
ps="Get-CimInstance Win32_Process | Where-Object { $_.ProcessId -eq "+str(seal['operation_pid'])+" -or (($_.Name -in @('cl.exe','link.exe')) -and $_.CommandLine -like '*repair-r21-game-cxx03-repair-20261103*') -or ($_.Name -like 'python*' -and $_.CommandLine -like '*run_repair_gate.py*') } | Select-Object ProcessId,ParentProcessId,Name,CommandLine | ConvertTo-Json -Compress"
raw=subprocess.check_output(['powershell','-NoProfile','-Command',ps],text=True).strip();live=json.loads(raw)if raw else[]
if isinstance(live,dict):live=[live]
assert not live,live
value={'head':head,'status':completion['status'],'owned_operation_pid':seal['operation_pid'],'owned_operation_pid_live':False,'owned_compiler_or_linker_children':live,'compiler_exit':completion['compiler_exit'],'binder_exit':completion['binder_exit'],'intake_clean':True,'intake_detached':True,'checkpoint':completion['local_checkpoint'],'prior_checkpoint_preserved':'codex/experimental-x360-native-r21-checkpoint','prior_checkpoint_head':seal['base'],'canonical_head':C['published_base'],'canonical_clean':True,'parent_index_gitlink':index,'parent_working_gitlink_dirty_expected':True,'seal_sha256':completion['diagnostic_sha256'],'report_sha256':completion['report_sha256'],'audit_sha256':completion['audit_sha256'],'created_utc':datetime.now(timezone.utc).isoformat()};save(H/'terminal-process-and-checkpoint-readback.json',value)
text=f'''# Repaired R21 native result

**{completion['native_passed']} PASS / {completion['native_failed']} FAIL / 3616 active native units**, plus 2 separate SDK references (3618 broad dispositions).

The repaired composite contains **1 fresh Game object + 178 verified R21 phase-one rows + 3437 verified R20 rows**, retaining their actual build origins. The 179-source first gate remains preserved at 178 PASS / 1 FAIL. A five-line, one-CPP constructor repair then passed its separate native 1/1 gate; no 179-source repeat or full sweep was run. All 3615 previous passes survived, and the final old failure was repaired. No source/provider/denominator change.

Clean detached HEAD: `{head}`. New checkpoint: `{completion['local_checkpoint']}`. The original R21 checkpoint at `{seal['base']}` is preserved. The repair changes exactly five constructor statements and keeps all blocks, assignments, headers, layout guards and the dead diagnostic counter. The only difference from the author's Game source is the retained three counter/comment lines.

Native `Vector3(raw)` copies the full `__vector4` register, including W; it does not use the scalar three-lane constructor. The actual Game source now passes with the original size `0x9A1300` and Network-offset `0x8C3600` assertions active. All nine canonical fallbacks, `/showIncludes`, D-only outputs/TEMP and unchanged frozen context were verified. The seven unaffected named guards were retained with proofs. Previously verified products were not hashed again.

Seal SHA-256: `{completion['diagnostic_sha256']}`

Report SHA-256: `{completion['report_sha256']}`

Audit SHA-256: `{completion['audit_sha256']}`

The operation/compiler/binder are terminal. Canonical remains clean at `3534f7f16103a58ff904bbd1604e18e9934868f4`; parent index gitlink remains `209fb3b94f7266506dff4ffd569e8553f1b3af93`. Its working gitlink difference is expected.

The original failed gate and author Game source-context gap remain in `repair-r21-layout-intake-20261103`. This is native compilation evidence; opaque-storage identity and parity limits remain. No game link, runtime, wave17 link-repair/R22 source, ledger, production, main/dev/origin or publication change was made.
'''
with(H/'README.md').open('x',encoding='utf-8',newline='\n')as f:f.write(text)
save(H/'final-artifact-bindings.json',{'completion_sha256':sha(H/'completion.json'),'terminal_readback_sha256':sha(H/'terminal-process-and-checkpoint-readback.json'),'README_sha256':sha(H/'README.md'),**completion});print(json.dumps(value,indent=2))
