from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
from validate_resource_pointer import Bundle
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');obj=out/'Wheel/probe.obj';o=R.Coff(obj,'?UpdateVelocity@Wheel@');provider=R.Coff(out/'vecfloat/probe.obj','??0VecFloat@vpu@math@rw@@QAA@ABM@');bundle=Bundle([o,provider]);bundle.select('?UpdateVelocity@Wheel@');rng=random.Random(0x825d7008);errors=[]
def fbits(f):return struct.unpack('>I',struct.pack('>f',f))[0]
for i in range(256):
 initial=bytearray(rng.randbytes(0x100));vals=[rng.randrange(-80,81)/4,rng.randrange(-20,21)/4,rng.randrange(1,21)/4,rng.randrange(1,21)/4];struct.pack_into('>f',initial,0x30,vals[0]);struct.pack_into('>f',initial,0x34,vals[1]);struct.pack_into('>f',initial,0x68,vals[2]);struct.pack_into('>f',initial,0x6c,vals[3]);initial[0xd7]=i%3
 args=[0.125,80.0,float(i%4)/4,100.0,1000.0];results=[]
 for compiled in (False,True):
  c=bundle.cpu() if compiled else R.M.new_cpu();c.mem.wr(R.M.S.SELF,initial);c.setg(3,R.M.S.SELF);c.setg(4,i&1);c.setg(5,(i>>1)&1)
  for k,val in enumerate(args,1):c.vr[k]=(fbits(val),)*4
  if not compiled:
   image=(out/'825D7008.bin').read_bytes();c.mem.add(0x825d7008,len(image),image)
   for addr,data in [(0x82fb9cf0,struct.pack('>4f',*[9000.0]*4)),(0x82fb8bc0,struct.pack('>4f',*[2**-23]*4)),(0x82fb8b40,struct.pack('>4f',*[500.0]*4)),(0x8327f240,bytes(16)+bytes([255])*16)]:
    try:c.mem.wr(addr,data)
    except KeyError:c.mem.add(addr,len(data),data)
  if compiled:
   c.mem.add(0x838f0000,16,bytes.fromhex("34000000")+bytes(12)) # fixture COMDAT padding for lvlx
   for addr,name in bundle.unresolved.items():
    if name.startswith(('__savefpr_','__restfpr_','__savevmx_','__restvmx_')):c.hooks[addr]=lambda cpu:None
  c.run(bundle.entry if compiled else 0x825d7008);results.append(c.mem.rd(R.M.S.SELF,0x100))
 if results[0]!=results[1]:errors.append({'case':i,'offsets':[j for j,(a,b) in enumerate(zip(*results)) if a!=b], 'native':results[0][0x30:0x40].hex(), 'actual':results[1][0x30:0x40].hex()})
d={'cases':256,'mismatch_count':len(errors),'mismatches':errors[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'vecfloat_provider_sha256':hashlib.sha256((out/'vecfloat/probe.obj').read_bytes()).hexdigest(),'scope':'whole actual Wheel::UpdateVelocity vs original 825D7008 on finite binary-fraction states/flags, seeded original runtime constants/masks, full poisoned wheel bytes, SDK VecFloat constructor linked, FPR and VMX save/restore hooks controlled, software VMX; no hardware/game claim'};(out/'wheel-updatevelocity-comparison.json').write_text(json.dumps(d,indent=2));print(json.dumps(d,indent=2))


