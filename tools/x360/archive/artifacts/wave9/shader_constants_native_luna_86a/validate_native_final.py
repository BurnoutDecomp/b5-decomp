from pathlib import Path
import ast,json
HERE=Path(__file__).resolve().parent
source=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text(encoding='utf-8')).body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in node.targets):break
 nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report=json.loads((HERE/'final-compile/objects/compile-results.json').read_text(encoding='utf-8'))
report_path=HERE/'final-compile/objects/compile-results.json'
report=json.loads(report_path.read_text(encoding='utf-8'))
paths={Path(r['source']).name:Path(r['object'])for r in report['results']if r['passed']}
BASE,SELF,SOURCE,DEST,BIN=0x57000000,0x57000100,0x57002000,0x57008000,0x57011000
w=F.M.V.x360rd.u32(0x822A0AA8);li=w&0x03FFFFFC;li=li-(1<<26)if li&(1<<25)else li
allocate_original=(0x822A0AA8+li)&0xFFFFFFFF
rng=random.Random(0x822B3458);errors=[];details=[]
for kind,prefix,original,cases in [
 ('plus','?SetShaderConstantArrayData@ShaderConstantTable@@QAAXIPBVVector3Plus@vpu@math@rw@@@Z',0x822B3458,256),
 ('affine','?SetShaderConstantArrayData@ShaderConstantTable@@QAAXIPBUMatrix44Affine@vpu@math@rw@@@Z',0x827FB918,256),
 ('serialise','?ShouldSerialise@',0x827E99F8,256)]:
 obj=PackedCoff(paths['CgsShaderConstants.cpp'if kind=='serialise'else'CgsShaderConstantTable.cpp'],prefix)
 for index in range(cases):
  image=bytearray(rng.randbytes(0x12000));slot=index%50
  def put32(address,value):struct.pack_into('>I',image,address-BASE,value&0xFFFFFFFF)
  if kind!='serialise':
   count=([1,2,7,8,9,16,31,64,128,255][index%10]if kind=='plus'else index%9+1)
   quads=count*(4 if kind=='affine'else 1)
   struct.pack_into('>HBB',image,SELF-BASE+slot*12,quads,64 if kind=='affine'else 16,count)
   image[SELF-BASE+0x578]=50;image[SELF-BASE+0x580]=index%64
   put32(SELF+0x57C,BIN)
  else:
   count=index%6;match=(index//6)%(count+1)
   put32(SELF+4,count);put32(SELF+8,BASE+0x5000);put32(SELF+12,BASE+0x5100)
   for i in range(count):
    name=(b'AnimDuration'if i==match else [b'Other',b'AnimDuratio',b'AnimDurationX'][i%3])+b'\0'
    put32(BASE+0x5100+i*4,BASE+0x5200+i*0x20)
    image[0x5200+i*0x20:0x5200+i*0x20+len(name)]=name
    put32(BASE+0x5000+i*4,BASE+0x5400+i*0x10)
    if i==match:
     bits=[0,0x80000000,0x3F800000,0xBF800000,1,0x7F800000,0x7FC12345,0x7FA12345][(index//7)%8]
     put32(BASE+0x5400+i*0x10,bits)
  results=[]
  for compiled in (False,True):
   c=cpu(obj);c.mem.add(BASE,len(image),image);trace=[];c.max_steps=2000000
   def allocate(state):
    trace.append(('allocate',state.g(3)&0xFFFFFFFF,state.g(4)&0xFFFFFFFF))
    state.setg(3,DEST)
   c.hooks[allocate_original]=allocate
   for address,name in obj.unresolved.items():
    if name.startswith('?AllocateMemoryFast@'):c.hooks[address]=allocate
    elif name.startswith('?FireAssert@'):c.hooks[address]=lambda state:trace.append(('ASSERT',))
    elif name.startswith(('?BeginAssert@','?EndAssert@')):c.hooks[address]=lambda state:None
   c.setg(3,SELF);c.setg(4,slot);c.setg(5,SOURCE)
   try:c.run(obj.entry if compiled else original)
   except Exception:
    print('FAIL',kind,index,compiled,hex(getattr(c,'last_pc',0)),flush=True);raise
   results.append((c.g(3)&0xFFFFFFFF if kind=='serialise'else None,c.mem.rd(BASE,len(image)),trace))
  if results[0]!=results[1]:
   errors.append({'kind':kind,'index':index,'count':count,'slot':slot,'original_return':results[0][0],'compiled_return':results[1][0],'different_bytes':[i for i,(a,b)in enumerate(zip(results[0][1],results[1][1]))if a!=b],'original_trace':results[0][2],'compiled_trace':results[1][2]})
 details.append({'kind':kind,'cases':cases,'original':hex(original),'symbol':obj.fn[0],'object':str(obj.path),'object_sha256':sha(obj.path)})
 print(kind,cases,'checked',flush=True)
result={'cases':sum(d['cases']for d in details),'groups':details,'mismatches':errors,'native_report_sha256':sha(report_path),'verifier_sha256':sha(__file__),'limits':'Actual whole original/native Plus array upload including real dirty-table/update/copy body; native typed Plus symbol is directly defined, arbitrary raw128-bit lanes including NaN payloads and ±zero, valid slot0..49/count1..255 and varied dirty counts. Affine upload actual whole methods including full rawXYZ copies and prescribedW0,0,0,1, counts1..9. Allocation boundary controlled with exact bin/quad-count trace; full mapped table/source/dest/guards compared. CPUShouldSerialise executes original/native real GetValue bytewise lookup and raw vector copy, names/count0..5 and first-lane zero/negativezero/normal/subnormal/inf/NaN. No invalid-count/assert metadata, aliasing/overlapping inputs, floating status, actual DispatchBin allocator/GPU/frame/game or full serialized-resource fixup proof.'}
out=HERE/'native-comparison-final.json';assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':result['cases'],'mismatches':len(errors)}),flush=True)
assert not errors
