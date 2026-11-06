"""Extract the actual frozen Main layout, preserving all production layout guards."""
from pathlib import Path
import hashlib,json,re,struct,subprocess
OUT=Path(__file__).resolve().parent
ROOT=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
NOTES=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/parallel-native/native_boot_entry')
ORIG=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def save(n,d):(OUT/n).write_text(json.dumps(d,indent=2)+'\n',encoding='utf-8')
def hx(n):return '0x%X'%n
r=json.loads((OUT/'compile-results.json').read_text())
assert r['passed']==1 and not r['whole_main_compiled'] and not r['production_guards_modified']
obj=Path(r['result']['object']);assert sha(obj)==r['result']['object_sha256']
b=obj.read_bytes();machine,nsect,_,ptr,nsy,opt,_=struct.unpack_from('<HHIIIHH',b)
assert machine==0x1f2
stp=ptr+nsy*18;st=b[stp:stp+struct.unpack_from('<I',b,stp)[0]]
names={'native_game_layout':94,'native_game_containment':13,'native_game_contained_size_align':22}
arrays={};i=0
while i<nsy:
 name,value,sect,ty,storage,aux=struct.unpack_from('<8sIhHBB',b,ptr+i*18)
 if name[:4]==b'\0'*4:name=st[struct.unpack_from('<I',name,4)[0]:].split(b'\0',1)[0]
 name=name.split(b'\0',1)[0].decode(errors='replace')
 if name in names:
  assert sect>0
  raw=struct.unpack_from('<I',b,20+opt+(sect-1)*40+20)[0]
  arrays[name]=list(struct.unpack_from('>'+names[name]*'I',b,raw+value))
 i+=1+aux
assert arrays.keys()==names.keys()
for name,value in arrays.items():
 old=json.loads((OUT/(name+'.json')).read_text());assert old==value
layout,contain,sizes=[arrays[k]for k in names]
e=json.loads((NOTES/'game-tail-expected.json').read_text());assert len(e['fields'])==90
base=layout[4];tail=[]
for (name,native),actual in zip(e['fields'],layout[4:]):
 tail.append({'field':name,'actual':actual,'native':native,'actual_hex':hx(actual),'native_hex':hx(native),'delta':actual-native,'actual_relative':actual-base,'native_relative':native-e['base'],'relative_passes':actual-base==native-e['base']})
assert len(tail)==90 and all(x['relative_passes'] for x in tail)
q=json.loads((NOTES/'GAME_LAYOUT_QUEUE.json').read_text())
renderer={'type':'Renderer','qualified_type':'BrnRendererModule','header':'src/GameSource/Render/BrnRendererModule.h','native_member':'0x4400','next_member':'0x10E80','native_sizeof':None,'sizeof_evidence':'Native neighboring-member span only in this evidence set; do not promote to independent sizeof.'}
modules=[]
member_names=['mRenderModule','mWorldModule','mGameDataModule','mGameStateModule','mDirectorModule','mInputModule','mGuiModule','mEffectsModule','mSoundModule','mReplayModule','mNetworkModule']
for i,t in enumerate([renderer,*q['types']]):
 actual=contain[i+2];native=int(t['native_member'],16)
 after=contain[i+3]if i+3<len(contain)else base
 native_after=int(t['next_member'],16);size=sizes[2*i];align=sizes[2*i+1]
 modules.append({'type':t['type'],'qualified_type':t.get('qualified_type'),'member':member_names[i],'actual_size':size,'actual_size_hex':hx(size),'actual_alignment':align,'actual_alignment_hex':hx(align),'actual_offset':actual,'actual_offset_hex':hx(actual),'native_offset':native,'native_offset_hex':hx(native),'offset_delta':actual-native,'actual_neighbor_span':after-actual,'actual_neighbor_span_hex':hx(after-actual),'native_neighbor_span':native_after-native,'native_neighbor_span_hex':hx(native_after-native),'span_delta':after-actual-(native_after-native),'independent_native_sizeof':t.get('native_sizeof'),'independent_native_sizeof_hex':hx(t['native_sizeof'])if t.get('native_sizeof')is not None else None,'sizeof_delta':size-t['native_sizeof']if t.get('native_sizeof')is not None else None,'sizeof_evidence':t['sizeof_evidence'],'sizeof_instruction_evidence':t.get('sizeof_instruction_evidence',[])})
