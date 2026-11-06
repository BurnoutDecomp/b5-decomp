from pathlib import Path
import ast,collections,hashlib,json,random,struct,sys
HERE=Path(__file__).resolve().parent
source=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text(encoding='utf-8')).body:
 if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in node.targets):break
 nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report_path=HERE/'native-candidate-1/compile-results.json'
report=json.loads(report_path.read_text(encoding='utf-8'))
row=next(r for r in report['results']if Path(r['source']).name=='BrnRaceCarEntityModule_NearMissTailgate.cpp')
obj=PackedCoff(Path(row['object']),'?IsPlayerCarTailgatingOtherRaceCars@')
BASE,SELF,ACBASE=0x57000000,0x57000100,0x57020000
SIZE=0x31000
rng=random.Random(0x822CE260)
errors=[];outcomes=collections.Counter();cases=int(sys.argv[1])if len(sys.argv)>1 else 16
hooks_seen=collections.Counter()
for index in range(cases):
 image=bytearray(rng.randbytes(SIZE))
 player=index%8
 def packed(address,fmt,*args):struct.pack_into('>'+fmt,image,address-BASE,*args)
 for slot in range(8):
  ac=ACBASE+slot*0x1CD0
  packed(ac+0x740,'B',3 if rng.randrange(3) else 0)
  packed(ac+0x410,'4f',*(rng.randrange(-32,33)*0.25 for _ in range(4)))
  packed(ac+0x4AC,'f',rng.randrange(20,101))
 # Controlled active-car services publish these complete raw position/direction quads.
 positions=[struct.pack('>4f',*(rng.randrange(-80,81)*0.25 for _ in range(4)))for _ in range(8)]
 directions=[struct.pack('>4f',0,0,1,rng.randrange(-100,101)*0.25)for _ in range(8)]
 candidate=(player+1)%8
 if index%8<7:
  for slot in range(8):packed(ACBASE+slot*0x1CD0+0x740,'B',3 if slot in (player,candidate) else 0)
  positions[player]=struct.pack('>4f',0,0,0,-0.0)
  positions[candidate]=struct.pack('>4f',0,0,8,3.0)
  packed(ACBASE+player*0x1CD0+0x410,'4f',0,0,10,17)
  packed(ACBASE+candidate*0x1CD0+0x410,'4f',0,0,10,-18)
  packed(ACBASE+candidate*0x1CD0+0x4AC,'f',50)
  if index%8==0:packed(ACBASE+candidate*0x1CD0+0x740,'B',0)
  if index%8==2:positions[candidate]=struct.pack('>4f',0,0,100,3)
  if index%8==3:positions[candidate]=struct.pack('>4f',10,0,8,3)
  if index%8==4:packed(ACBASE+candidate*0x1CD0+0x4AC,'f',29)
  if index%8==5:packed(ACBASE+candidate*0x1CD0+0x410,'4f',0,0,110,-18)
  if index%8==6:positions[candidate]=struct.pack('>4f',0,0,-2,3) # zero separation: original has no fallback.
 packed(ACBASE+player*0x1CD0+0x740,'B',3)
 results=[]
 for compiled in (False,True):
  c=cpu(obj);c.mem.add(BASE,len(image),image);c.max_steps=2000000
  c.mem.add(0x82CDB4B0,0x20,bytes(0x20));c.mem.add(0x82FAD3F0,4,bytes(4))
  constants=[('KF_Z_OFFSET_FROM_CAR_CENTRE_TO_CONE_APEX',0x82CDB4B0,2.0),('KF_TAILGATING_CONE_HALF_ANGLE_RADS',0x82CDB4B4,0.346),('KF_TAILGATING_CONE_DEPTH',0x82CDB4B8,20.0),('KF_MIN_TAILGATEE_SPEED',0x82CDB4C0,30.0),('KF_TAILGATING_MAX_RELATIVE_VELOCITY_MAGNITUDE',0x82FAD3F0,f32(f32(0.44704)*90.0))]
  for fragment,original,value in constants:
   c.mem.wr(original,struct.pack('>f',value))
   for address,name in obj.unresolved.items():
    if fragment in name:c.mem.wr(address,struct.pack('>f',value))
  trace=[]
  def slot_of(address):
   offset=(address&0xFFFFFFFF)-ACBASE
   assert offset%0x1CD0==0 and 0<=offset//0x1CD0<8,(hex(address),hex(offset))
   return offset//0x1CD0
  def get_position(state):
   s=slot_of(state.g(4));state.mem.wr(state.g(3)&0xFFFFFFFF,positions[s]);trace.append(('position',s))
  def get_direction(state):
   s=slot_of(state.g(4));state.mem.wr(state.g(3)&0xFFFFFFFF,directions[s]);trace.append(('direction',s))
  def is_attached(state):state.setg(3,int(state.mem.rd((state.g(3)&0xFFFFFFFF)+0x740,1)[0]!=0))
  def is_active(state):state.setg(3,int(state.mem.rd((state.g(3)&0xFFFFFFFF)+0x740,1)[0]==3))
  def physics(state):state.setg(3,(state.g(3)+0xE0)&0xFFFFFFFF)
  def assertion(state):trace.append(('ASSERT',))
  originals={0x822CCF78:get_position,0x822CD038:get_direction,0x822A1F10:is_attached,0x822A1FB8:is_active,0x822B84E0:physics}
  c.hooks.update(originals)
  for address,name in obj.unresolved.items():
   if name.startswith('?GetPosition@ActiveRaceCar@'):c.hooks[address]=get_position
   elif name.startswith('?GetDirection@ActiveRaceCar@'):c.hooks[address]=get_direction
   elif name.startswith('?IsAttached@ActiveRaceCar@'):c.hooks[address]=is_attached
   elif name.startswith('?IsActive@ActiveRaceCar@'):c.hooks[address]=is_active
   elif name.startswith('?GetPhysicsState@ActiveRaceCar@'):c.hooks[address]=physics
   elif name.startswith('?FireAssert@'):c.hooks[address]=assertion
   elif name.startswith(('?BeginAssert@','?EndAssert@')):c.hooks[address]=lambda state:None
  c.setg(3,SELF);c.setg(4,player);c.setg(5,ACBASE)
  try:c.run(obj.entry if compiled else 0x822CE260)
  except Exception:
   print('FAIL',index,compiled,'pc',hex(getattr(c,'last_pc',0)),flush=True);raise
  results.append((c.g(3)&0xFFFFFFFF,c.mem.rd(BASE,len(image)),trace))
 if results[0]!=results[1]:
  errors.append({'index':index,'player':player,'original_return':results[0][0],'compiled_return':results[1][0],'original_latch':results[0][1][SELF-BASE+0x182F4:SELF-BASE+0x182F8].hex(),'compiled_latch':results[1][1][SELF-BASE+0x182F4:SELF-BASE+0x182F8].hex(),'original_trace':results[0][2],'compiled_trace':results[1][2],'differing_bytes':[i for i,(a,b)in enumerate(zip(results[0][1],results[1][1]))if a!=b]})
 outcomes[str(results[0][0])]+=1
 hooks_seen.update(t[0]for t in results[1][2])
result={'cases':cases,'mismatches':errors,'outcomes':dict(outcomes),'hooks_seen':dict(hooks_seen),'object':str(obj.path),'object_sha256':sha(obj.path),'native_report_sha256':sha(report_path),'verifier_sha256':sha(__file__),'limits':'Full ARTIST822CE260 and actual complete compiled product predicate, all mapped object/car/guard bytes plus return and ordered position/direction service trace compared. Active-car position/direction, active/attached and physics-state services controlled; valid states and integer/quarter finite xyz, raw varying W, explicit speed/cone/distance/relative-velocity rejects and zero-separation case. Actual inline XDK cosine executes, shared software VMX reciprocal-root estimate, floating-status/physical estimates/nonfinite XYZ/all real ActiveRaceCar providers/full frame or game are not established.'}
out=HERE/('tailgate-'+str(cases)+'.json');assert not out.exists();out.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print(json.dumps({'cases':cases,'mismatches':len(errors),'outcomes':dict(outcomes)}),flush=True)
assert not errors
