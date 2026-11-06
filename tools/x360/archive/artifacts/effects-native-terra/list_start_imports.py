from pathlib import Path
import ast,json,struct,hashlib,sys,re
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
MOD=0x40000000;TEXT=0x41000000;DATA=0x42000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem()
 for a,n in [(MOD,0x40000),(TEXT,0x1000),(DATA,0x1000),(STACK,0x20000)]:m.add(a,n)
 c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/effects-native-terra/effects12/EffectsModule-87aee4b9.obj');coff=Coff(obj,'?JunkyardVfxStart@EffectsModule@BrnEffects@@')
def readz(q,addr,n=1024):
 out=bytearray()
 for i in range(n):
  b=q.mem.rd((addr+i)&0xffffffff,1)[0]
  if not b:break
  out.append(b)
 return bytes(out)
def nicmp(q):
 a=q.g(3)&0xffffffff;b=q.g(4)&0xffffffff;n=q.g(5)&0xffffffff;x=readz(q,a,n).lower();y=readz(q,b,n).lower();q.setg(3,0 if x==y else (1 if x>y else 0xffffffff))
def strlen(q):q.setg(3,len(readz(q,q.g(3)&0xffffffff)))
def vecfloat(q):
 dst=q.g(3)&0xffffffff;src=q.g(4)&0xffffffff;q.mem.wr(dst,q.mem.rd(src,4)*4)
print('\n'.join(coff.unresolved.values()))
