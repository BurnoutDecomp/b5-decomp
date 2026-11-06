from pathlib import Path
import hashlib,json,re,subprocess,sys
from coff_records import Records
OUT=Path(__file__).resolve().parent
MAIN=Path('D:/codex-native/racecar-containment-sol-r1')
LAB=Path('D:/codex-native/rce-constraints-sol')
R1='a26ca20f812a737975e02d92d12156958a50866b'
HELD='3fe384095764ca88d72262c7fd4a203bbfe57986'
FINAL='08161d055d077444da3f83d0e193a16c2090adc8'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(root,*a):return subprocess.check_output(['git','-C',str(root),*a]).decode('utf-8').strip()
assert git(MAIN,'rev-parse','HEAD')==FINAL
assert not git(MAIN,'status','--porcelain') and not git(LAB,'status','--porcelain')
paths=['src/GameSource/Replays/Serialisers/BrnReplayRaceCarEntitySerialiser.h','src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h']
owned={p:sha(MAIN/p)for p in paths}
assert owned=={p:sha(LAB/p)for p in paths}
final=json.loads((OUT/'final-native-v1/compile-results.json').read_text())
assert final['passed']==20 and final['failed']==6
inputs={p:h for row in final['results']for p,h in row['inputs'].items()}
assert inputs=={p:sha(p)for p in inputs}
for p,h in owned.items():assert inputs[str((LAB/p).resolve())]==h
code=json.loads((OUT/'candidate-codegen-comparison.json').read_text())
assert all(not x['changed']and not x['only_a']and not x['only_b']for x in code['method_code_comparisons']['members128'].values())
native=json.loads((OUT/'candidate-native-boundaries.json').read_text())
assert sum(x['cases']for x in native['methods'])==640 and all(not x['mismatches']for x in native['methods'])
assert all(len(x['mismatches'])==(32 if x['expected_negative']else 0)for x in native['world_prefix'])
assert sha(OUT.parent/'CONTAINMENT_DIAGNOSTIC_CHECKPOINT.json')=='faad577c1804a084ee30e074a10594ed488a5f33252b14eb597346f5091331d4'
world=Path('D:/Reverse/BP-X360-Experimental-Artifacts/world-containment-r1-sol/WORLD_CONSTRAINTS.json')
assert sha(world)=='48748b67a7c155da98f92b0e47f80a2a13f97cf5e3ce3fa6385476677bb3e406'
raw_before=subprocess.check_output(['git','-C',str(MAIN),'show',R1+':'+paths[1]]).decode('utf-8')
raw_after=(MAIN/paths[1]).read_text(encoding='utf-8')
decl=lambda s:re.search(r'void\s+RenderRaceCar\s*\([^;]*;',s,re.S).group().replace('\r\n','\n')
assert decl(raw_before)==decl(raw_after)
patch=subprocess.check_output(['git','-C',str(MAIN),'diff','--binary',R1,FINAL,'--',*paths])
(OUT/'RCE_COMBINED_OWNED.patch').write_bytes(patch)
controls={}
for k in ['class128','members128']:
    r=json.loads((OUT/('candidate-'+k+'-v2/compile-results.json')).read_text())
    row=next(x for x in r['results']if 'candidate-probe.cpp'in x['source'])
    controls[k]={n:v for n,v in Records(row['object']).functions().items()if n.startswith('Diagnostic')}
failures=[{'source':r['source'],'diagnostic_count':len(r['errors']),'errors':r['errors'],'log':r['log']}for r in final['results']if not r['passed']]
(OUT/'REMAINING_CONSUMERS.json').write_text(json.dumps(failures,indent=2)+'\n')
foreign_original=['aeb809e9b','4f2a81704','7d9539653','5862f71b6','cb2ee9b8f','02396283d','2528dc239','ab0f90b2b','e5999b06e','64f990bf3']
foreign_local=['e52ac2965','110bc6279','934d95414','c15113f72','5c9966552','490e02196','afd8619ed','3c3bd9620','8eb07d44b','d13668139']
aliases=[{'original':git(MAIN,'rev-parse',a),'lab':git(LAB,'rev-parse',b)}for a,b in zip(foreign_original,foreign_local)]
artifact_names=['POSTMORTEM_CANDIDATE.md','RCE_COMBINED_OWNED.patch','REMAINING_CONSUMERS.json',
 'decfigs-retained-metadata.json','decfigs-internal-metadata.json','native-derived-user-census.json',
 'candidate-codegen-comparison.json','candidate-native-boundaries.json','final-code-retention.json',
 'build_candidates.py','build_candidates_v2.py','build_final.py','coff_records.py','validate_candidate_boundaries.py',
 'extract_decfigs_metadata.py','extract_internal_metadata.py','census_native_derived_users.py',
 'candidate-provider-baseline/compile-results.json','candidate-class128/compile-results.json',
 'candidate-class128-v2/compile-results.json','candidate-class64-v2/compile-results.json',
 'candidate-members128-v2/compile-results.json','candidate-members64-v2/compile-results.json',
 'final-native-v1/compile-results.json']
