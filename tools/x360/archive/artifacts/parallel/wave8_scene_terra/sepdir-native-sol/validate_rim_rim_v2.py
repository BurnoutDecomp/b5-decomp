from pathlib import Path
import ast,hashlib,json,math,random,struct
p=Path(__file__).with_name('validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent;bundle.select('?RimToRim@')
rng=random.Random(0x82BB56C8);records=[];bad=[];errors=[];coverage={}
labels={0x82BB5740:'reject',0x82BB5850:'coplanar',0x82BB59F0:'general',0x82BB5A50:'heron',0x82BB5D14:'antipodal',0x82BB5E8C:'iteration',0x82BB5990:'coplanar_fallback',0x82BB5E64:'pick_low',0x82BB6070:'pick_high',0x82BB610C:'success'}
for i in range(160):
 c1=[f32(rng.uniform(-2,2))for _ in range(4)];c2=[f32(rng.uniform(-6,6))for _ in range(4)]
 a1=[0,0,1,.3];a2=[0,1,0,-.7];sep=[-1,0,0,1.1]
 r1=[f32(rng.uniform(.1,3))]*4;r2=[f32(rng.uniform(.1,3))]*4
 k=i%12
 if k==0:r1=[0]*4
 if k==1:c2=[c1[0],f32(c1[1]+4),f32(c1[2]+5),-.2]
 if k==2:c2=[c1[0],c1[1]+.5,c1[2]+.5,0]
 if k==3:c2=[c1[0]+.2,c1[1]+.5,c1[2]+.5,1]
 if k==4:r1=[1.2,.8,.9,.4];r2=[.8,1.6,.5,.3]
 if k==5:c2=c1[:]
 if k==6:a2=a1[:]
 if k==7:c2=[c1[0]+3,c1[1],c1[2],0]
 if k==8:a1=[.6,.8,0,1];a2=[0,.6,.8,-1];sep=[.64,-.48,.36,0]
 if k==9:r1=[-1]*4
 if k==10:c1=[0,0,0,0];c2=[0,0,1,0];r1=[1]*4;r2=[1]*4
 if k==11:c2=[c1[0],c1[1]+8,c1[2]+8,0]
 if i>=128:
  val=frombits([0x7fc12345,0x7f800000,0xff800000,1,0x80000000][i%5]);[c1,c2,a1,a2,r1,r2,sep][i%7][i%4]=val
 rows=[c1,a1,r1,c2,a2,r2,sep];region=0x57000000;target=region+0x180;image=bytearray(rng.randbytes(0x300))
 for j,v in enumerate(rows):struct.pack_into('>4f',image,0x10*j,*v)
 outputs=[]
 for compiled in [False,True]:
  c=basecpu();c.mem.add(region,len(image),image)
  for j in range(7):c.setg(3+j,region+0x10*j)
  c.setg(10,target)
  class Observe(type(c)):
   def fetch(s,pc):
    if not compiled and pc in labels:coverage[labels[pc]]=coverage.get(labels[pc],0)+1
    return super().fetch(pc)
  c.__class__=Observe
  try:c.run(bundle.entry if compiled else 0x82BB56C8);outputs.append((c.g(3)&0xffffffff,c.mem.rd(region,len(image))))
  except Exception as e:errors.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  rec={'case':i,'kind':k,'return':outputs[0][0],'original_sha256':hashlib.sha256(outputs[0][1]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1][1]).hexdigest()};records.append(rec)
  if outputs[0]!=outputs[1]:bad.append(dict(rec,inputs=[list(map(bits,v))for v in rows],original=outputs[0][1].hex(),compiled=outputs[1][1].hex()))
report={'compared':len(records),'mismatches':bad,'exceptions':errors,'coverage':coverage,'cases':records,'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
p=OUT/'rim-rim-v2-diagnostic.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('compared',len(records),'bad',len(bad),'exceptions',len(errors),'coverage',coverage);print('first',bad[:1],errors[:1])
