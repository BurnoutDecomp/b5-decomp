import json,re,hashlib,collections
from pathlib import Path
base=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts')
w6=base/'wave6'
raw=w6/'full-stage1/compile-results.json'
prev_raw=base/'wave5-full/compile-results.json'
prev_eff=base/'wave5-delivery/native-parallel-wave5-final-effective.json'
summary=json.loads((w6/'triage_summary.json').read_text(encoding='utf-8'))
cur=json.loads(raw.read_text(encoding='utf-8'))
old=json.loads(prev_eff.read_text(encoding='utf-8'))
oldraw=json.loads(prev_raw.read_text(encoding='utf-8'))

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def statuses(r):return {x['source']:bool(x['passed']) for x in r['results']}
a,b=statuses(old),statuses(cur)
trans={
 'previous_effective_pass_to_fail':sorted(s for s in a.keys()&b.keys() if a[s] and not b[s]),
 'previous_effective_fail_to_pass':sorted(s for s in a.keys()&b.keys() if not a[s] and b[s]),
 'new_source_pass':sorted(s for s in b.keys()-a.keys() if b[s]),
 'new_source_fail':sorted(s for s in b.keys()-a.keys() if not b[s]),
 'failed_in_both':sorted(s for s in a.keys()&b.keys() if not a[s] and not b[s]),
}
regressions={
 'src/GameShared/GameClasses/RenderWare/cross/CgsMaterialTechniqueResourceType.cpp':{'owner':'rendercore_sol','cause':'old static two-argument BlendState::GetParameters call; native API is the recovered const member form','log':str(w6/'full-stage1/CgsMaterialTechniqueResourceType-1c2ec44f.log'),'line':193},
 'src/GameSource/Effects/Particles/Native/BrnLionBlendRenderer.cpp':{'owner':'rendercore_sol','cause':'obsolete PS3 stateparams include / BlendMaterialState forward conflicts with recovered native classes','log':str(w6/'full-stage1/BrnLionBlendRenderer-fb279e1c.log'),'line':39},
}
layout={
 'src/GameSource/Game/BrnGameModule.cpp':{'owner':'boot_resume_sol','guard':'sizeof(BrnGameModule)==0x9A1300 and offsets at mGameStateModule 0x669500, mGuiModule 0x6EAA20, mNetworkModule 0x8C3600, mInputModule 0x6E9630','log':str(w6/'full-stage1/BrnGameModule-f766a093.log'),'line_range':'3132-3136','c2118_count':5},
 'src/GameSource/Main/BrnMainX360.cpp':{'owner':'boot_resume_sol','guard':'sizeof(BrnGameModule)==0x9A1300 entry-size guard','log':str(w6/'full-stage1/BrnMainX360-9a3a4c27.log'),'line':17,'c2118_count':1},
 'src/GameSource/GameState/Offences/BrnDriveThruManager.cpp':{'owner':'not listed in current active ownership summary; owner confirmation needed','guard':'offsetof(mpProgressionManager)+sizeof(void*)==sizeof(DriveThruManager), final-member assertion','log':str(w6/'full-stage1/BrnDriveThruManager-77f50479.log'),'line':277,'c2118_count':1},
}
unevaluable=[
 {'source':'src/GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.cpp','log':str(w6/'full-stage1/BrnReplayGPUDiskWriteStream-e4571'.replace('e4571','e4575')+'1e1a') if False else str(w6/'full-stage1/BrnReplayGPUDiskWriteStream-e4571'.replace('e4571','e4575')),'reason':'CGS_STATIC_ASSERT over maLock and CRITICAL_SECTION is not evaluable in this compile; log reports maLock/sizeof operand errors. Not counted as a proven storage-size mismatch.'},
 {'source':'src/SDKs/Packages/ICE/ICECameraSpaceHandlerCtor.cpp','log':str(w6/'full-stage1/ICECameraSpaceHandlerCtor-a1e5cc0e.log'),'reason':'trivially-copyable trait assertion rejected/unevaluable under this XDK compile; not proof of an actual layout-size mismatch.'},
]
# Explicit current primary diagnostic partition, one bucket per failed TU; later diagnostics remain in full summary JSON.
primary_rules=[
 ('known wave6 regression', lambda s,e: s in regressions),
 ('hard source-layout gate', lambda s,e: s in layout),
 ('layout/trait guard unresolved, not proven mismatch', lambda s,e: s in {'src/GameSource/Replays/Stream/BrnReplayGPUDiskWriteStream.cpp','src/SDKs/Packages/ICE/ICECameraSpaceHandlerCtor.cpp'}),
 ('missing / unsupported include', lambda s,e: e.get('code')=='C1083'),
 ('CRT / standard-library API absent in XDK', lambda s,e: bool(re.search(r"not a member of 'std'|not found in namespace 'std'",e.get('message',''),re.I))),
 ('C++03 syntax / unsupported language feature', lambda s,e: e.get('code') in {'C2059','C2143','C2146','C2147','C2153','C2334','C2447','C2552','C2601','C2466','C2550','C3646','C3409'} or bool(re.search(r'initializer list|syntax error|illegal use of this type|local function definitions|unknown override specifier|noreturn',e.get('message',''),re.I))),
 ('native type / provider closure', lambda s,e: bool(re.search(r"not a class or namespace|undefined type|incomplete type|not a member of .renderengine|not a member of .Cgs|unknown size",e.get('message',''),re.I)) or e.get('code') in {'C2027','C2036','C2070','C2079','C2061','C4430','C2653'}),
 ('signature / redeclaration / duplicate conflict', lambda s,e: e.get('code') in {'C2660','C2666','C2733','C2511','C2084','C2352','C2370','C2371','C2374','C2086','C2614','C2761','C2885'}),
 ('math / vector API or ABI spelling', lambda s,e: bool(re.search(r"\bfma\b|\bfmaf\b|fpclassify|copysign|signbit|nearbyint|isfinite|\bvmx\b|\bVector[234]\b|Matrix44Affine|Quaternion|mV\.",e.get('message',''),re.I)) or e.get('code')=='C2872'),
 ('missing identifier / helper / call target', lambda s,e: e.get('code') in {'C2065','C3861','C2064'}),
 ('constant width / native integer literal', lambda s,e: e.get('code')=='C2177'),
 ('typed expression / aggregate / member access', lambda s,e: e.get('code') in {'C2228','C2227','C2275','C2440','C2562'}),
]
buckets=collections.defaultdict(list)
for row in summary['failed_rows']:
 e=row['diagnostics'][0] if row['diagnostics'] else {'code':'','message':''}
 s=row['source']; chosen=next((name for name,fn in primary_rules if fn(s,e)),'other compiler diagnostic')
 buckets[chosen].append(row)
