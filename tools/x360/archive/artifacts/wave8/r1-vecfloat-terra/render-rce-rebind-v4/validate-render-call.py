from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import validate_render_states as R
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v4';obj=out/'render/probe.obj';o=R.Coff(obj,'?GenerateDispatchLists@RaceCarEntityModule@BrnWorld@@');rng=random.Random(0x822e8324);errors=[]
for i in range(256):
 def vec():return tuple(rng.getrandbits(32) for _ in range(4))
 distance,fog,col=vec(),vec(),vec();index=i%8; this=R.M.S.SELF;frame=R.M.S.AUX+0x100;shadow=R.M.S.AUX+0x200;layout=R.M.S.SELF+0x2000;off=0x300;args=[rng.randrange(100) for _ in range(3)];on=i&1;results=[]
 for compiled in (False,True):
  c=o.cpu() if compiled else R.M.new_cpu();sp=c.g(1);c.setg(3,layout);c.vr[121]=col;c.vr[122]=fog
  if compiled:
   c.vr[126]=distance;c.setg(11,index*0x14a0);c.setg(18,off);c.setg(21,shadow);c.setg(22,on);c.setg(23,this);c.setg(27,frame);c.setg(24,args[0]);c.mem.wr(sp+0x324,struct.pack('>I',args[1]));c.mem.wr(sp+0x32c,struct.pack('>I',args[2]));c.mem.wr(o.entry+0x45c,bytes.fromhex('4e800020'));c.run(o.entry+0x424)
  else:
   image=(base/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.vr[127]=distance;c.setg(30,index);c.setg(16,off);c.setg(19,shadow);c.setg(20,on);c.setg(22,this);c.setg(23,frame);c.setg(28,sp+0x120);c.setg(29,sp+0x140)
   for k,v in zip((0x394,0x39c,0x3a4),args):c.mem.wr(sp+k,struct.pack('>I',v))
   c.mem.wr(0x822e8324,bytes.fromhex('4e800020'));c.run(0x822e82e8)
  results.append(([c.g(x) for x in range(3,11)],c.vr[1],c.vr[2],c.vr[3],c.mem.rd(sp+0x84,4),c.mem.rd(sp+0x8f,1)))
 if results[0]!=results[1]:errors.append([i,str(results)])
d={'cases':256,'count':len(errors),'mismatches':errors[:2],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'compiled_slice':'GenerateDispatchLists COMDAT+424..458','original_slice':'822E82E8..8320','scope':'actual replay caller RenderRaceCar outgoing r3-r10, v1-v3 and stack shadow/attached arguments vs ARTIST, randomized vector bits, eight indices and list IDs; call setup only, not callee body/full dispatch'};(out/'actual-render-call-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
