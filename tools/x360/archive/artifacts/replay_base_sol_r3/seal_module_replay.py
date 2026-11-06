from pathlib import Path
import hashlib,importlib.util,json,subprocess
ROOT=Path('D:/codex-native/replay-base-sol-r3');OUT=Path(__file__).resolve().parent
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
run=lambda *a:subprocess.check_output(['git',*a],cwd=ROOT).decode().strip()
spec=importlib.util.spec_from_file_location('records','D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/derived-evidence-v2/coff_records.py');C=importlib.util.module_from_spec(spec);spec.loader.exec_module(C)
old=json.loads((OUT/'name-void-v2/compile-results.json').read_text());new=json.loads((OUT/'module-replay-v1/compile-results.json').read_text());key='src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.cpp'
a=next(r for r in old['results']if r['source']==key);b=next(r for r in new['results']if r['source']==key)
delta=C.compare(a['object'],b['object']);assert not delta['only_a']and not delta['only_b']
records=[C.Records(x['object'])for x in [a,b]];line_changes=[];ctor=[]
for name,change in delta['changed'].items():
    if name.startswith('?Construct@RaceCarEntityModule@'):ctor.append(name);continue
    assert change['a']['size']==change['b']['size']and change['a']['relocations']==change['b']['relocations']
    bodies=[]
    for record in records:
        _,value,section,_,_=next(x for x in record.symbols.values()if x[0]==name);h=record.sections[section];bodies.append(record.data[h[4]+value:h[4]+h[3]])
    for offset in range(0,len(bodies[0]),4):
        aw=int.from_bytes(bodies[0][offset:offset+4],'big');bw=int.from_bytes(bodies[1][offset:offset+4],'big')
        if aw==bw:continue
        assert aw&0xFFFF0000==0x38A00000 and bw-aw==5,(name,offset,hex(aw),hex(bw))
        line_changes.append({'function':name,'offset':hex(offset),'old_line':aw&0xFFFF,'new_line':bw&0xFFFF})
assert len(ctor)==1 and len(line_changes)==212 and len(delta['changed'])==44 and delta['common']==344
delta['assertion_source_line_changes']=line_changes
p=OUT/'module-replay-code-delta.json';assert not p.exists();p.write_text(json.dumps(delta,indent=2)+'\n')
binding=json.loads((OUT/'module-replay-v1/source-object-binding.json').read_text())
for p,h in binding['sources'].items():assert sha(ROOT/p)==h
for p,h in binding['actual_inputs'].items():assert sha(p)==h
assert run('diff','HEAD^','HEAD','--name-only')==key
header=ROOT/'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h';assert sha(header)=='8573aa9e9039bee8792f77b77d57a4f999f4fc9e9ba48658c4746b6fb5c06f77'
artifacts=['POSTMORTEM_MODULE_REPLAY.md','NAME_VOID_CONSTRUCTOR_CLOSURE.json','NATIVE_INITIALIZATION_FIX.json','module-replay-code-delta.json','run_module_build.py','validate_module_replay_v1.py','module-replay-v1/compile-results.json','module-replay-v1/source-object-binding.json','module-replay-v1-module-replay-v1.json','name-void-v2-module-replay-v1.json']
for r in new['results']:artifacts += [r['object'],r['assembly'],r['log']]
pos=json.loads((OUT/'module-replay-v1-module-replay-v1.json').read_text());neg=json.loads((OUT/'name-void-v2-module-replay-v1.json').read_text())
assert not pos['replay_mismatches'] and len(neg['replay_mismatches'])==128
report={'status':'PRIVATE_NATIVE_MODULE_REPLAY_INITIALIZATION_READY_FOR_BOUNDED_SOURCE_INTEGRATION','commit':run('rev-parse','HEAD'),'parent':run('rev-parse','HEAD^'),'minimum_order':['3fe384095764ca88d72262c7fd4a203bbfe57986','08161d055d077444da3f83d0e193a16c2090adc8','010302bc1a6a1076098d6f5beeb069f5528f9272','c27765da40ea52c5b346bb295a3e0d14c0a36fb3','a9461759c08838563ad758930e25dad9188e44f1',run('rev-parse','HEAD')],'base_R3':'2b4c441f20fafbb1b061618fda4d6e023abe8336','owned_paths':{key:sha(ROOT/key)},'new_CPPs':0,'source_effect':'Only mbInReplay=false and named mReplaySerialiser.Construct(); no185F5 clear or other behavior/header/layout change.','unchanged_RCE_header_sha256':sha(header),'whole_CPPs':{'requested':3,'passed':new['passed'],'failed':new['failed'],'actual_inputs':len(binding['actual_inputs'])},'actual_composed_replay_cases':128,'replay_mismatches':0,'prior_module_negative_mismatches':128,'physical_derived_pointer':'module+18600, no normalization','covered_region':'185E0..1869F, including full80-byte serialiser and surrounding poisoned guards/flags; static/dynamic buffers and16-byte guards after actual state methods','outside_module_differing_byte_counts':sorted(set(x['differing_module_bytes_outside_replay_window']for x in pos['preserved_whole_module_outside_debt'])),'emitted_code_delta':{'common_functions':344,'raw_identical_functions':300,'assertion_line_only_functions':43,'assertion_line_immediates_plus5':212,'other_changed_functions':ctor},'controlled_services':pos['limits'],'remaining_limits':['Whole module constructor byte/behavior parity explicitly fails outside the bounded replay interval.','Actual nested Director+100 containment negative remains separate.','The constrained128 alignment declaration is unchanged and not newly promoted to original-spelling or allocator proof.','No FrameClear/foreign constructor/monitor internals, actual assertion UI, synchronization, Xbox hardware, full replay, full image or launch proof.'],'artifacts':{str((OUT/p)if not Path(p).is_absolute()else Path(p)):sha((OUT/p)if not Path(p).is_absolute()else Path(p))for p in artifacts},'clean':not run('status','--porcelain')}
p=OUT/'MODULE_REPLAY_INITIALIZATION.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print(report['commit'],p,sha(p))
