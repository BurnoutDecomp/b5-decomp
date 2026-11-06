import ast, json, random, struct, hashlib, re, sys
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
from_script=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\validate_render_states.py')
t=ast.parse(from_script.read_text(encoding='utf-8'))
ns={'M':M,'struct':struct,'re':re}
exec(compile(ast.Module(body=[n for n in t.body if isinstance(n,ast.ClassDef) and n.name in ('CPU','Coff')],type_ignores=[]),str(from_script),'exec'),ns)
Coff=ns['Coff']
obj=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\shader_constants_native_luna_86a\final-compile\objects\CgsShaderConstants-52bcb307.obj')
rng=random.Random(0x827E8CF8)
BASE=0x57000000
SIZE=0x4000

def newmem():
 c=M.new_cpu();c.mem.add(BASE,SIZE);return c

def run_fix(prefix,entry,r3,base,buffer):
 coff=Coff(obj,prefix)
 outs=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else newmem()
  # Keep the same immutable input bytes for each side.
  if compiled:c.mem.add(BASE,SIZE)
  c.mem.wr(BASE,buffer)
  c.setg(3,r3);c.setg(4,base);c.run(coff.entry if compiled else entry)
  outs.append(c.mem.rd(BASE,SIZE))
 return outs,coff

def u32(buf,off):return struct.unpack_from('>I',buf,off)[0]
def w32(buf,off,v):struct.pack_into('>I',buf,off,v&0xffffffff)
report={'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'tests':[],'mismatches':[],'limits':'actual native PPC interpreter methods vs ARTIST with mapped source blocks; no link/renderer/GPU/game claim'}
# External base relocation: the three outer pointers relocate; each non-null name relocates.
for case,count in enumerate([0,1,4]):
 b=bytearray(SIZE);this=BASE+0x100
 w32(b,0x100,count);w32(b,0x104,0x300);w32(b,0x108,0x400);w32(b,0x10c,0x500)
 for i in range(4):w32(b,0x400+i*4,[0x700,0,0x720,0x740][i])
 b[0x700:0x70d]=b'AnimDuration\0';b[0x720:0x726]=b'World\0';b[0x740:0x747]=b'Normal\0'
 outs,coff=run_fix('?FixUp@ShaderConstantsExternal@@QAAIPAE@Z',0x827E8CF8,this,BASE,bytes(b))
 row={'method':'ShaderConstantsExternal::FixUp(u8*)','case':case,'count':count,'equal':outs[0]==outs[1],
      'original_fields':[hex(u32(outs[0],0x100+i*4)) for i in range(4)],'compiled_fields':[hex(u32(outs[1],0x100+i*4)) for i in range(4)],
      'original_name_slots':[hex(u32(outs[0],0x400+i*4)) for i in range(4)],'compiled_name_slots':[hex(u32(outs[1],0x400+i*4)) for i in range(4)]}
 report['tests'].append(row)
 if not row['equal']:report['mismatches'].append(row)
# Internal base relocation: four outer fields and each instance-data pointer are rebased.
for case,count in enumerate([0,1,5]):
 b=bytearray(SIZE);this=BASE+0x100
 for off,val in [(0,count),(4,0x300),(8,0x400),(12,0x500),(16,0x600)]:w32(b,0x100+off,val)
 for i in range(5):w32(b,0x400+i*4,[0x700,0,0x720,0x740,0x760][i])
 outs,coff=run_fix('?FixUp@ShaderConstantsInternal@@QAAIPAE@Z',0x827E8D68,this,BASE,bytes(b))
 row={'method':'ShaderConstantsInternal::FixUp(u8*)','case':case,'count':count,'equal':outs[0]==outs[1],
      'original_fields':[hex(u32(outs[0],0x100+i*4)) for i in range(5)],'compiled_fields':[hex(u32(outs[1],0x100+i*4)) for i in range(5)],
      'original_instance_slots':[hex(u32(outs[0],0x400+i*4)) for i in range(count)],'compiled_instance_slots':[hex(u32(outs[1],0x400+i*4)) for i in range(count)]}
 report['tests'].append(row)
 if not row['equal']:report['mismatches'].append(row)
# CPU block relocation: slot 0 is cleared, outer pointers and all data slots relocate,
# while null name slots stay null.
for case,count in enumerate([0,1,5,255]):
 b=bytearray(SIZE);this=BASE+0x100
 w32(b,0x100,0xDEADBEEF);w32(b,0x104,count);w32(b,0x108,0x800);w32(b,0x10c,0x1000)
 for i in range(count):
  w32(b,0x1000+i*4,0 if i%3==1 else 0x2000+i*16)
  w32(b,0x800+i*4,0x3000+i*16)
 outs,coff=run_fix('?FixUp@ShaderConstantsCPU@@QAAIPAE@Z',0x827E9800,this,BASE,bytes(b))
 row={'method':'ShaderConstantsCPU::FixUp(u8*)','case':case,'count':count,'equal':outs[0]==outs[1],
      'original_fields':[hex(u32(outs[0],0x100+i*4)) for i in range(4)],'compiled_fields':[hex(u32(outs[1],0x100+i*4)) for i in range(4)],
      'original_null_name_slots':[i for i in range(count) if u32(outs[0],0x1000+i*4)==0], 'compiled_null_name_slots':[i for i in range(count) if u32(outs[1],0x1000+i*4)==0]}
 report['tests'].append(row)
 if not row['equal']:report['mismatches'].append(row)
# External post-fixup name lookup: use actual absolute pointers (the base FixUp is not
# part of this method) and compare the full return flag across exact/missing/case cases.
for case,(count,query) in enumerate([(0,'AnimDuration'),(1,'AnimDuration'),(2,'World'),(3,'world'),(4,'Missing'),(3,'')]):
 b=bytearray(SIZE);this=BASE+0x100
 w32(b,0x100,count);w32(b,0x108,BASE+0x400)
 names=['AnimDuration','World','Normal','Extra']
 for i in range(count):
  addr=BASE+0x800+i*0x100;w32(b,0x400+i*4,addr)
  val=names[i].encode()+b'\0';b[addr-BASE:addr-BASE+len(val)]=val
 qptr=BASE+0x1200;qb=query.encode()+b'\0';b[0x1200:0x1200+len(qb)]=qb
 coff=Coff(obj,'?HasShaderConstant@ShaderConstantsExternal@@QBA_NPBD@Z')
 outs=[]
 for compiled in (False,True):
  c=coff.cpu() if compiled else newmem()
  if compiled:c.mem.add(BASE,SIZE)
  c.mem.wr(BASE,b);c.setg(3,this);c.setg(4,qptr);c.run(coff.entry if compiled else 0x827E8DE0);outs.append(c.g(3)&0xffffffff)
 row={'method':'ShaderConstantsExternal::HasShaderConstant','case':case,'count':count,'query':query,'original':outs[0],'compiled':outs[1],'equal':outs[0]==outs[1]}
 report['tests'].append(row)
 if not row['equal']:report['mismatches'].append(row)
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\shader_constants_native_luna_86a\fixup-lookup-comparisons.json')
out.write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'tests':len(report['tests']),'mismatches':len(report['mismatches']),'report':str(out)},indent=2))
assert not report['mismatches']
