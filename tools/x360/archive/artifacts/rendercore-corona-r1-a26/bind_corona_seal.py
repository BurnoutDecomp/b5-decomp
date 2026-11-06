from pathlib import Path
import hashlib,json,os,subprocess,datetime
R=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-corona-r1-a26');O=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-corona-r1-a26');ENV=dict(os.environ,GIT_NO_LAZY_FETCH='1')
def git(*args):return subprocess.check_output(['git',*args],cwd=R,env=ENV).decode().strip()
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def pin(p):p=Path(p);return {'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size}
def put(n,v):
 p=O/n;assert not p.exists();p.write_text(json.dumps(v,indent=2,sort_keys=True)+'\n',encoding='utf-8');return pin(p)
BASE='a26ca20f812a737975e02d92d12156958a50866b';HEAD='0b82748bcfcfd348bab01a5c0326eefaee64aa80';R3='2b4c441f20fafbb1b061618fda4d6e023abe8336'
assert git('rev-parse','HEAD')==HEAD and not git('diff','--name-only') and not git('diff','--cached','--name-only')
assert git('rev-parse',HEAD+'^')==BASE
message=git('show','-s','--format=%B',HEAD);assert message.splitlines()[-1]=='Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>'
paths=git('diff-tree','--no-commit-id','--name-only','-r',HEAD).splitlines();assert len(paths)==17
blobs={}
def blob(commit,path):
 s=git('ls-tree',commit,'--',path)
 return s.split()[2] if s else None
for p in paths:blobs[p]={'source_sha256':sha(R/p),'R1_preimage':blob(BASE,p),'frozen_R3':blob(R3,p),'private_postimage':blob(HEAD,p)}
blob_receipt=put('source-blob-map.json',{'base':BASE,'commit':HEAD,'frozen_R3_for_planning_only':R3,'paths':blobs,'rule':'No foreign aliases were adopted. R3/Matrix union must retain disjoint changes and rerun actual source/COFF gates; this blob map does not approve a union.'})
reports=[]
for name,expected in [('final-core-providers-comparison.json',2376),('final-submission-providers-comparison.json',896),('final-constructor-comparison2.json',640),('final-table-crt-comparison.json',64),('final-atlas-crt-comparison.json',32)]:
 p=O/name;r=json.loads(p.read_text());errors=r.get('errors',r.get('final_state_errors'));assert errors==[]
 count=r.get('cases',sum(r['checks'].values()) if 'checks' in r else 0);assert count==expected,(name,count)
 if 'table-crt' in name:assert r['pre_CRT_image_matches'] and r['write_footprint_matches'] and r['original_writes']==r['native_writes']==80
 if 'atlas-crt' in name:assert r['write_footprint_matches'] and r['original_writes']==r['native_writes']==48
 reports.append({'report':pin(p),'cases':count,'scope':r['scope'],'limits':r.get('limits')})
assert sum(r['cases'] for r in reports)==4008
# Every claimed object/log/listing and command is pinned; the three moved sources
# remain actual inventory inputs although their implementation is now at proper homes.
stages=[]
for folder in ['native-final1','renderer-direct-consumer1']:
 p=O/folder/'compile-results.json';r=json.loads(p.read_text());assert r['state']=='complete';rows=[]
 for x in r['results']:
  entry={'source':x['source'],'source_sha256':sha(R/x['source']),'passed':x['passed'],'command':x['command'],'log':pin(x['log'])}
  if x['passed']:entry.update(object=pin(x['object']),assembly=pin(x['assembly']))
  rows.append(entry)
 stages.append({'report':pin(p),'results':rows})
assert sum(x['passed'] for x in stages[0]['results'])==10 and len(stages[0]['results'])==13
assert not stages[1]['results'][0]['passed']
compile_receipt=put('source-object-receipt.json',{'commit':HEAD,'stages':stages,'selected_total':14,'selected_pass':10,'selected_retained_fail':4,'stage_scope':'All eight core product CPPs, whole RendererModule and one genuine-header probe pass; three direct Effects CPPs and foreign CoronaRenderer retain baseline failures. This is not all2400 conservative header consumers.'})
baseline=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/compile-triage/repair-r1/r1-failures.json');b=json.loads(baseline.read_text());failed={x['source']:x for x in b['allR1FailedRows']};retained=[]
for st in stages:
 for x in st['results']:
  if not x['passed']:
   row=failed[x['source']];assert row['r1']=='fail';assert sha(row['log'])==row['logSha256'];retained.append({'source':x['source'],'baseline_R1_log':pin(row['log']),'current_log':x['log'],'baseline_error_families':row['errorFamilies'],'status':'Retained failure, not a newly claimed pass.'})
retained_receipt=put('retained-baseline-failures.json',{'baseline_inventory':pin(baseline),'failures':retained})
# Pin all current literal-include inputs and independently recheck the graph snapshot.
inventory=O/'consumer-inventory.json';inv=json.loads(inventory.read_text());assert inv['commit']==HEAD
for path,h in inv['source_header_hashes'].items():assert sha(R/path)==h,path
reference=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/vector_abi_sol/layers/v34_homes_v1/external-toolchain-inputs-v1.json');external=json.loads(reference.read_text());mismatch=[p for p,h in external['files'].items() if sha(p)!=h];assert not mismatch
external_receipt=put('external-inputs-bound.json',{'reference':pin(reference),'checked_files':len(external['files']),'mismatches':mismatch,'files':external['files'],'note':'Same actual SDK/compiler and canonical EABase/EASTL/EAThread roots used by the exact commands; project sources use this private tree only.'})
foreign_path=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra/render-rce-rebind-v4/actual-corona-chain-v3-comparison.json');foreign=json.loads(foreign_path.read_text());assert foreign['cases']==128 and foreign['count']==0
foreign_evidence={'receipt':pin(foreign_path),'scope':foreign['scope'],'objects':foreign['objects'],'ownership':'RaceCar owner independent combined RCE/shader/Matrix layer; not a dependency replay or full Render claim. Failed v1 and corrected v2/v3 remain in that owner directory.'}
source_contracts={
 'AddCorona_params':{'original':'823FD270','signature':'void(Vector3 value,Vector3 value,f32,f32,const BrnCoronaTypeParams&)','registers':'r3=this,v1/v2=raw vectors,f1/f2=scale/opacity,r6=params'},
 'AddCorona_enum':{'original':'823FD428','signature':'void(Vector3 value,Vector3 value,f32,f32,const BrnCoronaType&)','registers':'same vector/FPR entry,r6=enum pointer; unchecked table lookup'},
 'AddPropCorona':{'original':'823FD138','signature':'void(Vector3 value,Vector3 value,const FPU2&,f32,s32)','registers':'r3=this,v1/v2=raw vectors,r4=8-byte FPU pair,f1=alpha,r6=texture'},
 'CurveEvaluate':{'original':'8227D2F0','signature':'static f32(const FPU3&,const FPU2&,f32)','registers':'r3=12-byte triplet,r4=8-byte pair,f1=input/result'},
 'BufferDescriptor':{'original':'8228D6C0','signature':'static rw::ResourceDescriptor(Parameters&)','registers':'r3=hidden40-byte result,r4=params'},
 'BufferInitialize':{'original':'822850D8','signature':'static CoronaBuffer*(const rw::Resource&,Parameters&)','registers':'r3=20-byte resource,r4=params; result r3'},
 'IteratorWrite':{'original':'823F3350','signature':'void(Vector3 value,Vector3 value,Vector2 value,f32,rw::RGBA value,s32)','registers':'r3=iterator,v1/v2/v3=raw vectors,f1=distance,r5=ABGR,r6=texture; stored field is genuine RGBA8'},
 'layout':{'Corona':64,'BufferHeader':8,'Iterator':12,'Interface':112,'Manager':320,'TypeParams':48,'Curve':20,'FPU2':8,'FPU3':12,'VPU2':16}}
post=O/'POSTMORTEM.md';assert not post.exists();post.write_text('''The bounded native Corona submission/provider closure passes. The source checkpoint is0b82748bcfcfd348bab01a5c0326eefaee64aa80 on exact R1a26; no foreign source was adopted, and integration/publication remains with root.\n\nARTIST823FD270 and823FD428 consume by-value Vector3 values in v1/v2, f1/f2 scalars and r6 parameter/enum references. AddProp823FD138 has a packed FPU pair through r4. Genuine emitted callers and complete actual providers now use those contracts, including all128 vector bits, lazy color initialization, assertion metadata/continuation and callback-mutated output/type pointers. The input rw::RGBA remains ABGR and the record is the existing real RGBA8; the native conversion occurs in Iterator::Write. No type conflation or scalar/reference adapter was introduced.\n\nThe real Curve is20 bytes from FPU3(12)+FPU2(8), and a CoronaTypeParams record is48 bytes by natural VPU alignment. Explicit scalar copies from the SDK/DWARF and inline placement of the already-proven VPU2/RGBA constructors restore constant initialization. The actual1200-byte table image matches retail exactly, three genuine named curves match their original20-byte images, and the CRT produces the same80 disjoint address/width/value writes and final bytes. Table instruction order differs and is recorded; the pure initializer has no called service observation between these disjoint writes. Atlas startup has an exact zero image,48 vector writes in matching order and all768 resulting bytes. The earlier all-BSS/175-write source diagnostic is retained as a meaningful negative. No byte overlay, alternate record, fabricated pad or initializer shim was used.\n\nAll14 selected whole CPP/probe gates were run:10 pass, including every core provider, retained moved-body CPP and whole Renderer; four are existing R1 failures (three Effects callers plus CoronaRenderer). Their actual logs are bound and baseline status verified. Constructor and provider tests total4008 cases with zero errors. The independent RaceCar owner also binds128 actual first-strobe caller-to-four-real-provider cases, with prior transform/direction controlled; its failed zero-constant-page fixture and corrections remain explicit. core_probe is only generated typed caller/ctor evidence and supplies no controlled service or engine provider.\n\nThe shared-header consumer union is2400 conservative CPPs in a3616-input native inventory; it is an inventory rather than a complete preprocessor or whole-program retention gate. The integrated R3/Matrix source union must compile and recheck actual COFF/provider coverage. Generic by-value FPU APIs beyond these fields/constructors, full Manager lifecycle bodies (compiled/source recovered but not all executed here), CoronaRenderer/GPU/device semantics, hardware estimates, strict game link, boot and launch remain outside this seal. Existing lifecycle declaration/nominal-interface debt is not certified by the AddCorona proof. Root's global goal remains active.\n''',encoding='utf-8')
script_names=['corona_fixture_final.py','validate_core_final.py','validate_submission_final.py','validate_constructors_final2.py','validate_table_final.py','validate_atlas_final.py','run_final_retention.py','inventory_consumers.py']
negative_names=['core-providers-comparison.json','submission-providers-comparison.json','table-crt-phase-diagnostic.json','table-placement-diagnostic1.json','colour-placement-diagnostic1.json','constructor-fixture-incomplete.json']
manifest={
 'status':'PRIVATE_BOUNDED_CORE_CORONA_SEALED_WITH_RETAINED_BASELINE_DEBT',
 'created_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'base':BASE,'commit':HEAD,'worktree':str(R),'frozen_R3_preflight':R3,
 'minimal_original_graph':[BASE,HEAD],'new_foreign_source_dependencies':[],'foreign_alias_rule':'No private ancestry or aliases are replayed. Math/MaskScalar/Resource/state/RGBA8 providers already present in R1 are inherited prerequisites. Apply only this own source node to a later approved combined tree and bind the blob map/gates.',
 'paths':blobs,'new_CPPs':[],'new_headers':['src/SDKs/RenderEngineClub/MAIN/components/include/coronas/rwgcorona.h'],
 'moved_provider_homes':{'Vector2_float2':'existing class inline in vendor/rw/math/vpu/types.h; original CPP retained/gated','RGBA_four_channel':'existing class inline in rwcore_structs.h; original CPP retained/gated','CoronaTypeParams':'actual BrnCoronaManager.cpp/table CRT; former split CPP retained/gated; actual getter header-inline','Corona_and_Buffer':'real SDK corona headers; one shared definition, no nested descriptor facade'},
 'source_blob_map':blob_receipt,'source_object_receipt':compile_receipt,'baseline_failures':retained_receipt,'external_inputs':external_receipt,
 'consumer_inventory':pin(inventory),'conservative_consumer_union':len(inv['conservative_union']),'native_inventory_count':inv['native_inventory_count'],'source_header_inputs':len(inv['source_header_hashes']),
 'contracts':source_contracts,'proofs':reports,'own_comparison_cases':4008,'own_errors':0,
 'foreign_actual_RaceCar_caller':foreign_evidence,
 'validation_scripts':{n:pin(O/n) for n in script_names},'generated_probe_sources':{str(p.relative_to(R)):pin(p) for p in sorted((R/'scratch/corona-r1').glob('*.cpp'))},
 'negative_and_historical_receipts':{n:pin(O/n) for n in negative_names},'native_image':pin(O/'native-table-image-positive.json'),'initialized_table':pin(O/'native-initialized-table.bin'),'postmortem':pin(post),
 'limits':['Four retained R1 compiler failures remain and were not hidden by source exclusion.','All2400 conservative consumers and a combined R3/Matrix source tree require the root/integrator gate.','Whole Manager lifecycle source recovery/compile does not claim every lifecycle body was behaviorally executed.','CoronaRenderer/GPU/unknown foreign state and strict whole-game link/boot/launch remain open.','PPC/VMX/FPU/NaN arithmetic is modeled; no physical hardware claim.','Curve ctor128 is an original field-copy-window proof, not a standalone retail ctor or blanket generic FPU by-value ABI approval.'],
 'trailers_verified':message.splitlines()[-2:],'tracked_clean':True}
receipt=put('CORE_CORONA_PRIVATE_HANDOFF.json',manifest)
resume=O/'CHECKPOINT_RESUME.md';assert not resume.exists();resume.write_text('''Read CORE_CORONA_PRIVATE_HANDOFF.json first. Source is immutable private0b82748bcfcfd348bab01a5c0326eefaee64aa80 on R1a26 in rendercore-corona-r1-a26,17 owned paths, tracked-clean plus scratch probes. All final provider/constructor/table/atlas comparisons have completed,4008 cases zero errors; no live owned job remains. Root owns any R4 collection after preflight; no canonical/intake/main/dev/ledger operation was performed.\n\nTable closure is real: exact1200-byte retail image,80-write footprint and final bytes; original named curves and inline SDK constructor/scalar copy homes explain it. Atlas48 writes/order/bytes also match. Preserve negative BSS/175-write evidence and the incomplete fixture-sentinel report. core_probe contains generated callers/ctor probes only; actual direct provider and RaceCar v3 real-object chains do not use it.\n\nFour baseline compiler failures and the2400-consumer full-source gate remain. No global Math/Matrix, Manager lifecycle, GPU/device, strict-link or launch approval is implied. If continuing these broader methods, request exact scope/ownership first; the current bounded author deliverable is sealed.\n''',encoding='utf-8')
seal=put('SEAL.json',{'handoff':receipt,'resume':pin(resume),'commit':HEAD,'tracked_status':git('status','--short'),'own_comparisons':4008,'selected_gate':'10PASS/4 retainedFAIL; full consumer gate pending','status':'PRIVATE_BOUNDED_SEAL_ROOT_INTEGRATION_REQUIRED'})
print(json.dumps({'handoff':receipt,'seal':seal,'paths':len(paths),'cases':4008,'consumer_union':len(inv['conservative_union'])},indent=2))
