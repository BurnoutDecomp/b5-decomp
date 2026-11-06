"""Bind the private native compressor provider and composed caller proof."""
from pathlib import Path
import hashlib,json,sys,re,shutil,subprocess
OUT=Path(__file__).resolve().parent
ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/compressor-native-3534')
sys.path.insert(0,str(OUT));from validate_kernel import F,Obj,Bundle
EXPORTS=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')

def sha(p):
 with Path(p).open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
def save(p,obj):p.write_text(json.dumps(obj,indent=2)+'\n')
functions={};rd=F.M.V.x360rd
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/tools/re');import ppcdis
for ea in [0x82b64db0,0x82b67188,0x82b671f0,0x82b9d988,0x82b96b28,0x82b9e3a0,0x82b97ab0]:
 source=EXPORTS/('0x%08X.json'%ea);row={}
 if source.exists():
  dest=OUT/('original-%08X.json'%ea)
  if not dest.exists():shutil.copyfile(source,dest)
  assert source.read_bytes()==dest.read_bytes()
  obj=json.loads(source.read_text());addrs=[int(x,16)for x in re.findall(r'^0x([0-9a-fA-F]{8})\s',obj['assembly'],re.M)];end=max(addrs)+4
  row.update({'name':obj['name'],'export_path':str(dest),'export_sha256':sha(dest)})
 else:
  assert ea==0x82b9e3a0
  end=0x82b9e4ac;listing=ppcdis.disasm(ea,(end-ea)//4);dest=OUT/'original-82B9E3A0-disassembly.txt';dest.write_text('\n'.join(listing)+'\n');assert rd.u32(end-4)==0x4e800020
  row.update({'name':'rw::audio::core::Limiter1::Process','export_hole':True,'disassembly_path':str(dest),'disassembly_sha256':sha(dest),'boundary_evidence':'Native blr82B9E4A8; following00000000 padding82B9E4AC and next prologue82B9E4B0. Source/caller identity is independently attested by registered descriptor and actual callsite bytes.'})
 raw=OUT/('original-%08X.bin'%ea);raw.write_bytes(rd.rd(ea,end-ea));row.update({'range':['0x%08X'%ea,'0x%08X'%end],'original_instruction_bytes':str(raw),'original_instruction_sha256':sha(raw)});functions['0x%08X'%ea]=row
assert rd.u32(0x82815570)==0x7c8a2378
image=Path(rd.RAW or rd.FLAT)
save(OUT/'original-bindings.json',{'reader_path':rd.__file__,'reader_sha256':sha(rd.__file__),'disassembler_path':ppcdis.__file__,'disassembler_sha256':sha(ppcdis.__file__),'image_path':str(image),'image_sha256':sha(image),'calibration':{'0x82815570':'0x7C8A2378'},'functions':functions})
commits=[('b70dba1e99841d53f73de09bedfc4e59c81cdb42','482856c1e'),('ad4d64c2fc311aacd46e7ffee0f814275d2fb7a1','7192b23af'),('19a9fab7b040aafcf07660ab9f6c3900b130ea24','b74dccc72'),('11227e27134e5b414f0d0f1c9e234586b36eb1ab','3edda6d30'),('24f279305ff0014afbd3045302088bfc2afb4318','e6c08283f'),('00d2278e5433b2b8d6b72e7a70e0101bb1ee1dbc','ec638c11a'),('a60950d315762ed73940caa3914e3e3f1656b785','279fae362'),('0dee55e9f2bb4b81a00bc8eea7c0a984655b5f64','5e5f51199'),('1e34676afd8a7fd14a1017232aec0eb8b90e10d6','ea4c95e3c'),('62b3d4c34cc7a008207c3eb4a97764dd04f45a55','a55f023a8'),('a2a9d8aaa362566109e008b1b82d874c5c62fcb4','b76cc5bc1'),('b4d81df315f9592d95158e59dfe7cd67a1818310','73e33fc13')]
adoptions=[{'source_commit':source,'private_alias':subprocess.check_output(['git','rev-parse',alias],cwd=ROOT).decode().strip()}for source,alias in commits]
head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT).decode().strip()
save(OUT/'dependency-adoption.json',{'owned_commit':'eda9b17486b71dfaeb411a82d5d6e11c7276fa8e','base':'3534f7f16103a58ff904bbd1604e18e9934868f4','head':head,'foreign_adoptions_in_actual_order':adoptions,'duplicate_skipped':{'09842b282':'Exact peer cherry-pick of eda9 owned provider already present.'},'manual_resolution':'b70 Voice.cpp conflict preserved published Gain/Rechannel cases and removed only the obsolete Resample composed-destructor case. No further foreign edits.','coherent_original_prerequisite_order':['24f279305','00d2278e (peer alias59b5cd658)','a60950d31','0dee55e9f','1e34676af','62b3d4c34','a2a9d8aaa','b4d81df31','b70dba1e9','ad4d64c2f','19a9fab7b','eda9b1748 (peer alias09842b282)','11227e271'],'peer_source_confirmation':{'matches':8,'checked':9,'manifest':'composed-source-closure.json','exception':'Mixer.h peer0d8cfcb251e7e2de0dd1cb9132db363b8f559c75e997036b4f241e888ca9dd5f versus local178379e661ad6d9ee17cd6cba618a5906f98c405d152de62c0699cf2bbcffd95. Peer later e7d9cc482 changes only struct SampleBuffer to class SampleBuffer with public access. Fields/layout unchanged; raw identities are distinct.'}})
r=json.loads((OUT/'composed-callers-ready/native/compile-results.json').read_text());objects=[Obj(x['object'])for x in r['results']if Path(x['source']).name in('CompressorLimiter1.cpp','Compressor1.cpp','Limiter1.cpp')];providers={}
for name in ['Compressor1','Limiter1']:
 b=Bundle(objects);b.select('?Process@'+name+'@');providers[name]={'actual_entry':hex(b.entry),'actual_kernel_entry':hex(b.addr('?Process@CompressorLimiter1@')),'unresolved_services':sorted(b.unresolved.values()),'kernel_unresolved':any(x.startswith('?Process@CompressorLimiter1@')for x in b.unresolved.values())};assert not providers[name]['kernel_unresolved']
