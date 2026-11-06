import json,hashlib,subprocess
from pathlib import Path
repo=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/fma_scalar_luna_526adc')
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave6/fma_native_luna')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
head=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD'],text=True).strip()
base=subprocess.check_output(['git','-C',str(repo),'rev-parse','HEAD~2'],text=True).strip()
status=subprocess.check_output(['git','-C',str(repo),'status','--porcelain'],text=True)
c1='c21c82b203466f67b39f2f1e8f3cd47204ea5fb9'
c2='a76269d542b8e7fa9c5efc63124b6f093d4bd352'
changed={}
for c in (c1,c2):
 paths=subprocess.check_output(['git','-C',str(repo),'diff-tree','--no-commit-id','--name-only','-r',c],text=True).splitlines()
 changed[c]=paths
assert changed[c1]==['src/GameShared/GameClasses/System/Input/CgsInputPads.cpp']
assert changed[c2]==['src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp']
assert base=='526adc27662c0c2aedf417980dcf824c989bb035' and head==c2
assert not status
validation=out/'whole_cpp_codegen_validation.json'
v=json.loads(validation.read_text(encoding='utf-8'))
assert v['reports']['inputpads']['source_sha256']==sha(repo/v['reports']['inputpads']['source'])
assert v['reports']['gpu_perfmon']['source_sha256']==sha(repo/v['reports']['gpu_perfmon']['source'])
assert v['whole_cpp_codegen']['inputpads']['compiled_local_order'].count('fmadds')==2
assert len(v['whole_cpp_codegen']['gpu_perfmon']['compiled_table_fnmsubs'])==1
assert len(v['whole_cpp_codegen']['gpu_perfmon']['compiled_graph_fmadds'])==2
delivery={
 'status':'private, separate source commits, no push',
 'workspace':str(repo),'base':base,'head':head,'clean':not status,
 'commits':[
  {'sha':c1,'changed_paths':changed[c1],'trailer':'Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>'},
  {'sha':c2,'changed_paths':changed[c2],'trailer':'Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com>'}],
 'source_and_object_validation':{'path':str(validation),'sha256':sha(validation),'content':v},
 'rounding_witness':{'path':str(out/'rounding_witness_results.txt'),'sha256':sha(out/'rounding_witness_results.txt'),'model':'exact decimal products rounded to binary32; operation edge witnesses only, not PPC runtime or hardware execution'},
 'intrinsic_probe':{
  'xdk_scalar_names_rejected':['__fma','__fmaf','__fmadd','__fmadds','fmaf'],
  'reason':'each direct XDK 14.00.6502 compile probe reports C3861 identifier not found',
  'native_optimizer_probe':'scalar expression a*b+c emitted one fmadds; scalar expression c-a*b emitted one fnmsubs under shipping flags',
  'limits':'Only whole-CPP shipping-flag output proves the changed implementation sites; this is not a guarantee across other compilers or flags.'},
 'whole_cpp_results':{
  'inputpads':{'passed':True,'report':v['reports']['inputpads']['report'],'report_sha256':v['reports']['inputpads']['report_sha256'],'object':v['reports']['inputpads']['object'],'object_sha256':v['reports']['inputpads']['object_sha256'],'assembly':v['reports']['inputpads']['assembly'],'assembly_sha256':v['reports']['inputpads']['assembly_sha256'],'log':v['reports']['inputpads']['log'],'log_sha256':v['reports']['inputpads']['log_sha256']},
  'gpu_perfmon':{'passed':True,'report':v['reports']['gpu_perfmon']['report'],'report_sha256':v['reports']['gpu_perfmon']['report_sha256'],'object':v['reports']['gpu_perfmon']['object'],'object_sha256':v['reports']['gpu_perfmon']['assembly_sha256'],'assembly':v['reports']['gpu_perfmon']['assembly'],'assembly_sha256':v['reports']['gpu_perfmon']['assembly_sha256'],'log':v['reports']['gpu_perfmon']['log'],'log_sha256':v['reports']['gpu_perfmon']['log_sha256']}},
 'limits':['No actual PPC hardware/game execution was performed.','The bounded f32 witness distinguishes fused from separately rounded arithmetic; compiled codegen matches ARTIST opcodes.','No changes to headers, APIs, caller signatures or engine state.']}
(out/'delivery.json').write_text(json.dumps(delivery,indent=2)+'\n',encoding='utf-8')
md=f'''# Native scalar FMA CPP repairs

- Private checkout: {repo} (detached, clean)
- Base: {base} (frozen Wave6 head)
- Commit 1: {c1} — src/GameShared/GameClasses/System/Input/CgsInputPads.cpp
- Commit 2: {c2} — src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp
- Both commits include the actual-model trailer Co-Authored-By: Codex GPT-6 Luna <noreply@openai.com> and contain no other paths.
- Whole-CPP XDK result: InputPads 1/1 pass; GPU PerfMon 1/1 pass. The native COFF machine is 01F2.
- Full source/object/ASM/log hashes and exact compiled instructions: {validation} (SHA-256 {sha(validation)}).

## Native codegen evidence

- UpdateJoltEnvelope matches ARTIST 0x828E7618: the two compiled expressions emit fmadds. The release path orders fneg, fdivs, then fmadds, matching ARTIST 0x828E76E4/76E8/76EC. The same CPP builds its four direct UpdatePadRumble calls; ARTIST caller 0x828EFC58 uses r3=this, r4=envelope pointer, f1=time, with the result in f1.
- GPU RenderPerformanceTable emits one fnmsubs, matching ARTIST 0x8282CE58 (RenderPerformanceTable at 0x8282CDF0). RenderPerformanceGraph emits two fmadds, matching ARTIST 0x8282CF7C and 0x8282D184 (RenderPerformanceGraph at 0x8282CEA0). The same CPP compiles the RenderHUD branches to table/graph (0x8282F520).
- A local XDK probe showed no declared scalar __fma, __fmaf, __fmadd, __fmadds, or fmaf intrinsic. With the shipping XDK flags, a scalar multiply/add expression contracts to one fmadds; a scalar subtract/product expression contracts to one fnmsubs. Whole-CPP assembly—not the probe—binds this behavior at every changed site.

## Rounding edge witnesses

{out/'rounding_witness_results.txt'} records four binary32 edge triples. The InputPads attack and release cases and both Perf operation forms distinguish the fused result from separate binary32 multiply/add rounding. This is a focused arithmetic witness, not a gameplay or hardware test.

No actual PPC hardware/game execution or image launch was performed; these are compile and object-instruction results only.
'''
(out/'DELIVERY.md').write_text(md,encoding='utf-8')
print(json.dumps({'head':head,'base':base,'commits':changed,'clean':not status,'validation_sha256':sha(validation),'delivery':str(out/'DELIVERY.md'),'delivery_json':str(out/'delivery.json')},indent=2))
