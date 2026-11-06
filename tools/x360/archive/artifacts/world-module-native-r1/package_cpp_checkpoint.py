"""Seal the owned World CPP checkpoint without changing source or shared state."""
from pathlib import Path
import collections,hashlib,json,re,shutil,struct,subprocess,sys
OUT=Path(__file__).resolve().parent;ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/world-module-native-r1')
sys.path.insert(0,'D:/Reverse/BP-X360-Experimental-Artifacts/job-provider-526adc');from validate_startup_phase import F,Obj

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(p,obj):p.write_text(json.dumps(obj,indent=2)+'\n')
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip();status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT).decode();assert not status,status
owned='src/GameSource/World/BrnWorldModule.cpp';report=json.loads((OUT/'native-final/native/compile-results.json').read_text());row=report['results'][0];assert row['passed'];before=json.loads((OUT/'native-final/source-before.json').read_text())['hashes'];assert before[owned]==sha(ROOT/owned)
changed=subprocess.check_output(['git','show','--format=','--name-only',head],cwd=ROOT).decode().splitlines();assert changed==[owned],changed
message=subprocess.check_output(['git','show','-s','--format=%B',head],cwd=ROOT).decode();assert message.rstrip().endswith('Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>')
# Pin all selected final comparisons to the actual last whole-CPP object.
objsha=sha(row['object'])
for name in ['quadric-comparison.json','local-shading-comparison.json','game-actions-prefix-comparison.json']:
 r=json.loads((OUT/name).read_text());assert not r['mismatches'];assert r['object_sha256']==objsha,name
r=json.loads((OUT/'shader-publisher-comparison.json').read_text());assert not r['mismatches'];assert r['actual_provider_objects'][row['object']]==objsha
# Native instruction categories in the actual whole-CPP listing.
asm=Path(row['assembly']).read_text();counts={}
for prefix in ['?GenerateShaderConstantsForQuadricIrradiance@','?ClampColourToWhiteLevel@','?ScaleIrradiance@','?Construct@ShaderLodInfo@','?Update@ShaderLodInfo@']:
 names=re.findall(r'^('+re.escape(prefix)+r'\S*)\s+PROC',asm,re.M);assert len(names)==1,(prefix,names);name=names[0];start=asm.index(name+' PROC');end=asm.index(name+' ENDP',start);body=asm[start:end]
 ops=[x.group(1)for x in re.finditer(r'^\s+[0-9a-fA-F]{5}\s+[0-9a-fA-F]{8}\s+(\S+)',body,re.M)];counts[prefix]={'instructions':len(ops),'opcodes':dict(collections.Counter(ops))}
assert counts['?GenerateShaderConstantsForQuadricIrradiance@']['opcodes'].get('vmulfp128')==18
assert counts['?ScaleIrradiance@']['opcodes'].get('vmulfp128')==4
assert sum(counts['?ClampColourToWhiteLevel@']['opcodes'].get(k,0)for k in ['vminfp','vminfp128'])==1
save(OUT/'native-instruction-audit.json',{'object_sha256':objsha,'assembly_sha256':sha(row['assembly']),'functions':counts,'limit':'Selected instruction-category evidence, not full instruction matching or complete source parity.'})
# Primary original code and declaration evidence.
base=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX');original={}
for ea in [0x827b0790,0x827d1410,0x827d1ce8,0x827cf540,0x827c44d8,0x823bd4e0,0x82934620,0x822a0a20,0x822a0620]:
 src=base/('0x%08X.json'%ea);dst=OUT/('original-%08X.json'%ea)
 if not dst.exists():shutil.copyfile(src,dst)
 assert src.read_bytes()==dst.read_bytes();d=json.loads(src.read_text());addrs=[int(x,16)for x in re.findall(r'^0x([0-9a-fA-F]{8})\s',d['assembly'],re.M)];end=max(addrs)+4;raw=OUT/('original-%08X.bin'%ea);raw.write_bytes(F.M.V.x360rd.rd(ea,end-ea));original[hex(ea)]={'name':d['name'],'export_sha256':sha(dst),'range':[hex(ea),hex(end)],'code_sha256':sha(raw)}
