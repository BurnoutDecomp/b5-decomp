from pathlib import Path
import ast,json,hashlib,sys,importlib.util,struct,difflib,subprocess
OUT=Path(__file__).resolve().parent;ROOT=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/wave8_scene_terra')
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
row=json.loads((OUT/'candidate-v4/compile-results.json').read_text())['results'][0];assert row['passed']
binding=json.loads((OUT/'candidate-v4/source-object-binding.json').read_text())
owned='src/vendor/renderware/collision/SeparatingDirection.cpp';final=ROOT/owned
assert sha(final)==binding['source_and_required_headers'][owned]
(OUT/'SeparatingDirection.final.cpp').write_bytes(final.read_bytes())
(OUT/'GPInstance.header-at-final-gate.hpp').write_bytes((ROOT/'src/vendor/renderware/collision/GPInstance.hpp').read_bytes())
assert sha(OUT/'GPInstance.header-at-final-gate.hpp')==binding['source_and_required_headers']['src/vendor/renderware/collision/GPInstance.hpp']
(OUT/'SeparatingDirection.sol-owned.diff').write_text(''.join(difflib.unified_diff((OUT/'SeparatingDirection.before.cpp').read_text().splitlines(keepends=True),final.read_text().splitlines(keepends=True),fromfile=owned+' (World handed-off1e39)',tofile=owned+' (Sol final)')))
# Preserve raw words for two original functions whose dedicated JSON export is
# absent. Primary raw image reads, not a pseudocode-derived instruction stream.
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
for addr,count in [(0x82BB4CA8,272),(0x82BAA258,19)]:
 p=OUT/('%08X.raw-asm.txt'%addr)
 assert not p.exists();p.write_text('\n'.join(ppcdis.disasm(addr,count))+'\n')
 (OUT/('%08X.raw.bin'%addr)).write_bytes(x360rd.rd(addr,count*4))
proofs={n:sha(OUT/n)for n in ['rim-edge-v2-diagnostic.json','rim-rim-v2-diagnostic.json','appenders-v2-diagnostic.json','cylinder-builder-v2-diagnostic.json','cylinder-builder-v3-diagnostic.json','kernel-v2-diagnostic.json','drivers-v3-composed-diagnostic.json','unordered-generic-v4-comparison.json','runtime-retention-v4.json']}
ret=json.loads((OUT/'runtime-retention-v4.json').read_text());assert ret['v3_to_final_v4']['common']==43 and not ret['v3_to_final_v4']['changed']
changed=list(ret['v2_to_v3']['changed']);assert len(changed)==1 and 'FindBestSepDirWithCylinder_FindCandidates' in changed[0]
compiler=Path(row['command'][0]);sdkpins={str(compiler):sha(compiler)}
for name in ['c1xx.dll','c2.dll']:
 p=compiler.with_name(name)
 if p.exists():sdkpins[str(p)]=sha(p)