partition=[{'family':name,'failed_tu_count':len(rows),'example_sources':[r['source'] for r in rows[:6]],'all_sources':[r['source'] for r in rows]} for name,rows in sorted(buckets.items(),key=lambda kv:(-len(kv[1]),kv[0]))]
# Symptom families are overlapping counts across all parsed log lines/TUs, not a separate partition.
patterns={
 'std::fma unavailable':r"'fma' : is not a member of 'std'|'fma': identifier not found",
 'std::fmaf unavailable':r"'fmaf' : is not a member of 'std'|'fmaf': identifier not found",
 'finite/sign math APIs absent':r'fpclassify|copysign|signbit|nearbyint|isfinite',
 'std::snprintf unavailable':r"'snprintf' : is not a member of 'std'|'snprintf': identifier not found",
 'vector/matrix name or member errors':r"Vector[234]|Matrix44Affine|Vector3Plus|VecFloat|\.mV\.|ambiguous symbol",
 'VMX helper/namespace errors':r"\bvmx\b|VmxMadd|MultiplyAdd|NegativeMultiplySubtract",
 'renderengine provider/type errors':r'renderengine::|renderengine.*not a class or namespace|not a member of .renderengine',
}
symptoms=[]
for name,pat in patterns.items():
 affected=[]; occurrences=0
 for row in summary['failed_rows']:
  hits=[e for e in row['diagnostics'] if re.search(pat,e['message'],re.I)]
  if hits: affected.append(row['source']); occurrences+=len(hits)
 symptoms.append({'family':name,'affected_tus':len(set(affected)),'diagnostic_occurrences':occurrences})
# Shared header root counts by unique TU.
headers=collections.defaultdict(set)
for row in summary['failed_rows']:
 for e in row['diagnostics']:
  if e['location'].lower().endswith(('.h','.hpp','.inl')): headers[e['location']].add(row['source'])