rd=F.M.V.x360rd;assert rd.u32(0x82815570)==0x7c8a2378;assert rd.u32(0x823bd62c)==0x3c80005e and rd.u32(0x823bd634)==0x60843c80;assert rd.u32(0x82cd3cc0)==0x82c74120
mask=OUT/'original-CRT-82C74120.bin';mask.write_bytes(rd.rd(0x82c74120,0x248));save(OUT/'original-bindings.json',{'image_path':rd.RAW or rd.FLAT,'image_sha256':sha(rd.RAW or rd.FLAT),'reader':rd.__file__,'reader_sha256':sha(rd.__file__),'calibration':{'0x82815570':'0x7C8A2378'},'exports':original,'mask_initializer':{'start':'0x82C74120','end':'0x82C74368','crt_cell':'0x82CD3CC0','code_sha256':sha(mask)},'independent_World_size':{'source':'CheckClassSizes823BD4E0','instructions':['823BD62C:3C80005E','823BD634:60843C80'],'size':'0x5E3C80'},'native_scalar_constants':{hex(a):hex(rd.u32(a))for a in[0x82009e10,0x820ca5a8,0x82001d9c]}})
# Verify actual included files of the passing foreign setter providers remain exact.
providers=json.loads((OUT/'real-shading-providers/actual-inputs.json').read_text());providerbinding={}
for item in providers['results']:
 if not item['artifacts'].get('object'):continue
 if not Path(item['artifacts']['object']['path']).exists():continue
 assert item['source_sha256']==sha(ROOT/item['source'])
 for p,h in item['includes'].items():assert sha(p)==h,p
 providerbinding[item['source']]={'source_sha256':item['source_sha256'],'object':item['artifacts']['object']}
save(OUT/'provider-rebind.json',{'providers':providerbinding,'all_actual_includes_unchanged':True,'foreign_compile_failure':'CgsShaderConstants.cpp623 Vector4.x; see real-shading-providers/native/compile-results.json'})
refs=[('b9cb3fb5a50356654e73ca80986cc626f4b1b294','aeb809e9b'),('83808045743378684fa401392046db3c4fdf0a9a','4f2a81704'),('d8cd3cbcefed29e1239e1379600b47c5289a9709','7d9539653'),('8dde20af8836ca5550165e98ce6fc9f662ee89b6','5862f71b6'),('2743950dd6e32f2e28ea477b22e4de0df4b08a5e','cb2ee9b8f'),('4270295cc09b7fa750486892941631b043a0e09f','02396283d'),('7eadaebc1611d74869d528f0827ea56a3ab76a4f','2528dc239'),('3b855eeffea39464b735725b326b5e3a12dfbde5','ab0f90b2b'),('d17a5bb91249646782d39ad50235b080de54004b','e5999b06e')]
save(OUT/'dependency-adoption.json',{'base':'a26ca20f812a737975e02d92d12156958a50866b','owned_commit':head,'prerequisites':[{'source':r,'private_alias':subprocess.check_output(['git','rev-parse',a],cwd=ROOT).decode().strip()}for r,a in refs],'source_ownership':'Only the final CPP commit is owned. Prerequisite authors/trailers preserved.','ready_later_bodypart':'7a3e92d5bc70c629540bc1043fff40e6b094e509 and its separate geometry dependencies; not adopted here.'})
layout=json.loads((OUT/'actual-layout.json').read_text())['values'];actual=json.loads((OUT/'native-final/actual-inputs.json').read_text())
save(OUT/'checkpoint.json',{'state':'owned-cpp-ready-later-only','base':'a26ca20f812a737975e02d92d12156958a50866b','commit':head,'owned_path':owned,'source_sha256':sha(ROOT/owned),'object_sha256':objsha,'clean':True,'whole_CPP_native_pass':True,'frozen_native_inputs':len(before),'actual_distinct_included_headers':len(actual['unique_headers']),'provider_whole_CPP':{'passed':3,'failed':1},'comparisons':{'quadric_actual_function':256,'LOD_construct':256,'LOD_update':256,'full_lane_clamp':256,'full_row_scale':256,'shader_publisher_actual_setter_providers':128,'action_processing_prefix':256,'original_CRT_initializer':128,'mismatches':0},'native_World_layout_pass':False,'World_actual_size':hex(layout['world_size']),'World_independent_original_size':'0x5E3C80','World_layout_excess':'0x7900','layout_evidence':'actual-layout.json','limits':['Shader publisher and action prefix explicitly account for named World locations affected by the existing+7900 containment defect; they are not whole-World ABI proof.','Environment outputs, dispatch allocation, queue getter and assert continuation are controlled boundaries as each report states.','Case23 changes are assembly-reviewed, not dynamically proven through its full parameter/bridge path.','Unchanged algorithms in the large CPP remain unverified.','Physical floating exceptions, VSCR modes, concurrency, full frame/game, strict full link and launch are not claimed.','Actual CgsShaderConstants.cpp global provider still fails its existing Vector4.x access at623.','Original CRT mask execution is fixture evidence, not a newly supplied production SDK global provider.']})
print('SEALED',head,'CPP',sha(ROOT/owned),'OBJ',objsha,'headers',len(actual['unique_headers']))
print('Native opcode checks',json.dumps({k:v['opcodes'].get('vmulfp128',0)for k,v in counts.items()}))
