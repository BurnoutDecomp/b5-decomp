from pathlib import Path
import ast, re, json, struct, random, hashlib, sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
SELF=0x40000000; MAT=0x41000000; STACK=0x44000000
class CPU(E.CPU64): pass
def cpu():
 m=V.Mem();m.add(SELF,0x1100);m.add(MAT,0x100);m.add(STACK,0x20000);m.add(0x82fae000,0x2000);m.wr(0x82faefb0,struct.pack('>4f',0.,.03,0.,0.));m.wr(0x82faf0e0,struct.pack('>4f',1.25,1.25,1.25,1.25));c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/blobby_before/probe.obj');coff=Coff(obj,'?AddShadow@BrnBlobbyShadowBuffer@BrnBlobbyShadowManager@@');print('object',coff.fn,'unresolved',coff.unresolved)
rng=random.Random(360);errors=[]
for index in range(256):
 buf=bytearray(rng.randbytes(0x1010));count=[0,1,32,63,64,65][index%6];struct.pack_into('>i',buf,0,count)
 mat=[rng.uniform(-5,5) for _ in range(16)]; ext=[rng.uniform(-5,5) for _ in range(4)]; width=[rng.uniform(-2,2) for _ in range(4)];height=[-1.,0.,1.25,1.251,2.][index%5];heights=[height]*4
 if index%11==0:heights[index%4]=-0.5
 outputs=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else cpu();c.mem.wr(SELF,buf);c.mem.wr(MAT,struct.pack('>16f',*mat));c.setg(3,SELF);c.setg(4,MAT);c.setvf(1,ext);c.setvf(2,width);c.setvf(3,heights);c.run(coff.entry if compiled else 0x823f7638);outputs.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0x1010)))
 if outputs[0]!=outputs[1]:errors.append({'case':index,'original_return':outputs[0][0],'native_return':outputs[1][0],'first_differences':[j for j,(a,b) in enumerate(zip(outputs[0][1],outputs[1][1])) if a!=b][:20]})
res={'cases':256,'mismatches':len(errors),'examples':errors[:10],'original':'ARTIST AddShadow 823F7638','object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual provider COFF vs ARTIST under emulator; synthetic inputs include divergent lanes and count/fade boundaries; startup-initialized offset and fade vectors supplied from original initializer; finite values only; no caller or hardware proof.'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/blobby-comparison.json').write_text(json.dumps(res,indent=2));print(json.dumps(res,indent=2));assert not errors