shared_headers=[{'path':p,'affected_tus':len(ss),'sources':sorted(ss)} for p,ss in sorted(headers.items(),key=lambda kv:(-len(kv[1]),kv[0]))]
# Candidate whole-CPP opportunities.
opportunities=[
 {'priority':1,'owner':'unowned in current active ownership summary (no InputPads path listed)','source':'src/GameShared/GameClasses/System/Input/CgsInputPads.cpp','log':str(w6/'full-stage1/CgsInputPads-e51bbd0e.log'),'raw_errors':[{'line':255,'code':'C2039/C3861','message':'std::fmaf is not a member of std / fmaf identifier not found'},{'line':268,'code':'C2039/C3861','message':'std::fmaf is not a member of std / fmaf identifier not found'}],'error_occurrence_count':2,'whole_cpp_error_family':'only these two std::fmaf call sites in the raw log','artist_exports':[{'address':'0x828E7618','name':'CgsInput::InputPads::UpdateJoltEnvelope','evidence':'fmadds at 0x828E76A0 and 0x828E76EC, preserving the two f32 fused operations'}],'next_action':'A bounded CPP-only recovery can retain both original fmadds and argument/order semantics using an XDK-supported native spelling; inspect the exact assembly before choosing that spelling. No helper or API expansion is implied.'},
 {'priority':2,'owner':'unowned in current active ownership summary (no PerfMon GPU component path listed)','source':'src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp','log':str(w6/'full-stage1/CgsDebugComponentPerfMonGpu-'.__str__()+'log') if False else str(next(Path(w6/'full-stage1').glob('CgsDebugComponentPerfMonGpu-*.log'))),'raw_errors':[{'line':116,'code':'C2039/C3861','message':'std::fma is not a member of std / fma identifier not found'},{'line':148,'code':'C2039/C3861','message':'std::fma is not a member of std / fma identifier not found'},{'line':176,'code':'C2039/C3861','message':'std::fma is not a member of std / fma identifier not found'}],'error_occurrence_count':3,'whole_cpp_error_family':'only these three std::fma call sites in the raw log','artist_exports':[{'address':'0x8282CDF0','name':'CgsDev::DebugComponentPerfMonGpu::RenderPerformanceTable','evidence':'fnmsubs at 0x8282CE58 corresponds to the negative product plus top expression at source line 116'},{'address':'0x8282CEA0','name':'CgsDev::DebugComponentPerfMonGpu::RenderPerformanceGraph','evidence':'fmadds at 0x8282CF7C and 0x8282D184 for the source line 148/176 graph math'}],'next_action':'A bounded CPP-only recovery can preserve the original scalar fused operation order; compare each expression to the listed PPC opcode before changing spelling. No common math wrapper is established by this triage.'}
]
obj={
 'task':'read-only wave6 full-stage1 native failure triage',
 'frozen_head':'526adc27662c0c2aedf417980dcf824c989bb035',
 'current_report':{'path':str(raw),'sha256':sha(raw),'requested':cur['requested'],'passed':cur['passed'],'failed':cur['failed'],'state':cur['state']},
 'baseline_reports':[
  {'path':str(prev_eff),'sha256':sha(prev_eff),'requested':old['requested'],'passed':old['passed'],'failed':old['failed'],'kind':'published effective baseline'},
  {'path':str(prev_raw),'sha256':sha(prev_raw),'requested':oldraw['requested'],'passed':oldraw['passed'],'failed':oldraw['failed'],'kind':'previous raw full sweep'}
 ],
 'delta_from_effective_baseline':{k:len(v) for k,v in trans.items()},
 'regressions':regressions,
 'primary_diagnostic_partition':{'basis':'one category per failing TU using the first parsed compiler error; later/cascade diagnostics are preserved in raw-diagnostic summary','sum':sum(x['failed_tu_count'] for x in partition),'buckets':partition},
 'overlapping_symptom_family_counts':symptoms,
 'shared_header_roots':shared_headers,
 'true_layout_gates':layout,
 'unevaluable_layout_trait_guards':unevaluable,
 'unowned_cpp_only_opportunities':opportunities,
 'raw_diagnostics_artifact':str(w6/'triage_summary.json'),
 'raw_diagnostics_sha256':sha(w6/'triage_summary.json'),
 'compile_only_limit':'No link, executable, or launch claim; report is compile-only and frozen at 526adc.'
}
md= f'''# Wave6 full-stage1 failure triage

Read-only analysis of frozen head `{obj['frozen_head']}`. The terminal compile report is `{raw}` with SHA-256 `{obj['current_report']['sha256']}`: **{cur['passed']}/{cur['requested']} pass; {cur['failed']} fail**. This is compile-only evidence; no link or launch claim.

## Change since wave5

- Published effective baseline: {old['passed']}/{old['requested']} pass, {old['failed']} fail (`{sha(prev_eff)}`).
- Effective-pass regressions: **2**, both already routed to `rendercore_sol` and recorded below.
- Old failures recovered: **29**. Three newly inventoried CPPs were added and all three pass. Remaining: **240 old failure rows + 2 regressions**.
- Against the previous raw full sweep (3335/3610), 35 failed rows recover, the same 2 regressions appear, and 3 new CPPs pass.

## Primary first-diagnostic partition (242 TUs)

Counts use the first raw error per TU, one bucket per failure; cascade errors and all individual raw compiler lines remain in [triage_summary.json](D:/Reverse/BP-X360-Experimental-Artifacts/wave6/triage_summary.json).

| Primary cause family | TUs |
|---|---:|
'''
for b in partition: md+=f"| {b['family']} | {b['failed_tu_count']} |\n"
md+='''
These buckets summarize the first diagnostic, not a claim that all later errors in a row share that root.

## Hard gates and regressions

- **True layout gates (3 TUs):** `BrnGameModule.cpp:3132-3136` has five failed native size/offset checks; `BrnMainX360.cpp:17` repeats the 0x9A1300 entry-size guard. These are assigned to `boot_resume_sol`. `BrnDriveThruManager.cpp:277` fails the native last-member constraint and needs an ARTIST-backed class-tail check; current owner not listed.
- **Not a proven layout mismatch:** `BrnReplayGPUDiskWriteStream.cpp` cannot evaluate its `maLock`/`CRITICAL_SECTION` storage assertion from the current declarations; `ICECameraSpaceHandlerCtor.cpp` hits a `CameraSpaceHandler` trivially-copyable trait assertion unsupported/unresolved in this compile. Five additional `sizeof` diagnostics are on unknown/incomplete types; they are not evidence that the layout values themselves are wrong.
- **Known regressions from published passes:** `CgsMaterialTechniqueResourceType.cpp:193` still uses the stale static two-argument BlendState API; `BrnLionBlendRenderer.cpp:39` conflicts with native state declarations through an obsolete PS3 header/forward. Both are owned by `rendercore_sol`, and the post-terminal queue records the repairs.

## Shared and repeated failure roots

- Standard math/API spellings are broad: `std::fma` errors affect 27 TUs, `std::fmaf` 11, finite/sign functions 13, and `std::snprintf` 17. These counts overlap. The XDK errors do not authorize replacing fused arithmetic with multiply-plus-add; each changed site needs its ARTIST operation order.
- 38 TUs report Vector/Matrix name/member errors; the vector ABI lane is owned by `vector_abi_sol`.
- 19 TUs include missing/incomplete RenderEngine type/provider diagnostics. Repeated native state roots include `CgsRasterizerStateFactory.h` (`RasterizerState`, 2 consumers) and `CgsDepthStencilStateFactory.h` (`DepthStencilState`, `BrnSunCorona.cpp`). RenderEngine provider headers are already active-owner territory; do not duplicate those repairs.
- `VolumeQuery.cpp` is one large collision TU with a clustered missing `VolumeLineSegIntersectResult` definition, absent `VolumeQueryHostLayout` constants/helpers, and unavailable `std::snprintf` spelling. It is a real collision dependency closure, not a single type error; see its raw log before assignment.

## Small unowned whole-CPP opportunities

1. **Input Jolt envelope:** `src/GameShared/GameClasses/System/Input/CgsInputPads.cpp:255,268`; raw log `{opportunities[0]['log']}`. Only errors are two `std::fmaf` call sites. Consult ARTIST `CgsInput::InputPads::UpdateJoltEnvelope @ 0x828E7618`; `fmadds` occurs at `0x828E76A0` and `0x828E76EC`. A faithful whole-CPP fix can retain these single-precision fused expressions with a native-supported spelling; preserve arguments/order.
2. **GPU PerfMon math:** `src/GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.cpp:116,148,176`; raw log `{opportunities[1]['log']}`. Only errors are three `std::fma` expressions. Consult ARTIST `RenderPerformanceTable @ 0x8282CDF0` (`fnmsubs` at `0x8282CE58`) and `RenderPerformanceGraph @ 0x8282CEA0` (`fmadds` at `0x8282CF7C` and `0x8282D184`). Preserve each fused scalar operation/order. Neither file is listed in the current active ownership summary.

Priority: Boot's GameModule/entry layout gates remain the only identified hard launch-chain gates and are already assigned to Boot. The two leaf CPPs above are small unowned compile gains; they are not launch proof.

## Evidence files

- Parsed raw diagnostics for all 242 failed TUs, including full native error lines and canonical log paths: `{w6/'triage_summary.json'}`.
- Machine-readable counts/queue: `{w6/'triage_inventory.json'}`.
- The original compile report and prior baseline reports remain unchanged.
'''
# create output only after verifying absent
outdir=w6/'current-failure-triage-luna'
if outdir.exists(): raise SystemExit(f'output directory already exists: {outdir}')
outdir.mkdir()
(outdir/'triage_inventory.json').write_text(json.dumps(obj,indent=2)+'\n',encoding='utf-8')
(outdir/'TRIAGE.md').write_text(md,encoding='utf-8')
print(json.dumps({'report_sha256':obj['current_report']['sha256'],'rows':cur['requested'],'pass':cur['passed'],'fail':cur['failed'],'primary_buckets':[(x['family'],x['failed_tu_count']) for x in partition],'output_dir':str(outdir)},indent=2))