post='''Self-postmortem, native SAT file only

Whole real CPP passes SDK6534.4/XDK14.00.6502, PPC32BE. No header/layout
relaxation, scalar Vec4 ABI, plain callback substitution, exact sqrt/division
replacement, empty return or offset-cast accommodation was introduced.

ARTIST sources: 82BB4038/4660/47F0/4CA8/50E8/53F8/56C8/6118/61C0/64C0/75B0/76E8
and 82BAA1A0/1F0/250/258. The rim bodies retain actual ordered VMX intrinsics,
including duplicated ONE-refine radial paths, sequential reciprocal refinements,
true fused negative subtraction crosses, all-lane CR records/selects, and the
scalar fsubs during the general RimToRim interpolation. Native all-four-lane
results and mutated buffer guards are compared, not geometry-only xyz values.

The generic scalar fcmpu/ble path uses ordered >: unordered is NOT_GT, branches,
and retains the first candidate. Six final-object NaN/infinity counterexamples
exercise 16 flip and12 best unordered branch occurrences. The cylinder driver
uses native vector all-greater records rather than a scalar-lane comparison.

RimToEdge v1-v5 -> v1; RimToRim references r3-r9/out r10; AddRimToEdgeCandidate
r3-r7/f1/f2/r10 plus count pointer on stack are actually executed by both sides.
Driver result is genuine VecFloat v1; GP interval dispatch uses the actual native
member-pointer type. Composed checks link the real GPRegistration table and all
five actual interval providers; no function hook implements any primitive math.

The inherited cylinder builder lost fused plus-rim/support constructions.
Nine negative actual-object cases are retained. The corrected builder matches
72/72 complete candidate buffers, including16-candidate box and degenerate
cases. Only this function's machine bytes/relocations changed from v2 to v3.
All43 emitted functions are identical v3 to final v4 after comment cleanup.

Declaration evidence limit: cylinder rim declarations are absent in the searched
DecFIGS/Feb corpus. Native ABI and scalar-splat radius uses support the genuine
VecFloat signature; exact original C++ radius type spelling is not proven.
Descriptive private helper names remain recovered labels, not original symbols.
The header edits belong to World and must travel with the coherent graph.

Runtime limits: shared software VMX reciprocal/rsqrt/NaN semantics are used on
both actual instruction streams. This is not a Xenon hardware estimate/denormal
or all-nonfinite contract proof. Rim BSS guard is the attested post-CRT FLT_MIN
value; arbitrary uninitialized startup state is not supported. GP objects are
constructed fixtures with true member layouts and genuine table relocations.
The external provider objects are exactly pinned prior World builds; final
shared graph/source rebinding is still World-owned. No complete collision solver,
contact/prism, GPU, full program, boot, launch or gameplay parity is claimed.
'''
(OUT/'POSTMORTEM.txt').write_text(post)
manifest={
 'status':'file-level implementation handoff; private shared World graph not committed',
 'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip(),
 'owner':'vector_abi_sol','owned_path':owned,
 'preimage_sha256':sha(OUT/'SeparatingDirection.before.cpp'),'source_sha256':sha(final),
 'source_snapshot':str(OUT/'SeparatingDirection.final.cpp'),'owned_delta_sha256':sha(OUT/'SeparatingDirection.sol-owned.diff'),
 'whole_cpp':{'report':str(OUT/'candidate-v4/compile-results.json'),'report_sha256':sha(OUT/'candidate-v4/compile-results.json'),'passed':True,'object':row['object'],'object_sha256':sha(row['object']),'assembly':row['assembly'],'assembly_sha256':sha(row['assembly']),'binding':str(OUT/'candidate-v4/source-object-binding.json'),'binding_sha256':sha(OUT/'candidate-v4/source-object-binding.json'),'resolved_headers':len(binding['actual_includes']),'compiler_pins':sdkpins},
 'evidence':proofs,
 'passing_complete_function_cases':{'RimToEdge':96,'RimToRim':160,'AddAxisToEdgeCandidate':64,'AddRimToEdgeCandidate':64,'AddRimToRimCandidates':64,'CylinderFindCandidates':72,'GenericFindCandidates':96,'BoxBox':64,'TriBox':64,'SphSph':64,'GenericDriver_composed':64,'CylinderDriver_composed':64,'VolCylDriver_composed':32,'GenericDriver_final_unordered':6},
 'total_passing_function_cases':974,
 'negative_controls':{'old_v2_whole_builder':{'cases':72,'mismatches':9,'path':str(OUT/'cylinder-builder-v2-diagnostic.json')},'old_world_TU':'volume-baseline/SeparatingDirection-native-v2.log failed before the old cylinder/rim source was migrated'},
 'postmortem_sha256':sha(OUT/'POSTMORTEM.txt'),
 'header_contracts':{'owner':'world_resume_terra','GPInstance.hpp_sha256':binding['source_and_required_headers']['src/vendor/renderware/collision/GPInstance.hpp'],'exact_snapshot':str(OUT/'GPInstance.header-at-final-gate.hpp'),'needed_delta':'true V3/VecFloat SAT/rim contracts, RimToRim reference ABI, genuine PMF GetIntervals; no layout change in this owned CPP'},
 'integration':'Only this CPP was edited by Sol; no stage/commit. World retains complete shared graph and must preserve header/API dependencies plus current whole-source bindings. Old GPBox/Cylinder/Physics and replay seals remain unchanged.',
 'limits':post[post.index('Declaration evidence limit:'):],
 'jobs':'all owned native compiler and SAT interpreter jobs terminal; compiler slot and header freeze released'
}
p=OUT/'FILE_HANDOFF.json';assert not p.exists();p.write_text(json.dumps(manifest,indent=2)+'\n')
print('FILE_HANDOFF',sha(p));print('SOURCE',sha(final));print('OBJECT',sha(row['object']));print('BINDING',sha(OUT/'candidate-v4/source-object-binding.json'));print('HEADERS',len(binding['actual_includes']));print('cases',sum(manifest['passing_complete_function_cases'].values()))
