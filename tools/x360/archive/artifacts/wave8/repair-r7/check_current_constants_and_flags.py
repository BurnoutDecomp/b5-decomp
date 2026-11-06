"""Execute current EventInfo CRT initializers and current64-bit Camera probe."""
from datetime import datetime,timezone
import hashlib,json,random,struct,sys
from pathlib import Path
sys.dont_write_bytecode=True
HERE=Path(__file__).resolve().parent;ART=HERE.parent.parent
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
import native_atomic_fixture as F
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
report=HERE/'selected-successor1/compile-results.json';rows=json.loads(report.read_text())['results'];row=next(r for r in rows if Path(r['source']).name=='BrnEventInfo.cpp');assert row['passed']
original_path=ART/'eventinfo-constants-sol/SIX_INITIALIZER_EXECUTION.json';original=json.loads(original_path.read_text());rng=random.Random(0x82c503d0);details=[]
for native in original['records']:
 name=native['existing_CPP_name'];co=F.Coff(Path(row['object']),'??__E'+name);b=F.Bundle([co]);b.select('??__E'+name)
 matches=[(value,sec)for symbol,value,sec in co.symbols.values()if symbol==name and sec>0];assert len(matches)==1,(name,matches)
 off,sec=matches[0];base=b.bases[(0,sec)];size=co.sections[sec][3];before=rng.randbytes(size);c=F.cpu(b);c.mem.wr(base,before)
 for i in range(128):c.vr[i]=tuple(rng.getrandbits(32)for _ in range(4))
 c.run(b.entry);after=c.mem.rd(base,size);want=bytearray(before);want[off:off+16]=bytes.fromhex(native['bytes_BE']);assert after==want,(name,after.hex(),want.hex())
 details.append({'name':name,'original_entry':native['entry'],'native_expected16':native['bytes_BE'],'compiled_initializer':b.symbol,'current_actual16':after[off:off+16].hex(),
  'poison_region_size':size,'all_other_current_storage_bytes_preserved':True,'current_object_sha256':sha(row['object'])})
obj=HERE/'named-native-gates/CameraFlags.obj';checked=0;flagdetails=[]
for name in ['SetValid','ClearValid','ReadValid']:
 co=F.Coff(obj,name)
 for i in range(256):
  before=bytearray(rng.randbytes(0x180));value=struct.unpack_from('>Q',before,0x140)[0];c=co.cpu();c.mem.wr(F.M.S.SELF,before);c.setg(3,F.M.S.SELF);c.run(co.entry);after=c.mem.rd(F.M.S.SELF,len(before))
  if name=='SetValid':struct.pack_into('>Q',before,0x140,value|2)
  if name=='ClearValid':struct.pack_into('>Q',before,0x140,value&~2)
  assert after==before,(name,i)
  if name=='ReadValid':assert(c.g(3)&0xffffffff)==bool(value&2)
  checked+=1
 flagdetails.append({'function':name,'cases':256})
result={'head':'129afef95856e383366a55fb8f30a66ceb74de42','actual_selected_report_sha256':sha(report),'EventInfo_current_object':row['object'],'EventInfo_current_object_sha256':sha(row['object']),
 'authenticated_original_thunk_report':str(original_path),'authenticated_original_thunk_report_sha256':sha(original_path),'passed_current_initializer_contracts':6,'initializers':details,
 'current_camera_probe':str(obj),'current_camera_probe_sha256':sha(obj),'camera_cases':checked,'camera_functions':flagdetails,
 'named_probe_current_input_binding_sha256':sha(HERE/'named-native-gates/successor1-current-binding.json'),
 'scope':'Six current actual CRT initializer bodies compared with original authenticated thunk16-byte results, full current shared-storage poison preserved. Camera actual typed-header probes exercise full64-bit BE flags and all surrounding bytes. No raw BSS inference, full CRT dispatch/boot/gameplay/hardware claim.',
 'verifier_sha256':sha(__file__),'created_utc':datetime.now(timezone.utc).isoformat()}
with(HERE/'current-constants-and-camera-flags.json').open('x')as f:json.dump(result,f,indent=2)
print('CURRENT_INITIALIZERS',6,'CAMERA_FLAGS',checked,'sha',sha(HERE/'current-constants-and-camera-flags.json'),flush=True)