by_member={x['member']:x for x in modules}
gates=[{'name':'sizeof(BrnGameModule)','actual':layout[0],'native':0x9a1300}]
for name,native in [('mGameStateModule',0x669500),('mGuiModule',0x6eaa20),('mNetworkModule',0x8c3600),('mInputModule',0x6e9630)]:gates.append({'name':'offsetof('+name+')','actual':by_member[name]['actual_offset'],'native':native})
for g in gates:g.update(actual_hex=hx(g['actual']),native_hex=hx(g['native']),delta=g['actual']-g['native'],passes=g['actual']==g['native'])
assert not any(g['passes']for g in gates)
assert [m['type']for m in modules if m['offset_delta']][:1]==['GameData']
originals=[]
for source,dest in [(ORIG/'0x827E5F00.json','original-constructor-827E5F00.json'),(NOTES/'game-check-class-sizes-original.json','original-check-class-sizes-823BD4E0.json'),(NOTES/'game-tail-expected.json','original-tail-expected.json')]:
 (OUT/dest).write_bytes(source.read_bytes());originals.append({'source':str(source),'copy':str(OUT/dest),'sha256':sha(source)})
ctor=json.loads((OUT/'original-constructor-827E5F00.json').read_text())
a=ctor['assembly'].splitlines()
windows={}
for member,start,end in [('World',0x827e5f80,0x827e5f88),('GameData',0x827e5f8c,0x827e5f94)]:
 windows[member]=[line for line in a if start<=int(line.split()[0],16)<=end]
guard_sources=[]
for relative,pattern in [('src/GameSource/Game/BrnGameModule.cpp','typedef char Native'),('src/GameSource/Main/BrnMainX360.cpp','BrnMain_requires_original_game_module_size')]:
 p=ROOT/relative;lines=p.read_text(encoding='utf-8').splitlines();matches=[]
 for i,line in enumerate(lines):
  if pattern in line:matches.append({'line':i+1,'text':line,'following':lines[i+1:i+2]})
 guard_sources.append({'path':relative,'sha256':sha(p),'guard_lines':matches})
