from pathlib import Path
import hashlib,json,ast,sys,inspect
OUT=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
world=OUT.parent;D=world/'volume-baseline';P=world/'scripts/validate_quartic_native_v2_model.py'
nodes=[]
for n in ast.parse(P.read_text()).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='out'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
OUT=Path(__file__).resolve().parent
files={
 'positive_report':OUT/'quartic-correct-load-same64.json',
 'positive_verifier':OUT/'validate_same64_correct_load.py',
 'legacy_exact_trace':OUT/'case1-logexp-trace.json',
 'positive_exact_trace':OUT/'case1-logexp-correct-load-trace.json',
 'load_dispatch_trace':OUT/'case1-lvlx-dispatch-trace.json',
 'coefficient_trace':OUT/'case1-coefficient-trace.json',
 'SAT_static_impact':OUT/'SAT-lvlx128-impact-inventory.json',
 'old_negative_report':D/'quartic-native-v2-model-comparison.json',
 'actual_object':D/'LineSeg-current-v8.obj',
 'actual_listing':D/'LineSeg-current-v8.asm',
 'frozen_current_product_source':Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra/src/vendor/renderware/collision/LineSegIntersect.cpp'),
 'original_primary_export':Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82BACA30.json'),
 'world_original_verifier':P,
 'legacy_handler':Path(sys.modules['atc_update_emu'].__file__),
 'existing_correct_handler':Path(cylmodel.Cpu.exec_vector.__code__.co_filename),
 'raw_field_reference':Path(r'D:/Reverse/BP-Decomp_Workflow/tools/re/vmx128.py')}
positive=json.loads(files['positive_report'].read_text());old=json.loads(files['old_negative_report'].read_text());fixed=json.loads(files['positive_exact_trace'].read_text());legacy=json.loads(files['legacy_exact_trace'].read_text());impact=json.loads(files['SAT_static_impact'].read_text())
assert positive['compared']==64 and not positive['mismatches']and not positive['exceptions'];assert len(old['mismatches'])==58
assert not any(impact['compiled_opcode_family_hits'].values())and not impact['original_opcode_family_hits']
notes='''Read-only quartic source-versus-fixture diagnosis

No product/source/SDK/shared-fixture file was changed or compiled.
The unchanged LineSeg-current-v8 object executes true SDK XMVectorPow.

First divergence is a load destination, not the slope multiplication:
  ARTIST82BACFE0 raw13204C07 = lvlx128 v57,r0,r9.
  r9=4400FE90 points at the actual var170, memory word40000000 (2.0).
  f12=2.0 and that store are correct. The source polynomial c4 is3E051A02.

The combined CPU MRO places atc_update_emu.UpdateCPU before cylmodel.Cpu.
The earlier lvlx128 handler uses ((word>>21)&31)|((word&3)<<5), giving
v121. The true destination high bits are bits2-3 of the low word, so
((word>>21)&31)|(((word>>2)&3)<<5) gives v57. The later cylmodel handler
already implements this correct field, but the legacy handler shadows it.
Consequently old v57 retains c4=.12998202443122864; vspltw82BAD010 feeds it
as the fourth pow exponent, even though the actual exponent in memory is2.

At case1/root.4709235429763794 the old fourth exp floor is−1 (scale.5),
producing slope1.6905174255371094. Correcting only load dispatch yields
floor−3 (scale.125) and slope1.5927221775054932, exactly the real compiled
object. Residual.2325131595134735 agrees before/after. Subsequent original
and compiled iterates also agree; do not alter exponent/source to imitate
the broken original-model result.

Private CorrectLoadCPU gives the lvlx128/lvrx128 family to the existing
correct cylmodel handler before calling super for every other instruction.
The exact original64 seed/coefficient/root guard cases now pass64/64,
45accepted/19rejected, no exceptions;1318 original corrected loads and0
compiled occurrences. The old58/64 failure and every old trace stay intact.

SAT impact:0 occurrences of the affected opcode family in every final SAT
COFF text section, all six linked GP-provider/table COFF texts and conservative
original SAT/interval ranges. The immutable974-case SAT handoff remains bound;
no replayed or relabelled pass campaign was performed.

Limits: differential execution still shares software VMX log/exp/reciprocal
estimates and nonfinite behavior. No Xenon hardware/denormal/root-conditioning
or all coefficients proof is implied. Standard compiler save/restore runtime
controls are unchanged from World's original verifier. The global/shared
emulator is not modified; other prior receipts need their own opcode execution
inventory if this family is reached. Final coherent source/torus/provider
rebinding remains World-owned.
'''
(OUT/'DIAGNOSIS.txt').write_text(notes,encoding='utf-8')
receipt={
 'status':'read-only author diagnostic sealed; actual product unchanged',
 'first_divergent_original_instruction':{'pc':'0x82BACFE0','word':'0x13204C07','instruction':'lvlx128 v57,r0,r9','EA':'0x4400FE90','memory_lane0':'0x40000000','true_destination':57,'legacy_destination':121,'next_exponent_consumer':'0x82BAD010','stale_value_bits':'0x3E051A02'},
 'cause':'atc_update_emu.UpdateCPU lvlx128 destination extension uses low source/opcode bits; earlier Python MRO shadows correct cylmodel handler',
 'private_correction':'prioritize existing correct lvlx128/lvrx128 decoder before inherited legacy method; no math/source/input normalization',
 'same_original64_result':{'compared':64,'mismatches':0,'exceptions':0,'accepted':45,'rejected':19,'corrected_loads':positive['corrected_loads']},
 'original_negative_preserved':{'mismatches':58,'cases':64,'path':str(files['old_negative_report'])},
 'SAT_affected_family_occurrences':0,
 'inputs_and_reports':{key:{'path':str(path),'sha256':sha(path)}for key,path in files.items()},
 'MRO':[(cls.__module__,cls.__name__)for cls in CPU.__mro__],
 'diagnosis_text_sha256':sha(OUT/'DIAGNOSIS.txt'),
 'limits':notes[notes.index('Limits:'):],
 'jobs':'all owned compiler and CPU diagnostic jobs terminal; World LineSeg source ownership unchanged'}
p=OUT/'READ_ONLY_DIAGNOSIS.json';assert not p.exists();p.write_text(json.dumps(receipt,indent=2)+'\n')
print('RECEIPT',sha(p));print('POSITIVE_REPORT',sha(files['positive_report']));print('VERIFIER',sha(files['positive_verifier']));print('LINESEG_OBJECT',sha(files['actual_object']));print('SAT_IMPACT',sha(files['SAT_static_impact']));print('LINESEG_SOURCE',sha(files['frozen_current_product_source']))