report={
 'status':'PRIVATE_CONSTRAINED_DECLARATION_READY_FOR_OWNER_REBIND_NOT_PUBLICATION',
 'inference_boundary':'The128-byte class attribute is the minimal reconstruction satisfying independent ARTIST containment/caller constraints under the corroborated ordinary declarations. Its exact original C++ spelling is not independently recovered. Alternative extra member annotations can be observationally equivalent.',
 'minimal_owned_source_order':[HELD,FINAL],'root_base':R1,'new_CPPs':0,
 'source_worktree':str(MAIN),'source_head':FINAL,'source_paths':owned,'source_clean':True,
 'lab_worktree':str(LAB),'lab_alias_of_final':git(LAB,'rev-parse','HEAD'),
 'validation_only_foreign_dependencies':aliases,
 'parking_body_dependency':{'original':'dd11badd34355016c6c37d0b77b4147ec614ea2e','local':git(MAIN,'rev-parse','7c4387ed4')},
 'minimum_recipe':'On R1 or a compatible descendant, apply3fe and081 atomically, or the exact combined owned two-header patch. The foreign10 commits are the actual World validation graph, not required changes to the serialiser definition. Preserve separately owned Render signature hunks.',
 'all38_RCE_gates_active_and_pass':True,'all33_pre_replay_gates_preserved':True,
 'all_Math_Base_constructor_field_storage_unchanged_in_final_step':True,
 'reserved_RenderRaceCar_signature_unchanged':True,
 'candidate_layouts':code['layouts'],
 'actual_final_compile':{'passed':20,'failed':6,'requested':26,'unique_actual_inputs':len(inputs),'report':str(OUT/'final-native-v1/compile-results.json'),'same_six_failures_across_all_four_candidates':True},
 'actual_method_equivalence':{'successful_real_CPPs':20,'identical_emitted_functions_between_class128_and_members128':2064,'original_parity_not_implied':True,'provider_baseline_functions_unchanged':code['provider_baseline_comparison']['common']},
 'native_comparisons':{'derived_cases':640,'derived_mismatches':0,'positive_World_receiver_cases':64,'negative_World_receiver_cases':64,'parking128_retained_by_exact_function_and_relocation_identity':True,'report':str(OUT/'candidate-native-boundaries.json')},
 'constructor_copy_effect_controls':controls,
 'world_prefix_owner_receipt':{'path':str(world),'sha256':sha(world)},
 'remaining_source_body_debt':['Current module Construct omits original replay reset/call work.','Reconstructed BaseSerialiser::Construct omits original18/1C clears; derived Construct comparison links original Base code, not that reconstructed body.','Six named consumer CPPs are not compile-ready; see separate queue.','No complete World/sibling/layout/link/boot/gameplay certification.'],
 'original_controls':{'checkpoint':HELD,'receipt':str(OUT.parent/'CONTAINMENT_DIAGNOSTIC_CHECKPOINT.json'),'sha256':sha(OUT.parent/'CONTAINMENT_DIAGNOSTIC_CHECKPOINT.json')},
 'artifacts':{str(OUT/p):sha(OUT/p)for p in artifact_names}
}
p=OUT/'RCE_CONSTRAINED_CANDIDATE.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'source':FINAL,'lab':report['lab_alias_of_final'],'receipt':str(p),'sha256':sha(p),'owned_paths':owned},indent=2))
