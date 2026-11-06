from pathlib import Path
import sys,struct,json,hashlib,random,inspect
sys.path.insert(0,r'D:/Reverse/BP-Decomp_Workflow/tools/re');import x360rd,ppcdis
sys.path.insert(0,r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
OUT=Path(__file__).resolve().parent;sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
assert x360rd.u32(0x82815570)==0x7c8a2378 and x360rd.u32(0x82f264a8)==4
assert F.M.V.x360rd.u32(0x82815570)==0x7c8a2378 and F.M.V.x360rd.u32(0x82f264a8)==4
specs=[
 {'address':0x82fb2a00,'name':'KV4_RACE_DISTANCE_START_COLOUR','entry':0x82c503d0,'end':0x82c50418,'store':0x82c50410,'slot':0x82cd0954,'source':[0x82013a78,0x8204e448,0x82005574,0x82001c98],'literal':'Vector4(0.85f, 0.55f, 0.02f, 1.0f)'},
 {'address':0x82fb2a90,'name':'KV4_RACE_DISTANCE_END_COLOUR','entry':0x82c50418,'end':0x82c50458,'store':0x82c50450,'slot':0x82cd0958,'source':[0x82054364,0x82001cc0,0x82001cc0,0x82001c98],'literal':'Vector4(0.64f, 0.0f, 0.0f, 1.0f)'},
 {'address':0x82fb2be0,'name':'KV2_RACE_DISTANCE_START_SCALE','entry':0x82c50458,'end':0x82c5048c,'store':0x82c50484,'slot':0x82cd095c,'source':[0x82001c98,0x82001c98,None,None],'literal':'Vector2(1.0f, 1.0f)','zero_std':0x82c50470},
 {'address':0x82fb2d70,'name':'KV2_RACE_DISTANCE_END_SCALE','entry':0x82c50490,'end':0x82c504c4,'store':0x82c504bc,'slot':0x82cd0960,'source':[0x8201ecc8,0x8201ecc8,None,None],'literal':'Vector2(1.3f, 1.3f)','zero_std':0x82c504a8},
 {'address':0x82fb29e0,'name':'KV4_SAFECOLOUR','entry':0x82c504c8,'end':0x82c50508,'store':0x82c50500,'slot':0x82cd0964,'source':[0x82001c98,0x820054c8,0x82001cc0,0x82001c98],'literal':'Vector4(1.0f, 0.8f, 0.0f, 1.0f)'},
 {'address':0x82fb2ac0,'name':'KV4_DANGERCOLOUR','entry':0x82c50508,'end':0x82c50548,'store':0x82c50540,'slot':0x82cd0968,'source':[0x8204c5b4,0x820047b8,0x820047b8,0x82001c98],'literal':'Vector4(0.6f, 0.06f, 0.06f, 1.0f)'}]
source_words={a:x360rd.u32(a)for row in specs for a in row['source']if a is not None}
for a,w in source_words.items():assert F.M.V.x360rd.u32(a)==w
records=[];rng=random.Random(0x82c503d0)
for index,row in enumerate(specs):
 code=x360rd.rd(row['entry'],row['end']-row['entry']);assert x360rd.u32(row['slot'])==row['entry']
 (OUT/('%08X.thunk.bin'%row['entry'])).write_bytes(code)
 # ppcdis symbol comments can retain a stale prior value when r11 becomes a
 # stack address. Strip those comments; executed effective addresses below
 # are independently captured from actual GPRs.
 asm='\n'.join(line.split('  ;',1)[0]for line in ppcdis.disasm(row['entry'],len(code)//4))+'\n'
 (OUT/('%08X.thunk.asm'%row['entry'])).write_text(asm)
 expected=struct.pack('>4I',*[source_words[a]if a is not None else 0 for a in row['source']])
 base,size=0x82fb29d0,0x3c0;initial=rng.randbytes(size);c=F.M.new_cpu();c.__class__=F.AtomicCPU
 c.msr=0x8000;c.reservation=None;c.failures=0;c.atomic_trace=[];c.fence_trace=[]
 c.mem.wr(F.M.S.STACK,bytes([0x31+index*0x17])*0x20000);c.mem.add(base,size,initial)
 for r in range(2,32):c.setg(r,rng.getrandbits(64))
 for r in range(128):c.vr[r]=tuple(rng.getrandbits(32)for _ in range(4))
 for r in range(32):c.setf(r,(-1. if r%2 else 1.)*(r+index+.25))
 events=[]
 class Observe(type(c)):
  def fetch(s,pc):
   w=super().fetch(pc);op=w>>26
   if op==48:
    rd,ra=(w>>21)&31,(w>>16)&31;imm=w&65535;imm=imm-65536 if imm&32768 else imm;ea=((s.g(ra)if ra else 0)+imm)&0xffffffff
    events.append({'event':'lfs','pc':hex(pc),'word':hex(w),'fpr':rd,'address':hex(ea),'bits':s.mem.rd(ea,4).hex()})
   if pc==row['store']:events.append({'event':'stvx128','pc':hex(pc),'word':hex(w),'address':hex(s.g(11)&0xffffffff),'width':16,'vector0':[hex(v)for v in s.vr[0]]})
   if pc==row.get('zero_std'):
    events.append({'event':'std zeros ZW','pc':hex(pc),'word':hex(w),'address':hex(s.g(11)&0xffffffff),'r10':hex(s.g(10))})
   return w
 c.__class__=Observe;c.run(row['entry']);actual=c.mem.rd(base,size);want=bytearray(initial);off=row['address']-base;want[off:off+16]=expected
 assert actual==want,(row['name'],actual[off:off+16].hex(),expected.hex())
 assert any(e['event']=='stvx128'and int(e['address'],16)==row['address']for e in events)
 words=struct.unpack('>4I',expected);values=struct.unpack('>4f',expected)
 records.append({'index_CRT_order':index,'existing_CPP_name':row['name'],'target':hex(row['address']),'entry':hex(row['entry']),'last_instruction':hex(row['end']-4),'store_PC':hex(row['store']),'CRT_table_slot':hex(row['slot']),'CRT_table_slot_bytes':x360rd.rd(row['slot'],4).hex(),'source_literals':[{'address':hex(a),'bits':hex(source_words[a])}if a is not None else{'explicit_zero_by_std':hex(row['zero_std'])}for a in row['source']],'words':[f'{w:08X}'for w in words],'bytes_BE':expected.hex(),'float32_values':values,'CXX03_constructor_values_only':row['literal'],'width_bytes':16,'all_lanes_fully_written':True,'splat':False,'unused_lanes':'Z and W explicitly zero via64-bit std'if 'zero_std'in row else'RGBA all four explicitly initialized','poison_sha256':hashlib.sha256(initial).hexdigest(),'after_sha256':hashlib.sha256(actual).hexdigest(),'entire_guarded_region_compared':{'base':hex(base),'size':hex(size),'only_expected_target16byte_store':True},'native_events':events,'raw_thunk_sha256':hashlib.sha256(code).hexdigest()})
print([(r['target'],r['words'],r['CXX03_constructor_values_only'])for r in records])
# Pin the two named retail consumer listings to address roles; no consumer
# body or behavior is edited/executed in this data-only task.
primary=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX');consumers={}
for addr in [0x82421980,0x82429150]:
 path=primary/('0x%08X.json'%addr);d=json.loads(path.read_text());(OUT/('%08X.consumer.asm'%addr)).write_text(d['assembly']+'\n');consumers[hex(addr)]={'export':str(path),'export_sha256':sha(path)}
report={'scope':'exact six EventInfo native dynamic initializer contracts; read-only product, no compiler','records':records,'passed_initializer_contracts':6,'sentinels':{'82815570':'7C8A2378','82F264A8':'00000004'},'raw_image':x360rd.RAW,'raw_reader':str(Path(x360rd.__file__)),'raw_reader_sha256':sha(x360rd.__file__),'source_literal_words':{hex(a):f'{w:08X}'for a,w in source_words.items()},'consumers':consumers,'fixture_module':str(Path(F.__file__)),'fixture_module_sha256':sha(F.__file__),'verifier_sha256':sha(__file__),'limits':'Executes only six real straight-line initializer thunks. No VMX arithmetic/estimate model is needed; native lfs/stfs/std/lvx/stvx words run. Confirms exact full16-byte rows at completion and all neighboring guarded bytes unchanged, from distinct poison. CRT-table slots are raw primary registrations; startup dispatcher/full boot and untraced later writes are not executed or claimed. Existing source constant names are used as consumer labels, not recovered original symbol-name claims. Constructor component values are supplied; no source/type/home/storage-lifetime change is made. Raw BSS zeros are not treated as runtime values. Physical hardware and whole GUI behavior remain outside scope.'}
p=OUT/'SIX_INITIALIZER_EXECUTION.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('REPORT',sha(p))