save(OUT/'composed-symbol-closure.json',providers)
fixture={}
for name,module in sorted(sys.modules.copy().items()):
 p=getattr(module,'__file__',None)
 if p and (p.lower().startswith('d:/reverse')or p.lower().startswith('d:\\reverse')or p.lower().startswith('c:/users/adri1/.codex/worktrees')or p.lower().startswith('c:\\users\\adri1\\.codex\\worktrees'))and Path(p).is_file():fixture[str(Path(p).resolve())]=sha(p)
extra=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_native_strings.py');fixture[str(extra)]=sha(extra)
save(OUT/'fixture-bindings.json',{'python':sys.version,'numpy':sys.modules['numpy'].__version__,'files':fixture,'original_bytes':'original-bindings.json','limitations':'Shared surrogate estimates; pow/memset/FPR helpers controlled; native cache/floating exception/VSCR state excluded.'})
status=subprocess.check_output(['git','status','--porcelain'],cwd=ROOT).decode();assert not status,status
owned=subprocess.check_output(['git','show','--format=','--name-only','eda9b1748'],cwd=ROOT).decode().splitlines();assert len(owned)==2
save(OUT/'checkpoint.json',{'state':'ready-later-only','owned_commit':'eda9b17486b71dfaeb411a82d5d6e11c7276fa8e','owned_paths':owned,'actual_compile_head':head,'clean':True,'compiled_whole_cpp':{'passed':5,'failed':0,'inputs':6280,'included_distinct_headers':103},'gates_retained':'All14 owned state/context anchors plus genuine foreign class/union/Mixer/configuration gates.','behavior_comparisons':{'full_provider_cases':84,'vector_power_cases':154,'unsigned_reduction_cases':512,'state_method_cases':512,'composed_plugin_process_cases':224,'total_mixed_cases':1486,'mismatches':0},'true_api':['void ClearBuffer()','void Configure(f32,f32,f32,s32,s32,bool)','void Process(Mixer*,int)'],'exact_provider_code_and_relocations_unchanged_across_composition':'provider-code-composition-binding.json','historical_failed_gate_preserved':'composed-callers/native/compile-results.json','limits':['No hardware estimate-table or hardware-bit parity claim; actual production native estimate instructions execute.','No floating exception, VSCR saturation or hardware cache-effect proof.','Software power-island conversion uses NaN0 surrogate for vctsxs, only consumed parity bit compared; no full NaN conversion-result claim.','Only live word3 of vupkd3d NORM2 zero input is modeled.','CRT pow, memset/XMemSet, FPR save/restore are controlled services in composed comparisons.','Not a full original instruction match: 2294 ARTIST Process instructions versus re-rolled524 native compiled instructions.','No whole-game strict link, audio hardware/output listening or launch claim.','New root/peer System/Profiler lifetime work is subsequent and outside this frozen receipt.']})
print('Packaged',len(functions),'original functions; imageSHA',sha(image));print('Composed external services',json.dumps(providers));print('Owned',owned)
