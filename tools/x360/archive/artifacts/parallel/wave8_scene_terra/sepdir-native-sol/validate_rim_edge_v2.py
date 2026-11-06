from pathlib import Path
import ast,hashlib,json,math,random,struct,sys
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent
row=json.loads((OUT/'candidate-v2/compile-results.json').read_text())['results'][0]
obj=PackedCoff(Path(row['object']),'?RimToEdge@');bundle=PackedBundle([obj]);bundle.select('?RimToEdge@')
bits=lambda f:struct.unpack('>I',struct.pack('>f',f))[0]
f32=lambda f:struct.unpack('>f',struct.pack('>f',f))[0]
frombits=lambda n:struct.unpack('>f',struct.pack('>I',n))[0]
# Native ctor82C73DA8 writes FLT_MIN to the shared rim guard row. The same
# post-CRT program state is used here; this is not a guessed threshold.
def basecpu():
 c=cpu(bundle);c.mem.wr(F.M.S.STACK,b'\xA5'*0x20000)
 c.mem.add(0x8327EFB0,16,struct.pack('>4I',*[0x00800000]*4))
 return c
rng=random.Random(0x82BB53F8);out=[];bad=[];errors=[]
for i in range(96):
 centre=[f32(rng.uniform(-5,5))for _ in range(4)]
 axis=[[0,0,1,0],[0,1,0,1],[1,0,0,-.5],[.6,.8,0,.3]][i%4]
 radius=[f32(rng.uniform(.1,4))]*4
 p1=[f32(rng.uniform(-5,5))for _ in range(4)];p2=[f32(rng.uniform(-5,5))for _ in range(4)]
 if i%12==0:p2=p1[:]
 if i%12==1:p1=centre[:]
 if i%12==2:radius=[0]*4
 if i%12==3:p2=[f32(p1[k]+axis[k])for k in range(4)]
 if i%12==4:radius=[.2,.4,.8,1.6]
 if i>=80:
  v=frombits([0x7fc12345,0x7f800000,0xff800000,1][i%4]);p1[i%4]=v
 outputs=[]
 for compiled in [False,True]:
  c=basecpu()
  for k,v in enumerate([centre,axis,radius,p1,p2]):c.vr[k+1]=tuple(map(bits,v))
  try:c.run(bundle.entry if compiled else 0x82BB53F8);outputs.append(list(c.vr[1]))
  except Exception as e:errors.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  rec={'case':i,'inputs':[list(map(bits,v))for v in [centre,axis,radius,p1,p2]],'original':outputs[0],'compiled':outputs[1]};out.append(rec)
  if outputs[0]!=outputs[1]:bad.append(rec)
p=OUT/'rim-edge-v2-diagnostic.json';assert not p.exists();p.write_text(json.dumps({'compared':len(out),'mismatches':bad,'exceptions':errors,'cases':out,'object':str(obj.path),'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest()},indent=2)+'\n')
print('compared',len(out),'bad',len(bad),'exceptions',len(errors));print('first',bad[:1],errors[:1]);print('unresolved',bundle.unresolved)
