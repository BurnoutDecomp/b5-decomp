from pathlib import Path
import hashlib, json, re, subprocess, sys

ROOT = Path('D:/codex-native/racecar-containment-sol-r1')
OUT = Path(__file__).resolve().parent
EX = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()
git = lambda *a: subprocess.check_output(['git','-C',str(ROOT),*a]).decode('utf-8').strip()
head = git('rev-parse','HEAD')
assert head == '3fe384095764ca88d72262c7fd4a203bbfe57986'
assert not git('status','--porcelain')
rel = 'src/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'
header = ROOT/rel
before = OUT/'declaration-order-v2/header-before-full-gates.h'
marker = b'inline void RaceCarEntityModule::LockLayout_()'
assert before.read_bytes().split(marker)[0] == header.read_bytes().split(marker)[0]
baseline = subprocess.check_output(['git','-C',str(ROOT),'show','a26ca20f812a737975e02d92d12156958a50866b:'+rel]).decode('utf-8')
current = header.read_text(encoding='utf-8')
decl = lambda s: re.search(r'void\s+RenderRaceCar\s*\([^;]*;',s,re.S).group(0).replace('\r\n','\n')
assert decl(baseline) == decl(current)
sources = json.loads((OUT/'pre-replay-callers-v1/source-object-binding.json').read_text())
assert sources['actual_headers'][str(header)] == sha(before)
parking = json.loads((OUT/'caller-boundaries-v2-parking.json').read_text())
assert parking['cases'] == 128 and not parking['mismatches']
old = json.loads((OUT/'baseline-r1/measured-layout.json').read_text())
new = json.loads((OUT/'declaration-order-v2/measured-layout.json').read_text())
assert old['types']['BrnWorld::RaceCarEntityModule']['size'] == 0x20D50
assert new['types']['BrnWorld::RaceCarEntityModule']['size'] == 0x187E0
hard = json.loads((OUT/'hard-bounds-callers-v1/compile-results.json').read_text())
assert hard['passed'] == 1 and hard['failed'] == 2
for row in hard['results']:
    errors = [l for l in Path(row['log']).read_text(encoding='utf-8').splitlines() if 'error ' in l]
    if not row['passed']: assert len(errors) == 5 and all('C2118' in e for e in errors)
primaries = ['827E5160','827E5998','823BD4E0','822FD898','822D2C88','822E79F8','82301BB0','8264C4C0','82518028','8228FE98','822FF5B0']
primary = {str(EX/('0x'+a+'.json')):sha(EX/('0x'+a+'.json')) for a in primaries}
primary[str(Path('D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h'))] = sha('D:/Reverse/BP-Decomp_Workflow/references/DecFIGS/dwarfdump/GameSource/World/EntityModules/RaceCarEntityModule/BrnRaceCarEntityModule.h')
artifact_names = [
 'POSTMORTEM.md','build_layout.py','reconcile_containment.py','run_callers.py','validate_pre_replay_callers.py',
 'baseline-r1/measured-layout.json','baseline-r1/inputs.json','baseline-r1/layout.obj',
 'declaration-order-v1/layout.log','declaration-order-v1/diagnosed-offsets.log',
 'declaration-order-v2/measured-layout.json','declaration-order-v2/inputs.json','declaration-order-v2/layout.obj','declaration-order-v2/header-before-full-gates.h',
 'pre-replay-callers-v1/compile-results.json','pre-replay-callers-v1/source-object-binding.json','caller-boundaries-v2-parking.json',
 'hard-bounds-v1/layout.log','hard-bounds-v1/inputs.json',
 'hard-bounds-callers-v1/compile-results.json','hard-bounds-callers-v1/source-object-binding.json',
 'type-alignment-controls/compile.log','type-alignment-controls-v2/probe.cpp','type-alignment-controls-v2/probe.obj','type-alignment-controls-v2/probe.asm',
 'replay-matrix-immediate-census.json']
artifacts = {str(OUT/p):sha(OUT/p) for p in artifact_names}
gate_count = current.count('CGS_STATIC_ASSERT(')
report = {
 'status':'HELD_DIAGNOSTIC_ONLY_REPLAY_CONTAINMENT_UNRESOLVED_DO_NOT_COLLECT',
 'root_base':'a26ca20f812a737975e02d92d12156958a50866b',
 'foreign_dependency':{'original':'dd11badd34355016c6c37d0b77b4147ec614ea2e','local':'7c4387ed4dbafcc4e499c8ad19d5d0989e4da59c'},
 'checkpoint':head,'worktree':str(ROOT),'owned_paths':{rel:sha(header)},'new_CPPs':0,
 'source_graph':git('log','-3','--format=%H %s'),
 'all_other_sources_including_Math_unchanged':True,
 'reserved_RenderRaceCar_declaration_unchanged':True,
 'pre_gate_header':{'path':str(before),'sha256':sha(before),'declarations_byte_identical_to_checkpoint':True,'only_LockLayout_body_differs':True},
 'actual_caller_before_added_hard_gates':{'whole_CPP_passed':3,'whole_CPP_requested':3,'parking_cases':128,'parking_mismatches':0,'source_binding':str(OUT/'pre-replay-callers-v1/source-object-binding.json'),'report':str(OUT/'caller-boundaries-v2-parking.json')},
 'current_hard_gate_result':{'whole_CPP_passed':1,'whole_CPP_failed':2,'failing_gates_per_module_consumer':5,'total_layout_assertions':gate_count,'no_gate_disabled':True},
 'baseline':{'size':hex(old['types']['BrnWorld::RaceCarEntityModule']['size']),'parking':hex(old['offsets']['mPowerParkingManager']),'controls':hex(old['offsets']['mPlayerVehicleControls'])},
 'pre_replay_correction':{'parking':hex(new['offsets']['mPowerParkingManager']),'controls':hex(new['offsets']['mPlayerVehicleControls']),'mode':hex(new['offsets']['meGameModeType']),'mbInReplay':hex(new['offsets']['mbInReplay'])},
 'remaining_native_bounds':{k:{'candidate':hex(v),'ARTIST':hex(w)} for k,v,w in [
  ('serialiser_start',new['offsets']['mReplaySerialiser'],0x18600),('invalidated_byte',new['offsets']['mbInvalidatedRaceCars'],0x186C0),('rendered_array',new['offsets']['mabRaceCarsRendered'],0x186C1),('damage_override',new['offsets']['DEBUG_mbOverrideDamage'],0x18778),('module_size',new['types']['BrnWorld::RaceCarEntityModule']['size'],0x18800)]},
 'primary_inputs':primary,'artifacts':artifacts,
 'primary_gap':'No independent original Xbox derived serialiser declaration, sizeof/stride/allocation extent, or replay-matrix access distinguishing class/member alignment from a missing native member. Several hypotheses fit the observed range; no guessed alignment/padding/field accepted.',
 'limits':'Pre-replay pointer/register and controlled-call proof only; no full module, replay, Render, GPU, frame, hardware or gameplay claim. Current hard bounds intentionally reject the partial layout. CPP lifecycle/default state recovery beyond declarations is not claimed.'
 }
report['foreign_dependency']['local'] = git('rev-parse','HEAD^')
output = OUT/'CONTAINMENT_DIAGNOSTIC_CHECKPOINT.json'
assert not output.exists()
output.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'checkpoint':head,'report':str(output),'report_sha256':sha(output),'header_sha256':sha(header),'gates':gate_count},indent=2))
