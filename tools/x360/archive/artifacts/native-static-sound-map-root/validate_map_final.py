from pathlib import Path
import ast,json
HERE=Path(__file__).resolve().parent
source=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text(encoding='utf-8')).body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in node.targets):break
 nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report=json.loads((HERE/'native-candidate-1/compile-results.json').read_text(encoding='utf-8'))
report_path=HERE/'native-final/compile-results.json'
report=json.loads(report_path.read_text(encoding='utf-8'))
paths={Path(r['source']).name:Path(r['object'])for r in report['results']if r['passed']}
BASE,SELF,POINT,RECT,CELLS,ENTITIES,OUT,RESOURCE=0x57000000,0x57000100,0x57000200,0x57000240,0x57001000,0x57002000,0x57003000,0x57004000
ctor_report_path=HERE/'descriptor-provider/compile-results.json'
ctor_report=json.loads(ctor_report_path.read_text(encoding='utf-8'))
ctor_path=Path(ctor_report['results'][0]['object'])
rng=random.Random(0x8267AF10);errors=[];details=[]
for kind,name,prefix,original in [
 ('cell','BrnStaticSoundMap.cpp','?GetSubRegionDescrip@',0x8267AF10),
 ('range','BrnStaticSoundMap.cpp','?IsInRange@',0x82677570),
 ('entity','BrnStaticSoundMap.cpp','?GetEntity@',0x82675578),
 ('descriptor','BrnStaticSoundMapResourceType.cpp','?GetSerialisedResourceDescriptor@',0x8267AFC0),
 ('fixup','BrnStaticSoundMapResourceType.cpp','?FixUp@',0x826775C8)]:
 product=PackedCoff(paths[name],prefix)
 if kind=='descriptor':
  obj=PackedBundle([product,PackedCoff(ctor_path,'??0BaseResourceDescriptor@rw@@')])
  obj.select(prefix);obj.path=product.path;obj.fn=product.fn
 else:obj=product
 cases=256
 for index in range(cases):
  image=bytearray(rng.randbytes(0x5000))
  def put32(address,value):struct.pack_into('>I',image,address-BASE,value&0xFFFFFFFF)
  nx,nz=rng.randrange(1,9),rng.randrange(1,9);size=float(rng.choice([0.25,0.5,1,2,8]))
  minx,minz=rng.randrange(-32,33)*0.25,rng.randrange(-32,33)*0.25
  maxx,maxz=minx+nx*size,minz+nz*size
  struct.pack_into('>4f',image,SELF-BASE,minx,minz,-17,21)
  struct.pack_into('>4f',image,SELF-BASE+0x10,maxx,maxz,22,-18)
  struct.pack_into('>f',image,SELF-BASE+0x20,size)
  put32(SELF+0x24,CELLS);put32(SELF+0x28,nx);put32(SELF+0x2c,nz)
  count=index%9;put32(SELF+0x30,ENTITIES);put32(SELF+0x34,count);put32(SELF+0x38,index%2)
  x,z=rng.uniform(minx-size,maxx+size),rng.uniform(minz-size,maxz+size)
  if index%8==0:x,z=minx,minz
  if index%8==1:x,z=maxx,maxz
  if index%8==2:x,z=maxx,minz
  if index%8==3:x,z=minx,maxz
  if index%8==4:x,z=minx+0.5*size,minz+0.5*size
  if index%16==5:x=float('nan')
  if index%16==6:z=float('inf')
  struct.pack_into('>4f',image,POINT-BASE,x,-0.0,z,-7)
  struct.pack_into('>4f',image,RECT-BASE,minx,minz,maxx,maxz)
  radius=f32(rng.uniform(-4,8))
  if kind=='fixup':
   put32(SELF+0x24,0x1000);put32(SELF+0x30,0x2000);put32(RESOURCE,BASE)
  if kind=='entity':
   count=index%8+1;put32(SELF+0x34,count);entity=index%count
  results=[]
  for compiled in (False,True):
   c=cpu(obj);c.mem.add(BASE,len(image),image);trace=[]
   for address,symbol in obj.unresolved.items():
    if symbol.startswith('?FireAssert@'):c.hooks[address]=lambda state:trace.append(('ASSERT',))
    elif symbol.startswith(('?BeginAssert@','?EndAssert@')):c.hooks[address]=lambda state:None
   if kind=='cell':c.setg(3,SELF);c.setg(4,POINT);c.setg(5,0xDEAD0005)
   elif kind=='range':c.setg(3,POINT);c.setf(1,radius);c.setg(4,0xDEAD0004);c.setg(5,RECT)
   elif kind=='entity':c.setg(3,SELF);c.setg(4,entity)
   elif kind=='descriptor':c.setg(3,OUT);c.setg(4,0xDEAD0004);c.setg(5,SELF)
   else:c.setg(3,0xDEAD0003);c.setg(4,SELF);c.setg(5,RESOURCE)
   try:c.run(obj.entry if compiled else original)
   except Exception:
    print('FAIL',kind,index,compiled,hex(getattr(c,'last_pc',0)),flush=True);raise
   results.append((c.g(3)&0xFFFFFFFF if kind!='fixup'else None,c.mem.rd(BASE,len(image)),trace))
  if results[0]!=results[1]:
   errors.append({'kind':kind,'index':index,'original_return':results[0][0],'compiled_return':results[1][0],'original_trace':results[0][2],'compiled_trace':results[1][2],'different_bytes':[i for i,(a,b)in enumerate(zip(results[0][1],results[1][1]))if a!=b]})
 details.append({'kind':kind,'cases':cases,'original':hex(original),'symbol':obj.fn[0],'object':str(obj.path),'object_sha256':sha(obj.path)})
 print(kind,cases,'checked',flush=True)
result={'cases':sum(d['cases']for d in details),'groups':details,'mismatches':errors,'native_report_sha256':sha(report_path),'descriptor_provider_report_sha256':sha(ctor_report_path),'descriptor_constructor_object_sha256':sha(ctor_path),'verifier_sha256':sha(__file__),'limits':'Actual full original and whole compiled product map/resource methods with native scalar-vector reference/static radius ABI and hidden descriptor return. Positivecellsize/sane grid, finite+edge query values plus NaN/Inf position reject checks; arbitrary raw entity/bounds unused lanes. All mapped map/input/descriptor/entity/cell/resource/guard bytes and non-void returns compared. ResourceFixUp executes actual segment0 relocation/valid entity assert loop. Descriptor executes actual linked BaseResourceDescriptor constructor; the initial verifier could not resolve that real dependency and is preserved separately. No invalid-grid integer conversion, invalid assertion metadata, end-to-end streaming/audio/hardware/frame/game claim.'}
p=HERE/'map-resource-comparison-final.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':result['cases'],'mismatches':len(errors)}),flush=True)
assert not errors
