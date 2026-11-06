from pathlib import Path
import ast, hashlib, importlib, inspect, json, random, struct, sys

HERE = Path(__file__).resolve().parent
fixture_source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-tintblend-root/validate_native.py')
nodes = []
for node in ast.parse(fixture_source.read_text()).body:
    if isinstance(node, ast.Assert):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(fixture_source), 'exec'), globals())
# The imported prefix sets HERE from __file__, which remains this script.
assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4

# Actual hardware-init BSS includes its 400-KiB backing buffer. Pack sections by
# their real COFF extents; the shared fixture's fixed 64-KiB spacing cannot load
# that object. This changes addresses only, not any product instructions/data.
def private_loader(cls, declaration, assignment, replacement):
    code = inspect.getsource(cls)
    assert code.count(assignment) == 1
    code = code.replace(assignment, replacement)
    code = code.replace('assert s[3] < 0x10000', 'assert s[3] < 0x1000000')
    namespace = dict(vars(importlib.import_module(cls.__module__)))
    exec(compile(code, declaration, 'exec'), namespace)
    return namespace[cls.__name__]

PackedCoff = private_loader(F.Coff, 'private-packed-coff',
    'bases = {sec: 0x83800000 + i*0x10000 for i, sec in enumerate(sorted(used))}',
    '''bases = {}
        cursor = 0x83800000
        for sec in sorted(used):
            bases[sec] = cursor
            cursor += max(16, (self.sections[sec][3] + 15) & ~15)''')
PackedBundle = private_loader(F.Bundle, 'private-packed-bundle',
    'bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}',
    '''bases = {}
        cursor = 0x83800000
        for key in sorted(used):
            bases[key] = cursor
            cursor += max(16, (self.objects[key[0]].sections[key[1]][3] + 15) & ~15)''')

report_path=HERE/'native-final/compile-results.json'
report=json.loads(report_path.read_text(encoding='utf-8'));paths={Path(r['source']).name:Path(r['object'])for r in report['results']}
prereq_path=HERE/'program-prereqs/compile-results.json';prereq=json.loads(prereq_path.read_text(encoding='utf-8'));extra={Path(r['source']).name:Path(r['object'])for r in prereq['results']}
product=PackedCoff(paths['BrnPostFxBloom.cpp'],'?Construct@BrnPostFxBloom@')
objs=[product,PackedCoff(paths['vertexdescriptorparameters.cpp'],'??0Parameters@VertexDescriptor@renderengine@@'),PackedCoff(extra['BaseResourceDescriptor.cpp'],'??0BaseResourceDescriptor@rw@@'),PackedCoff(extra['rwgpfxhelper.cpp'],'?CreateProgram@PfxHelper@')]
bundle=PackedBundle(objs);bundle.select('?Construct@BrnPostFxBloom@')
packages=[('gacBloomDSVertexProgram',0x8203E6F8,348,0),('gacBloomDSPixelProgram',0x8203E858,516,1),('gacBloomBlurVertexProgram',0x8203EA60,368,0),('gacBloomBlurPixelProgram',0x8203EBD0,420,1),('gacBloomBlurOldVertexProgram',0x8203ED78,404,0),('gacBloomBlurOldPixelProgram',0x8203EF10,424,1)]
package_addresses={};c=cpu(bundle);images=[]
for name,original,size,kind in packages:
 address=F.address(bundle,name);data=c.mem.rd(address,size);assert data==F.M.V.x360rd.rd(original,size),(name,address)
 package_addresses[original]=name;package_addresses[address]=name
 images.append({'name':name,'original':hex(original),'compiled':hex(address),'size':size,'sha256':hashlib.sha256(data).hexdigest(),'kind':kind})
