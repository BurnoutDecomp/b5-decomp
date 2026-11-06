from pathlib import Path
import ast,hashlib,json,math,random,struct
p=Path(__file__).with_name('validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent;reports={}
for name,original in [('AddAxisToEdgeCandidate',0x82BB50E8),('AddRimToEdgeCandidate',0x82BB6118),('AddRimToRimCandidates',0x82BB61C0)]:
 bundle.select('?'+name+'@');rng=random.Random(original);records=[];bad=[];errors=[]
 for i in range(64):
  region=0x57000000;image=bytearray(rng.randbytes(0x800));count=i%3;struct.pack_into('>I',image,0x700,count);target=region+0x300;countptr=region+0x700
  axis=[0,0,1,f32(rng.uniform(-1,1))];edge=[1,0,0,f32(rng.uniform(-1,1))]
  centre=[f32(rng.uniform(-6,6))for _ in range(4)];point=[f32(rng.uniform(-6,6))for _ in range(4)]
  if i%8==0:edge=axis[:]
  if i%8==1:edge=[0,0,0,0]
  if i%8==2:point=centre[:]
  if i%8==3:centre=[0,0,0,1];point=[0,1,0,2]
  offset=f32(rng.uniform(-2,2));half=f32(rng.uniform(.1,2));radius=[f32(rng.uniform(.1,3))]*4
  if i>=56:radius=[.25,.5,1,2];point[i%4]=frombits([0x7fc12345,0x7f800000,1,0x80000000][i%4])
  if name=='AddRimToRimCandidates':
   for base,pos,ax in [(0,centre,axis),(0x100,point,[0,1,0,.2] if i%4 else axis)]:
    for off,v in [(0,pos),(0x40,ax),(0x70,[half,radius[0],.3,1.])]:struct.pack_into('>4f',image,base+off,*v)
  else:
   rows=[centre,axis,point,edge]if name=='AddAxisToEdgeCandidate'else[centre,axis,radius,point,edge]
   for j,v in enumerate(rows):struct.pack_into('>4f',image,j*16,*v)
  outputs=[]
  for compiled in [False,True]:
   c=basecpu();c.mem.add(region,len(image),image)
   if name=='AddRimToRimCandidates':
    for r,v in zip([3,4,5,6],[region,region+0x100,target,countptr]):c.setg(r,v)
   elif name=='AddAxisToEdgeCandidate':
    for j in range(4):c.setg(3+j,region+j*16)
    c.setg(9,target);c.setg(10,countptr);c.setf(1,offset);c.setf(2,half)
   else:
    for j in range(5):c.setg(3+j,region+j*16)
    c.setg(10,target);c.setf(1,offset);c.setf(2,half);c.mem.wr((c.g(1)&0xffffffff)+0x54,struct.pack('>I',countptr))
   try:c.run(bundle.entry if compiled else original);outputs.append(c.mem.rd(region,len(image)))
   except Exception as e:errors.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
  if len(outputs)==2:
   rec={'case':i,'kind':i%8,'count':int.from_bytes(outputs[0][0x700:0x704],'big'),'original_sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1]).hexdigest()};records.append(rec)
   if outputs[0]!=outputs[1]:bad.append(dict(rec,initial=image.hex(),original=outputs[0].hex(),compiled=outputs[1].hex(),difference=[hex(j)for j,(a,b)in enumerate(zip(*outputs))if a!=b],offset=offset,half=half))
  
 report={'compared':len(records),'mismatches':bad,'exceptions':errors,'cases':records};reports[name]=report
 print(name,'compared',len(records),'bad',len(bad),'exceptions',len(errors));print('first',[(x['case'],x['difference'][:12])for x in bad[:2]],errors[:1])
p=OUT/'appenders-v2-diagnostic.json';assert not p.exists();p.write_text(json.dumps({'functions':reports,'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
