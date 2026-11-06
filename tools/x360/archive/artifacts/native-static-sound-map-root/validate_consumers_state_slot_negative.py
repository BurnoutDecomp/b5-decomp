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
import math
report_path=HERE/'native-final/compile-results.json'
r=json.loads(report_path.read_text(encoding='utf-8'));paths={Path(t['source']).name:Path(t['object'])for t in r['results']}
BASE,SELF,QUERY,MAP,CELLS,ENTITIES,OUT=0x57000000,0x57000100,0x57001000,0x57001200,0x57002000,0x57003000,0x57004000
rng=random.Random(0x826D18F0);errors=[];details=[]
for kind in ['query','history','emitter']:
 if kind=='query':
  scene=PackedCoff(paths['BrnSoundWorldScene.cpp'],'?QuerySoundMap@')
  obj=PackedBundle([scene,PackedCoff(paths['BrnStaticSoundMap.cpp'],'?GetSubRegionDescrip@'),PackedCoff(paths['BrnStaticSoundMap.cpp'],'?GetEntity@')]);obj.select('?QuerySoundMap@')
  original=0x826D18F0;product=scene
 elif kind=='history':
  obj=product=PackedCoff(paths['BrnStaticPassbyControl.cpp'],'?Record@PassbyHistory@');original=0x8269B7D0
 else:
  obj=product=PackedCoff(paths['BrnEmitterStateManager.cpp'],'?GetFreeState@EmitterStateManager@');original=0x8269C200
 cases=128
 for index in range(cases):
  image=bytearray(rng.randbytes(0x10000))
  def put32(address,value):struct.pack_into('>I',image,address-BASE,value&0xFFFFFFFF)
  if kind=='query':
   struct.pack_into('>3f',image,QUERY-BASE,rng.randrange(-8,17)*0.5,rng.randrange(-4,5)*0.25,rng.randrange(-8,17)*0.5)
   struct.pack_into('>f',image,QUERY-BASE+0xC,float(index%3))
   put32(QUERY+0x10,OUT);put32(QUERY+0x14,index%9);put32(QUERY+0x18,index%3);image[QUERY-BASE+0x1C]=0
   struct.pack_into('>4f',image,MAP-BASE,-4,-4,5,6);struct.pack_into('>4f',image,MAP-BASE+16,8,8,-5,-6)
   struct.pack_into('>f',image,MAP-BASE+0x20,4.0);put32(MAP+0x24,CELLS);put32(MAP+0x28,3);put32(MAP+0x2C,3);put32(MAP+0x30,ENTITIES);put32(MAP+0x34,4)
   for cell in range(9):struct.pack_into('>hh',image,CELLS-BASE+cell*4,0,0 if(cell+index)%5==0 else 4)
   for entity in range(4):
    struct.pack_into('>3f',image,ENTITIES-BASE+entity*16,*(rng.randrange(-8,17)*0.5 for _ in range(3)))
    put32(ENTITIES+entity*16+12,((index+entity)%19<<16)|(1+(index+entity)%7))
  elif kind=='history':
   count=index%6;put32(SELF+0xA0,count)
   point=[rng.randrange(-20,21)*0.25 for _ in range(4)]
   for i in range(5):struct.pack_into('>4f',image,SELF-BASE+i*32,*(rng.randrange(-40,41)*0.25 for _ in range(4)))
   if index%4==0 and count:struct.pack_into('>4f',image,SELF-BASE,*point)
   if index%4==1 and count:struct.pack_into('>4f',image,SELF-BASE,point[0],point[1],point[2],point[3]+1000)
  else:
   states=[BASE+0x2000+i*0x100 for i in range(index%5)]
   put32(SELF+0x18,states[0]if states else 0)
   struct.pack_into('>4f',image,SELF-BASE+0x430,*(rng.randrange(-40,41)*0.25 for _ in range(4)))
   struct.pack_into('>4f',image,ENTITIES-BASE,*(rng.randrange(-40,41)*0.25 for _ in range(4)))
   put32(BASE+0x5018,BASE+0x6000)
   for i,address in enumerate(states):
    put32(address,BASE+0x5000);put32(address+0x18,states[i+1]if i+1<len(states)else 0)
    image[address-BASE+0x48]=0 if index%4==0 and i==len(states)-1 else 1
    struct.pack_into('>4f',image,address-BASE+0x60,*(rng.randrange(-80,81)*0.25 for _ in range(4)))
  results=[]
  for compiled in [False,True]:
   c=cpu(obj);c.mem.add(BASE,len(image),image);trace=[];c.max_steps=2000000
   def ceiling(state):state.setf(1,float(math.ceil(state.f(1))))
   def base_free(state):
    p=state.rd32((state.g(3)&0xFFFFFFFF)+0x18)
    while p and state.mem.rd(p+0x48,1)[0]:p=state.rd32(p+0x18)
    state.setg(3,p)
   def detach(state):
    p=state.g(3)&0xFFFFFFFF;trace.append(('detach',p))
    ok=((p//0x100+index)&1)!=0
    if ok:state.mem.wr(p+0x48,bytes([0]))
    state.setg(3,int(ok))
   word=F.M.V.x360rd.u32(0x826D1940);off=word&0x03FFFFFC;off=off-(1<<26)if off&(1<<25)else off
   c.hooks[(0x826D1940+off)&0xFFFFFFFF]=ceiling;c.hooks[BASE+0x6000]=detach
   for address,n in obj.unresolved.items():
    if n=='ceil':c.hooks[address]=ceiling
    elif n.startswith('?GetFreeState@StateManager@'):c.hooks[address]=base_free
    elif n.startswith('?FireAssert@'):c.hooks[address]=lambda state:trace.append(('ASSERT',))
    elif n.startswith(('?BeginAssert@','?EndAssert@','__savefpr_','__restfpr_')):c.hooks[address]=lambda state:None
   c.setg(3,SELF)
   if kind=='query':c.setg(4,QUERY);c.setg(5,MAP)
   elif kind=='history':c.setvf(1,point)
   else:c.setg(4,ENTITIES)
   try:c.run(obj.entry if compiled else original)
   except Exception:
    print('FAIL',kind,index,compiled,hex(getattr(c,'last_pc',0)),flush=True);raise
   data=bytearray(c.mem.rd(BASE,len(image)))
   if kind=='history'and c.g(3)&0xFF:
    # Only12 undefined tail bytes in the newly appended local32-byte record.
    a=SELF-BASE+count*32+20;data[a:a+12]=bytes(12)
   results.append((None if kind=='query'else c.g(3)&0xFFFFFFFF,bytes(data),trace))
  if results[0]!=results[1]:errors.append({'kind':kind,'index':index,'original_return':results[0][0],'compiled_return':results[1][0],'original_trace':results[0][2],'compiled_trace':results[1][2],'different_bytes':[i for i,(a,b)in enumerate(zip(results[0][1],results[1][1]))if a!=b]})
 details.append({'kind':kind,'cases':cases,'original':hex(original),'object':str(product.path),'object_sha256':sha(product.path),'symbol':product.fn[0]})
 print(kind,cases,'checked',flush=True)
result={'cases':sum(t['cases']for t in details),'groups':details,'mismatches':errors,'native_report_sha256':sha(report_path),'verifier_sha256':sha(__file__),'limits':'Actual whole QuerySoundMap plus real linked map GetSubRegion/GetEntity, scalar32-byte QueryInfo and full raw16-byte entities, valid3x3 grid/cell and output capacity/early-stop; debugdisabled, ceil and CRT nonvolatile convention modeled. Whole history Record with v1 position, valid count0..5, full mappedstate/guards/return excluding12 undefined newrecord tailpadding. Whole emitter GetFreeState with actual430camera offset, field/position/dot/list traversal; base first-free search and virtual Detach controlled, valid attached-state inputs. No debugcallbacks/hardware/audio, invalid assertion metadata, nonfinite arithmetic, full active frame or game claim.'}
p=HERE/'consumer-comparison.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print(json.dumps({'cases':result['cases'],'mismatches':len(errors)}),flush=True);assert not errors
