from pathlib import Path
import sys,random,struct,json,hashlib
sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
base=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/r1-vecfloat-terra');out=base/'render-rce-rebind-v4';ro=out/'render/probe.obj';po=out/'blobby/probe.obj';r=F.Coff(ro,'?GenerateDispatchLists@RaceCarEntityModule@BrnWorld@@');p=F.Coff(po,'?AddShadow@BrnBlobbyShadowBuffer@BrnBlobbyShadowManager@@');b=F.Bundle([r,p]);b.select('?GenerateDispatchLists@RaceCarEntityModule@BrnWorld@@');provider=F.address(b,'?AddShadow@BrnBlobbyShadowBuffer@BrnBlobbyShadowManager@@');loc={n:b.bases[(1,s)]+v for n,v,s in p.symbols.values() if n in ('kfAmbientShadowFadeDistance','kvShadowOffset') and (1,s) in b.bases};assert len(loc)==2,loc
rng=random.Random(0x822e8040);errors=[]
for i in range(192):
 def fv():return struct.unpack('>4I',struct.pack('>4f',*[rng.uniform(-5,5) for _ in range(4)]))
 ext,height,offset,dim=fv(),fv(),fv(),fv();matrix=struct.pack('>16f',*[rng.uniform(-100,100) for _ in range(16)]);initial=bytearray(rng.randbytes(0x1010));struct.pack_into('>I',initial,0,[0,1,63,64][i%4]);results=[]
 for compiled in (False,True):
  c=F.cpu(b);sp=c.g(1);c.mem.add(0x82fae000,0x2000);c.mem.wr(0x82faf0e0,struct.pack('>4f',*[1.25]*4));c.mem.wr(0x82faefb0,struct.pack('>4f',0.,.03,0.,0.));c.mem.wr(loc['kfAmbientShadowFadeDistance'],struct.pack('>4f',*[1.25]*4));c.mem.wr(loc['kvShadowOffset'],struct.pack('>4f',0.,.03,0.,0.));c.mem.wr(F.M.S.AUX,initial);c.setg(11,F.M.S.SELF+0x1000);c.mem.wr(F.M.S.SELF+0x1040,struct.pack('>4I',*dim));c.mem.wr(sp+(0xb8 if compiled else 0x120),struct.pack('>I',F.M.S.AUX));c.mem.wr(sp+(0x160 if compiled else 0x150),matrix)
  if compiled:
   c.vr[125]=ext;c.vr[127]=height;c.vr[126]=offset;c.mem.wr(b.entry+0x834,bytes.fromhex('4e800020'));c.run(b.entry+0x814);c.run(provider)
  else:
   image=(base/'822E79F8.bin').read_bytes();c.mem.add(0x822e79f8,len(image),image);c.vr[124]=ext;c.vr[126]=height;c.vr[125]=offset;c.mem.wr(0x822e8040,bytes.fromhex('4e800020'));c.run(0x822e8018);c.run(0x823f7638)
  results.append((c.g(3),c.mem.rd(F.M.S.AUX,0x1010)))
 if results[0]!=results[1]:errors.append([i,'return/buffer differs'])
d={'cases':192,'count':len(errors),'mismatches':errors[:3],'render_object_sha256':hashlib.sha256(ro.read_bytes()).hexdigest(),'provider_object_sha256':hashlib.sha256(po.read_bytes()).hexdigest(),'scope':'actual Render shadow-call setup to actual linked AddShadow provider versus original setup and original provider; full 0x1010 buffer/return, counts 0,1,63,64; statics seeded to independently witnessed native values; emulator arithmetic and controlled inputs, no physical renderer claim'};(out/'actual-shadow-chain-comparison.json').write_text(json.dumps(d,indent=2)+'\n');print(json.dumps(d,indent=2));assert not errors
