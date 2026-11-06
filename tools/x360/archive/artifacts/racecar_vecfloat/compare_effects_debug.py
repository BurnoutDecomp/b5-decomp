from pathlib import Path
import ast,re,json,struct,random,hashlib,sys
sys.path.insert(0,'D:/Reverse/BP-Decomp_Workflow/scratch/CRASHPARITY_0922/fxcrashvfx_vmxemu');import emu64 as E,vmxemu as V
p=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/validate_render_states.py');t=ast.parse(p.read_text());exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name=='Coff'],type_ignores=[]),'coff','exec'),globals())
DBG=0x40000000;MODULE=0x41000000;LION=0x42000000;STACK=0x44000000
class CPU(E.CPU64):pass
def cpu():
 m=V.Mem();m.add(DBG,0x1000);m.add(MODULE,0x40000);m.add(LION,0x1000);m.add(STACK,0x20000)
 def splat(a,x):m.add(a,16,struct.pack('>4I',*([x]*4)))
 for a,x in [(0x8307a680,0x3e22f983),(0x8307a3c0,0x3f800000),(0x8307a560,0xbe800000),(0x8307a5f0,0xc0c8e5ba),(0x8307a3b0,0x422396e8),(0x8307a670,0xc28e5ba2)]:splat(a,x)
 m.add(0x8307a590,16,struct.pack('>4I',0xbe800000,0,0xbe800000,0));c=CPU(m);c.setg(1,STACK+0x10000);return c
class M:pass
M.new_cpu=staticmethod(cpu)
obj=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/final_effects_debug/probe.obj');coff=Coff(obj,'?OnJunkyardEffectValueChange@EffectsDebugComponent@');rng=random.Random(3612);errs=[]
for i in range(128):
 dbg=bytearray(rng.randbytes(0x100));lion=bytearray(rng.randbytes(0x100));dbg[0x4d]=1;struct.pack_into('>I',dbg,0xc,MODULE);struct.pack_into('>I',dbg,0x50,i%10);struct.pack_into('>4f',dbg,0x54,*[rng.uniform(-500,500) for _ in range(3)],rng.uniform(-360,720));outs=[]
 for native in (False,True):
  c=coff.cpu() if native else cpu();c.mem.wr(DBG,dbg);c.mem.wr(LION,lion);c.mem.wr(MODULE+4*(48457+i%10),struct.pack('>I',0x1234));c.setg(3,0);c.setg(4,DBG)
  if native:
   for addr,name in coff.unresolved.items():
    if name.startswith('?GetJunkyardEffectHandle@'):c.hooks[addr]=lambda q:q.setg(3,0x1234)
    if name.startswith('?GetLionEffect@'):c.hooks[addr]=lambda q:q.setg(3,LION)
   c.run(coff.entry)
  else:c.hooks[0x82278380]=lambda q:q.setg(3,LION);c.run(0x82287c10)
  outs.append((c.mem.rd(DBG,0x100),c.mem.rd(LION,0x100)))
 if outs[0]!=outs[1]:errs.append({'case':i,'dbg_diffs':[j for j,(a,b) in enumerate(zip(outs[0][0],outs[1][0])) if a!=b][:10],'lion_diffs':[j for j,(a,b) in enumerate(zip(outs[0][1],outs[1][1])) if a!=b][:20]})
r={'cases':128,'mismatches':len(errs),'examples':errs[:8],'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'actual compiled EffectsDebug OnJunkyardEffectValueChange vs ARTIST 82287C10; edit-active valid index, finite positions/angles, GetLionEffect/handle controlled, original trig CRT globals supplied, full debug+Lion memory checked; no other branches or hardware claim'};Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_vecfloat/effects-debug-junkyard-comparison.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,indent=2));assert not errs