BASE,SELF,ALLOC,VT,DATA=0x57000000,0x57000100,0x57000300,0x57000400,0x57001000
rng=random.Random(0x82401628);errors=[];cases=128
for index in range(cases):
 initial=bytearray(rng.randbytes(0xC000));struct.pack_into('>I',initial,ALLOC-BASE,VT);struct.pack_into('>I',initial,VT-BASE+0x10,BASE+0xF000)
 results=[]
 for compiled in [False,True]:
  c=cpu(bundle);c.mem.add(BASE,len(initial),initial);trace=[];allocation=[0];lookup=[0]
  def words(state,address,count):return tuple(struct.unpack('>'+str(count)+'I',state.mem.rd(address,count*4)))
  def params(state,address):
   w=words(state,address,10);assert w[0]in package_addresses,tuple(hex(t)for t in w)
   return(package_addresses[w[0]],w[1],w[2],w[4],w[6],w[8],w[9])
  def vdesc(state):
   raw=state.mem.rd(state.g(4)&0xFFFFFFFF,0x110)
   trace.append(('VertexDescriptor',raw.hex()))
   state.mem.wr(state.g(3)&0xFFFFFFFF,struct.pack('>10I',80,16,0,1,0,1,0,1,0,1))
  def progdesc(state):
   p=params(state,state.g(4)&0xFFFFFFFF);trace.append(('ProgramDescriptor',p))
   size=0x100+p[2];state.mem.wr(state.g(3)&0xFFFFFFFF,struct.pack('>10I',size,16,0,1,0x80,128,0,1,0,1))
  def allocate(state):
   a=allocation[0];allocation[0]+=1
   trace.append(('Allocate',state.g(4)&0xFFFFFFFF,words(state,state.g(5)&0xFFFFFFFF,10),state.g(6)&0xFFFFFFFF))
   state.mem.wr(state.g(3)&0xFFFFFFFF,struct.pack('>5I',*(DATA+a*0x1000+j*0x100 for j in range(5))))
  def vinit(state):
   trace.append(('VertexInitialize',words(state,state.g(3)&0xFFFFFFFF,5),state.mem.rd(state.g(4)&0xFFFFFFFF,0x110).hex()))
   state.setg(3,DATA)
  def pinit(state):
   resource=words(state,state.g(3)&0xFFFFFFFF,5);p=params(state,state.g(4)&0xFFFFFFFF)
   trace.append(('ProgramInitialize',resource,p));state.setg(3,resource[0])
  def handle(state):
   n=lookup[0];lookup[0]+=1
   trace.append(('Handle',state.g(3)&0xFFFFFFFF,F.cstring(state,state.g(4)&0xFFFFFFFF)))
   F.put(state,state.g(5)&0xFFFFFFFF,0x10203040+n);state.setg(3,1)
  c.hooks[BASE+0xF000]=allocate
  c.hooks[0x82B637D0]=vdesc;c.hooks[0x82B63060]=vinit;c.hooks[0x82B634B0]=progdesc;c.hooks[0x82B62478]=pinit;c.hooks[0x82B609B0]=handle
  for address,n in bundle.unresolved.items():
   if n.startswith('?GetResourceDescriptor@VertexDescriptor@'):c.hooks[address]=vdesc
   elif n.startswith('?Initialize@VertexDescriptor@'):c.hooks[address]=vinit
   elif n.startswith('?GetResourceDescriptor@ProgramBuffer@'):c.hooks[address]=progdesc
   elif n.startswith('?Initialize@ProgramBuffer@'):c.hooks[address]=pinit
   elif n.startswith('?GetVariableHandleByName@ProgramBuffer@'):c.hooks[address]=handle
   elif n.startswith('?FireAssert@'):c.hooks[address]=lambda state:trace.append(('ASSERT',))
   elif n.startswith(('?BeginAssert@','?EndAssert@')):c.hooks[address]=lambda state:None
  c.setg(3,SELF);c.setg(4,ALLOC)
  try:c.run(bundle.entry if compiled else 0x82401628)
  except Exception:
   print('FAIL',index,compiled,hex(getattr(c,'last_pc',0)),flush=True);raise
  results.append((c.mem.rd(BASE,len(initial)),trace,allocation[0],lookup[0]))
 if results[0]!=results[1]:errors.append({'index':index,'different_bytes':[i for i,(a,b)in enumerate(zip(results[0][0],results[1][0]))if a!=b],'original_trace':results[0][1],'compiled_trace':results[1][1]})
result={'cases':cases,'mismatches':errors,'packages':images,'objects':{str(o.path):sha(o.path)for o in objs},'report_sha256':sha(report_path),'prereq_report_sha256':sha(prereq_path),'verifier_sha256':sha(__file__),'limits':'Actual whole original/native Bloom constructor, real linked PfxHelperCreateProgram, VertexParameters and BaseResourceDescriptor constructors; six actual emitted shader packages byte-identical to original. Allocator, vertex/program descriptor/init and handle lookup boundaries controlled, only package pointer relocations mapped to named identities. Entire poisoned object/resource/guard bytes and ordered parameter/allocation/lookup traces compared. Params compares seven defined words, three native-unused words excluded. Does not execute XG/D3D GPU allocation/program parsing, failure/assert paths, full startup/device/frame/game or global byte match.'}
p=HERE/'constructor-final-comparison.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps({'cases':cases,'mismatches':len(errors),'shader_packages':len(images)}),flush=True);assert not errors
