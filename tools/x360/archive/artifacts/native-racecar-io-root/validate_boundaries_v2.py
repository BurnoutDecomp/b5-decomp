from pathlib import Path
import ast,hashlib,json,random,struct,sys
HERE=Path(__file__).resolve().parent
source=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text(encoding='utf-8')).body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in node.targets):break
 nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report_path=HERE/'native-candidate-1/compile-results.json'
report=json.loads(report_path.read_text(encoding='utf-8'))
BASE,SELF,CAR,OUT,QUEUE=0x57000000,0x57000100,0x57030000,0x57031000,0x57034000
rng=random.Random(0x822B3F88);errors=[];details=[]
selected=sys.argv[1]if len(sys.argv)>1 else 'all'
for kind,name,prefix,original,cases in [
 ('addcar','BrnRCEntityActiveRaceCarOutputInterface.cpp','?AddCarToRace@',0x822B3F88,256),
 ('parking','BrnRaceCarEntityModule_NearMissTailgate.cpp','?UpdatePowerParking@',0x822FF5B0,128)]:
 if selected not in ('all',kind):continue
 row=next(r for r in report['results']if Path(r['source']).name==name)
 obj=PackedCoff(Path(row['object']),prefix)
 for index in range(cases):
  image=bytearray(rng.randbytes(0x36000))
  def put32(address,value):struct.pack_into('>I',image,address-BASE,value&0xFFFFFFFF)
  if kind=='addcar':
   car_type=index%3;slot=index%8;count=(index//3)%8;global_slot=index%35
   image[CAR-BASE+0xA4]=car_type;image[CAR-BASE+0xAC]=slot
   put32(OUT+0x200,count)
  else:
   slot=index%8;mode=index%20;step=f32(rng.uniform(0.0,0.1))
   put32(SELF+0x182F8,slot);put32(SELF+0x18368,mode)
   struct.pack_into('>f',image,SELF-BASE+0x18398,step)
  results=[]
  for compiled in (False,True):
   c=cpu(obj);c.mem.add(BASE,len(image),image);trace=[]
   def get_queue(state):
    trace.append(('queue',state.g(3)&0xFFFFFFFF))
    state.setg(3,QUEUE)
   def update(state):
    trace.append(('update',state.g(3)&0xFFFFFFFF,state.g(4)&0xFFFFFFFF,fbits(state.f(1)),state.g(6)&0xFFFFFFFF,state.g(7)&0xFFFFFFFF,state.g(8)&0xFFFFFFFF))
    F.put(state,(state.g(3)&0xFFFFFFFF)+4,0x13579BDF^index)
   def assertion(state):trace.append(('ASSERT',))
   c.hooks[0x822B67D0]=get_queue;c.hooks[0x822F8400]=update
   for address,n in obj.unresolved.items():
    if n.startswith('?GetGameEventQueue@OutputBuffer_PostPhysics@'):c.hooks[address]=get_queue
    elif n.startswith('?Update@PowerParkingManager@'):c.hooks[address]=update
    elif n.startswith('?FireAssert@'):c.hooks[address]=assertion
    elif n.startswith(('?BeginAssert@','?EndAssert@')):c.hooks[address]=lambda state:None
   c.setg(3,OUT if kind=='addcar'else SELF)
   c.setg(4,CAR if kind=='addcar'else 0xDEAD0004)
   c.setg(5,global_slot if kind=='addcar'else OUT)
   try:c.run(obj.entry if compiled else original)
   except Exception:
    print('FAIL',kind,index,compiled,hex(getattr(c,'last_pc',0)),flush=True);raise
   data=bytearray(c.mem.rd(BASE,len(image)))
   if kind=='addcar':
    # The original copies64B including uninitialized local tail padding. Its7
    # undefined bytes are not evidence for defined behavior or game layout.
    start=OUT-BASE+count*0x40+0x39;data[start:start+7]=bytes(7)
   results.append((bytes(data),trace))
  if results[0]!=results[1]:
   errors.append({'kind':kind,'index':index,'differing_bytes':[i for i,(a,b)in enumerate(zip(results[0][0],results[1][0]))if a!=b],'original_trace':results[0][1],'compiled_trace':results[1][1]})
 details.append({'kind':kind,'cases':cases,'original':hex(original),'object':str(obj.path),'object_sha256':sha(obj.path),'symbol':obj.fn[0]})
 print(kind,cases,'checked',flush=True)
result={'cases':sum(r['cases']for r in details),'groups':details,'mismatches':errors,'native_report_sha256':sha(report_path),'verifier_sha256':sha(__file__),'limits':'Actual whole original/product AddCarToRace including actual original getters/Append and compiled inline getters/Append, full raw position/previous/velocity quads, types0..2, active0..7, global0..34,count0..7, complete mapped state/guards with seven undefined appended-payload tail-padding bytes excluded explicitly. No float math in raw copies. Whole original/product UpdatePowerParking caller and real original GetActiveRaceCar/native inline index calculation; event-queue accessor and PowerParkingManager::Update services controlled, ordered service trace/argument bits/full mapped state/guards compared. No actual PowerParkingManager gameplay, queue internals, invalid indices/assert metadata, hardware/frame/game claim.'}
out=HERE/('caller-boundaries-v2-'+selected+'.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':result['cases'],'mismatches':len(errors)}),flush=True)
assert not errors