save('guard-source-evidence.json',guard_sources)
before=json.loads((OUT/'frozen-source-before.json').read_text());after=json.loads((OUT/'frozen-source-after.json').read_text())
assert before['hashes']==after['hashes']and len(before['hashes'])==6277
canonical_hash=hashlib.sha256(json.dumps(before['hashes'],sort_keys=True,separators=(',',':')).encode()).hexdigest()
inputs=json.loads((OUT/'actual-inputs.json').read_text());assert inputs['header_count']==1398
input_changed=[p for p,want in inputs['probe_and_unique_includes'].items()if sha(p)!=want]
assert not input_changed,input_changed
inputs_verified={'actual_headers':1398,'all_actual_input_hashes_match_at_packaging':True,'frozen_source_count':6277,'source_before_after_equal':True,'source_hashes_sorted_compact_json_sha256':canonical_hash}
save('packaging-input-verification.json',inputs_verified)
result={'head':r['head'],'measurement':'One actual-header XDK compile; big-endian u32 constants extracted from real COFF01F2. No synthetic class, source fallback, private bf3/VPU input, guard replacement or field changes.','main_size':layout[0],'main_size_hex':hx(layout[0]),'main_alignment':layout[1],'native_main_size':0x9a1300,'total_delta':layout[0]-0x9a1300,'tail_base':base,'tail_base_hex':hx(base),'native_tail_base':e['base'],'tail_delta':base-e['base'],'tail_anchor_count':len(tail),'relative_tail_mismatches':[],'metrics_sample_size_alignment':layout[2:4],'hard_gates':gates,'contained_modules':modules,'tail_anchors':tail,'original_evidence':originals,'first_mismatch':{'member':'mGameDataModule','actual_offset':0x5fc400,'native_offset':0x5f4b00,'preceding_member':'mWorldModule','preceding_actual_offset':0x10e80,'preceding_native_offset':0x10e80,'world_actual_size':0x5eb580,'world_independent_native_size':0x5e3c80,'world_excess':0x7900,'constructor_windows':windows,'native_size_instructions':by_member['mWorldModule']['sizeof_instruction_evidence']},'next_dependency':'Recover the genuine WorldModule containment from independently attested sizeof5E3C80 and following GameData boundary5F4B00; current World starts correctly at10E80 and is7900 too large. Director/GUI/Replay have additional independent span/size drift; they cannot compensate World.','binding':inputs_verified,'guard_source_evidence':str(OUT/'guard-source-evidence.json'),'command':str(OUT/'command.json'),'object':str(obj),'limits':'No whole Main/entry compile, full link, image or launch claim. Renderer/Director/Replay native sizes remain neighbor-span-only in this evidence set. Native alignment values are current compiled values; do not infer all original alignments from spans.'}
save('MAIN_LAYOUT_526ADC.json',result)
lines=['Frozen 526adc actual Main layout measurement','','Exact head: `'+r['head']+'`. This is a successful actual-header probe, not a whole Main or entry compile. All five production guards remain unchanged. One real portable XDK compiler, private D: TEMP and separate D: output were used.','','| Guard | Actual | Native required | Delta |','|---|---:|---:|---:|']
for g in gates:lines.append('| '+g['name']+' | '+g['actual_hex']+' | '+g['native_hex']+' | +0x%X |'%g['delta'])
lines+=['','Main alignment is 0x80. Tail starts 0x99FA00 versus native 0x996F00: +0x8B00. All 90 relative tail anchors match; absolute offsets retain that displacement. RenderMetricsSample is 0x80 / align 0x10.','','| Module | Actual sizeof / align | Actual Main offset | Native Main offset | Independent native sizeof | Native neighbor span |','|---|---:|---:|---:|---:|---:|']
for m in modules:lines.append('| '+m['type']+' | '+m['actual_size_hex']+' / '+m['actual_alignment_hex']+' | '+m['actual_offset_hex']+' | '+m['native_offset_hex']+' | '+(m['independent_native_sizeof_hex']or'unproven')+' | '+m['native_neighbor_span_hex']+' |')
lines+=['','First measured boundary mismatch: GameData 0x5FC400 versus native 0x5F4B00. Renderer starts 0x4400 and World 0x10E80 correctly. World actual sizeof 0x5EB580 exceeds independent native 0x5E3C80 by 0x7900. ARTIST constructor 827E5F80..94 forms both native addresses; CheckClassSizes 823BD62C/34 loads the true World size. These exact instructions are retained in MAIN_LAYOUT_526ADC.json and original JSON copies.','','Further drift: Director current neighbor span 0x393B0 versus native 0x38B20 (+0x890); GUI true size 0x18E6F0 versus 0x18DCD0 (+0xA20), with actual next-member span 0x18E750 versus 0x18DCE0 (+0xA70) because the following 128-byte alignment phase adds another0x50; Replay span 0x6200 versus 0x6300 (-0x100). Sum: 0x7900+0x890+0xA70-0x100=0x8B00. These are diagnoses, not padding prescriptions. Input true size 0x13E8 has a 0x13F0 neighbor span. Renderer/Director/Replay original sizeof are not independently established by this evidence set.','','The next dependency is genuine World containment recovery against true 0x5E3C80 and native following GameData boundary 0x5F4B00. No conclusion about which World child causes the excess is made from Main spans alone.','','Binding: all 6,277 frozen source/header hashes match before/after; 1,398 actual included headers are captured by /showIncludes and rechecked at packaging. command.json records the complete invocation and compiler environment, actual-inputs.json the actual resolved closure and SDK tool hashes. The existing native-game-layout-probe.cpp is byte-identical to the prior actual-header probe; include search resolves only frozen 526adc src, permitted vendor roots and SDK. It exposes access only, without changing layout or guards.','','Reproduce using run_probe.py only in a fresh uniquely named output directory after reviewing its fixed paths. extract_layout.py decodes the immutable object directly and can be rerun without invoking the compiler. No new build is needed to inspect this report.','','Source-only work; no main/dev/ledger/intake mutation, whole Main compile, full game link, image or launch.']
(OUT/'MAIN_LAYOUT_526ADC.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
files=[p for p in OUT.iterdir()if p.is_file()and p.name!='artifact-manifest.json']
save('artifact-manifest.json',{'head':r['head'],'artifacts':{p.name:{'sha256':sha(p),'size':p.stat().st_size}for p in sorted(files)},'frozen_source_hashes_canonical_sha256':canonical_hash,'input_verification':inputs_verified})
print('PACKAGED',r['head'],'Main',hx(layout[0]),'tail',hx(base),'relative anchors90/90; first mismatch GameData +7900')
for n in ('command.json','actual-inputs.json','native-game-layout-probe.cpp','native-game-layout-probe.obj','frozen-source-before.json','MAIN_LAYOUT_526ADC.json','artifact-manifest.json'):print(n,sha(OUT/n))
