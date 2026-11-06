import ast, importlib.util, itertools, json, random, re, struct, sys, hashlib
from pathlib import Path
sys.path.insert(0,r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk')
import validate_native_math as M
# Verify the original-image mapping before reading original X360 memory.
spec=importlib.util.spec_from_file_location('xr',r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\tools\re\x360rd.py');XR=importlib.util.module_from_spec(spec);spec.loader.exec_module(XR)
assert XR.u32(0x82815570)==0x7c8a2378 and XR.u32(0x82F264A8)==4

# Use the established PowerPC COFF relocation/emulator helper, with independent image bases
# for the CgsShaderConstants and ProgramBuffer objects so the compiled helper can be called.
checker=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\scratch\x360_sdk\validate_render_states.py')
tree=ast.parse(checker.read_text(encoding='utf-8'));ns={'M':M,'struct':struct,'re':re}
for node in tree.body:
 if isinstance(node,ast.ClassDef) and node.name in ('CPU','Coff'):
  code=ast.get_source_segment(checker.read_text(encoding='utf-8'),node)
  if node.name=='Coff':
   code=code.replace('def __init__(self, path, prefix):','def __init__(self, path, prefix, load_base=0x83800000):\n        self.load_base = load_base')
   code=code.replace('0x83800000 + i*0x10000','self.load_base + i*0x10000')
  exec(compile(code,str(checker),'exec'),ns)
Coff=ns['Coff']
objdir=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\shader_constants_native_luna_86a\final-compile\objects')
report=json.loads((objdir/'compile-results.json').read_text(encoding='utf-8'))
objects={Path(r['source']).name:Path(r['object']) for r in report['results'] if r['passed']}
main=Coff(objects['CgsShaderConstants.cpp'],'?FixUp@ShaderConstantsExternal@@QAAIPBVProgramBuffer@renderengine@@@Z',0x83800000)
prog=Coff(objects['programbuffer.cpp'],'?GetVariableHandleByName@ProgramBuffer@renderengine@@',0x85000000)

def used_bases(coff):
 used,pending=set(),[coff.fn[2]]
 while pending:
  sec=pending.pop()
  if sec in used:continue
  used.add(sec)
  for at,index,kind in coff.relocations(sec):
   if kind==0x12:continue
   _,_,target=coff.symbols[index]
   if target>0:pending.append(target)
 return {sec:coff.load_base+i*0x10000 for i,sec in enumerate(sorted(used))}

def symbol_address(coff,pattern,bases):
 found=[v for v in coff.symbols.values() if v[0]==pattern and v[2]>0]
 assert len(found)==1,(pattern,found)
 name,val,sec=found[0];assert sec in bases,(name,sec)
 return bases[sec]+val

main_bases=used_bases(main)
main_table=symbol_address(main,'?mShaderConstantTable@CgsGraphics@@3UShaderConstantTable@@A',main_bases)
# All table names are existing ShaderConstantTable::Construct slot definitions from c6.
SLOT_NAMES=['world','view','projection','viewProjection','worldViewProj','viewInverse','InstancingMatrixArray']
NAME_BASE=0x53000000; PBUF=0x54000000; EXT=0x55000000; TABLE_ORIG=0x830113D0

def prepare_data(input_order, program_order):
 names=bytearray(0x1000); table=bytearray(0x680); block=bytearray(0x1000); pbuf=bytearray(0x1000)
 name_ptr={}
 all_names=list(dict.fromkeys(SLOT_NAMES+[n for n in input_order]))
 for i,name in enumerate(all_names):
  b=name.encode()+b'\0';off=0x100+i*0x40;names[off:off+len(b)]=b;name_ptr[name]=NAME_BASE+off
 for slot,name in enumerate(SLOT_NAMES):
  size=64;count=5 if slot==6 else 1
  struct.pack_into('>HBB',table,slot*12,(size*count)//16,size,count)
  struct.pack_into('>I',table,800+slot*4,name_ptr[name])
 table[1400]=len(SLOT_NAMES)
 struct.pack_into('>IIII',block,0,3,EXT+0x200,EXT+0x300,EXT+0x400)
 for i,name in enumerate(input_order):
  struct.pack_into('>I',block,0x300+i*4,name_ptr[name])
  struct.pack_into('>I',block,0x200+i*4,0xFFFFFFFF)
  block[0x400+i*4:0x404+i*4]=bytes([0xA0+i]*4)
 # ProgramBufferData header then three native 8-byte descriptors, names stored at absolute pointers.
 struct.pack_into('>IHHIII',pbuf,0,1,len(program_order),0,0,0,0)
 for i,(name,regset,regindex,regcount) in enumerate(program_order):
  off=0x800+i*0x40;b=name.encode()+b'\0';pbuf[off:off+len(b)]=b
  struct.pack_into('>IBBBB',pbuf,0x14+i*8,PBUF+off,regset,regindex,regcount,0)
 return bytes(names),bytes(table),bytes(block),bytes(pbuf),name_ptr

def add_region(c,addr,data):
 c.mem.add(addr,len(data),data)

def new_native_cpu():
 c=M.new_cpu();c.__class__=ns['CPU'];return c

def compare_case(case,input_order,program_order):
 names,table,block,pbuf,name_ptr=prepare_data(input_order,program_order)
 results=[]
 # Original whole ARTIST FixUp calls original ProgramBuffer::GetVariableHandleByName at 0x82B609B0.
 orig=new_native_cpu();add_region(orig,NAME_BASE,names);add_region(orig,PBUF,pbuf);add_region(orig,EXT,block);orig.mem.add(TABLE_ORIG,len(table),table)
 orig.setg(3,EXT);orig.setg(4,PBUF);orig.run(0x827ED8D0)
 out_original=(orig.g(3)&0xffffffff,orig.mem.rd(EXT,len(block)))
 results.append(out_original)
 # Compiled whole FixUp plus the actual compiled ProgramBuffer helper; intercept only the
 # imported helper edge to execute that compiled PPC object in the same mapped memory.
 comp=new_native_cpu()
 for addr,body in main.images.items():add_region(comp,addr,body)
 for addr,body in prog.images.items():add_region(comp,addr,body)
 add_region(comp,NAME_BASE,names);add_region(comp,PBUF,pbuf);add_region(comp,EXT,block);comp.mem.wr(main_table,table)
 def reject(symbol):
  def call(cpu):raise AssertionError(('unexpected unresolved call',symbol))
  return call
 helper_addr=None
 for addr,symbol in main.unresolved.items():
  if symbol.startswith('?GetVariableHandleByName@ProgramBuffer@renderengine@@'):
   helper_addr=addr
   comp.hooks[addr]=lambda cpu:cpu.call(prog.entry,0)
  else:comp.hooks[addr]=reject(symbol)
 assert helper_addr is not None,main.unresolved
 comp.setg(3,EXT);comp.setg(4,PBUF);comp.run(main.entry)
 out_compiled=(comp.g(3)&0xffffffff,comp.mem.rd(EXT,len(block)))
 # Reorder to expose exact native slot-6 then slot-0 hoisting and array-size override.
 expected_order=sorted(zip(input_order,range(len(input_order))),key=lambda x:(x[0]!='world',x[0]!='InstancingMatrixArray'))
 # Original is authoritative; report final indices and handles to expose effects.
 index_ptr=u32(out_original[1],4);name_ptr_out=u32(out_original[1],8);handle_ptr=u32(out_original[1],12)
 final_indices=[u32(out_original[1],index_ptr-EXT+i*4) for i in range(3)]
 final_names=[struct.unpack_from('>I',out_original[1],name_ptr_out-EXT+i*4)[0] for i in range(3)]
 final_handles=[out_original[1][handle_ptr-EXT+i*4:handle_ptr-EXT+i*4+4].hex() for i in range(3)]
 row={'case':case,'input_order':input_order,'program_descriptor_order':[x[0] for x in program_order],
      'original_return_register_count':out_original[0],'compiled_return_register_count':out_compiled[0],
      'original_indices_after_6_then_0_hoists':final_indices,'original_name_ptrs':list(map(hex,final_names)),
      'original_handle_bytes':final_handles,'equal':out_original==out_compiled}
 return row

perms=list(itertools.permutations(['world','view','InstancingMatrixArray']))
rows=[]
for i,order in enumerate(perms):
 # Independently shuffle the program's descriptor order while preserving the names/types.
 desc=[('world',0,4,4),('view',0,8,2),('InstancingMatrixArray',1,12,1)]
 program_order=desc[i%len(desc):]+desc[:i%len(desc)]
 rows.append(compare_case(i,list(order),program_order))
report_out={'original':'ShaderConstantsExternal::FixUp(ProgramBuffer*) @0x827ED8D0',
 'compiled_symbol':main.fn[0],'compiled_helper':prog.fn[0],
 'main_object':str(objects['CgsShaderConstants.cpp']),'main_object_sha256':hashlib.sha256(objects['CgsShaderConstants.cpp'].read_bytes()).hexdigest(),
 'programbuffer_object':str(objects['programbuffer.cpp']),'programbuffer_object_sha256':hashlib.sha256(objects['programbuffer.cpp'].read_bytes()).hexdigest(),
 'table_global_symbol_address_compiled':hex(main_table),'cases':rows,'mismatches':[r for r in rows if not r['equal']],
 'limits':'Six valid bound-name orders with actual compiled ProgramBuffer::GetVariableHandleByName called through a controlled cross-object edge; shader table seeded from native Construct slot names/sizes, no renderer/GPU/link/game claim; null/unbound diagnostic path validated by source/ASM, not executed.'}
out=Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave9\shader_constants_native_luna_86a\program-buffer-fixup-comparison.json')
out.write_text(json.dumps(report_out,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':len(rows),'mismatches':len(report_out['mismatches']),'sample':rows[0],'report':str(out)},indent=2))
assert not report_out['mismatches']
