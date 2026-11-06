from pathlib import Path
import hashlib,json,re,subprocess,sys
ROOT=Path('D:/codex-native/replay-base-sol-r3');OUT=Path(__file__).resolve().parent
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2')
from coff_records import Records,compare
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
git=lambda *a:subprocess.check_output(['git','-C',str(ROOT),*a]).decode('utf-8').strip()
head=git('rev-parse','HEAD');assert head=='010302bc1a6a1076098d6f5beeb069f5528f9272';assert not git('status','--porcelain')
final=json.loads((OUT/'final-v3/compile-results.json').read_text());old=json.loads((OUT/'baseline-v1/compile-results.json').read_text())
assert (final['passed'],final['failed'])==(10,1)
bind=json.loads((OUT/'final-v3/source-object-binding.json').read_text())
assert bind['actual_inputs']=={p:sha(p)for p in bind['actual_inputs']}
for p,h in bind['sources'].items():assert sha(ROOT/p)==h
negative=json.loads((OUT/'baseline-v1-constructor-comparison.json').read_text());positive=json.loads((OUT/'final-v3-constructor-comparison.json').read_text());state=json.loads((OUT/'final-v3-state-sequence-v2.json').read_text())
assert sum(g['cases']for g in negative['groups'])==512 and all(len(g['mismatches'])==g['cases']for g in negative['groups'])
assert sum(g['cases']for g in positive['groups'])==512 and all(not g['mismatches']for g in positive['groups'])
assert state['cases']==128 and not state['mismatches']
oldrows={r['source']:r for r in old['results']};rows={r['source']:r for r in final['results']}
def errors(row):return [l for l in Path(row['log']).read_text().splitlines()if 'error 'in l]
fail=next(r for r in final['results']if not r['passed']);assert len(errors(fail))==9 and errors(fail)==errors(oldrows[fail['source']])
base=next(p for p in rows if p.endswith('BrnReplayBaseSerialiser.cpp'));derived=next(p for p in rows if p.endswith('BrnReplayRaceCarEntitySerialiser.cpp'))
basecompare=compare(oldrows[base]['object'],rows[base]['object']);derivedcompare=compare(oldrows[derived]['object'],rows[derived]['object'])
assert len(basecompare['changed'])==1 and all(n.startswith('?Construct@BaseSerialiser@')for n in basecompare['changed'])
assert not basecompare['only_a']and not basecompare['only_b']and not derivedcompare['changed']and not derivedcompare['only_a']and not derivedcompare['only_b']
(OUT/'provider-code-delta.json').write_text(json.dumps({'base':basecompare,'derived':derivedcompare,'limit':'Derived code identity alone is not the store proof; the new composed tests link the changed reconstructed BaseCtor.'},indent=2)+'\n')
header='src/GameSource/Replays/BrnReplayBaseSerialiser.h';cpp='src/GameSource/Replays/BrnReplayBaseSerialiser.cpp'
assert 'u32                maX360Extension18[2]'in(ROOT/header).read_text()
assert (ROOT/cpp).read_text().count('CGS_STATIC_ASSERT(')==7
originals=['3fe384095764ca88d72262c7fd4a203bbfe57986','08161d055d077444da3f83d0e193a16c2090adc8']
aliases=[git('rev-parse','751cf24e2'),git('rev-parse','2f74d9dbb')]
names=['POSTMORTEM.md','run_build.py','census_construct.py','construct-primary-inventory.json','validate_construct.py','validate_state_sequence.py','validate_state_sequence_v2.py','provider-code-delta.json','baseline-v1/compile-results.json','baseline-v1/source-object-binding.json','baseline-v1-constructor-comparison.json','words-v1/compile-results.json','words-v1-constructor-comparison.json','words-v1-state-sequence-v2.json','words-final-v2/compile-results.json','words-final-v2-constructor-comparison.json','words-final-v2-state-sequence-v2.json','final-v3/compile-results.json','final-v3/source-object-binding.json','final-v3-constructor-comparison.json','final-v3-state-sequence-v2.json']
report={
 'status':'PRIVATE_NATIVE_CONSTRUCTOR_WORD_INITIALIZATION_FIX_READY_LIMITED_TO_DOCUMENTED_CONTRACT',
 'commit':head,'base_R3':'2b4c441f20fafbb1b061618fda4d6e023abe8336','worktree':str(ROOT),'clean':True,
 'owned_paths':{p:sha(ROOT/p)for p in [header,cpp]},'new_CPPs':0,
 'RCE_validation_dependencies':[{'original':a,'local':b}for a,b in zip(originals,aliases)],
 'minimum_source_dependency':'Base fix applies to the existing R3 Base home; the RCE pair is the validation dependency for its genuine80-byte derived instance, not a new alignment requirement on Base.',
 'changes':'Existing Xbox18/1C interval is two attested native words, retaining its honest legacy name because semantic names/roles are not recovered. Clear both via named typed data; no new storage/padding/wrapper/default ctor or RCE/World/Traffic header change.',
 'hard_Base_guards':7,'Base_size':'0x5C','Base_alignment':4,
 'whole_CPP_check':{'passed':10,'failed':1,'requested':11,'same_nine_Sound_lane_errors':errors(fail),'actual_inputs':len(bind['actual_inputs'])},
 'new_original_compiled_comparisons':{'constructor_cases':512,'constructor_mismatches':0,'composed_state_cases':128,'composed_state_mismatches':0,'compiled_Base_is_real_not_redirected_to_original':True},
 'negative_control':{'constructor_cases':512,'mismatches':512,'only_bad_bytes':'this+18..1F','report':str(OUT/'baseline-v1-constructor-comparison.json')},
 'ABI_scope':'Actual r3 object pointer and r4..r9 id/context/buffer sizes/name/skip inputs; all located native skip inputs0/1. Whole object/guard/name-source bytes compared for names0..31/null; actual read/write/getter returns compared in state sequence. No register rebinding or type-layout normalization.',
 'controlled_services':['Frame::Clear live/previous services (raw argument trace and marker writes).','Standard memcpy for actual original/reconstructed Base read/write byte copies.'],
 'unresolved_contracts':['Legacy reconstructed s32 constructor return differs from void-shaped original; incidental constructor r3 excluded, not normalized.','Existing shared StrCpy invalid-long-name length assertion/continuation is not repaired; actual launch-name domain covered.','SoundSerialiser wholeCPP retains9 pre-existing V3 lane errors.','Current RCE module Construct omits original185F4 clear and derived call18600; owner confirms no active WIP, routed Root separately.'],
 'related_RCE_dependency':{'source':'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp','original_clear':'822FE0B4','original_derived_call':'822FE0B8','restored_flag_note':'185F5 clear belongs to EnterReplay822D2DA4, not invented in Construct.','edited':False},
 'limits':'Finite initialization/data-state contract; not full invalid/error-path Constructor/API return parity, standalone allocator, FrameClear internals, hardware, full replay/image/launch proof. Earlier original-Base-linked RCE evidence remains separate and is not relabelled as this reconstructed-provider proof.',
 'artifacts':{str(OUT/p):sha(OUT/p)for p in names}
}
p=OUT/'NATIVE_INITIALIZATION_FIX.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'commit':head,'receipt':str(p),'sha256':sha(p),'paths':report['owned_paths']},indent=2))
