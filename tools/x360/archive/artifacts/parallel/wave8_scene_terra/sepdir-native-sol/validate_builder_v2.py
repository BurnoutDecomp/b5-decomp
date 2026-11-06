from pathlib import Path
import ast,hashlib,json,math,random,struct
p=Path(__file__).with_name('validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent;bundle.select('?FindBestSepDirWithCylinder_FindCandidates@')
rng=random.Random(0x82BB64C0);records=[];bad=[];errors=[];counts={}
for i in range(72):
 region=0x57000000;image=bytearray(rng.randbytes(0x800));target=region+0x300
 kind=[1,2,3,4,5,0][i%6];phase=i//6
 for j,ty in [(0,5),(0x100,kind)]:
  xyz=[f32(rng.uniform(-5,5))for _ in range(4)]
  angle=f32(rng.uniform(-3,3));co=f32(math.cos(angle));si=f32(math.sin(angle))
  frame=[[co,si,0,f32(rng.uniform(-1,1))],[-si,co,0,f32(rng.uniform(-1,1))],[0,0,1,f32(rng.uniform(-1,1))]]
  if j==0:frame=[frame[2],frame[0],frame[1]]
  dimensions=[f32(rng.uniform(.3,4))for _ in range(4)]
  normals,edges={1:(0,0),2:(0,1),3:(1,3),4:(3,3),5:(0,1),0:(0,0)}[ty]
  if phase%5==0:frame=[[1,0,0,.2],[0,1,0,-.3],[0,0,1,.5]]
  for off,v in [(0,xyz),(0x70,dimensions)]+[(0x10+16*k,frame[k])for k in range(3)]+[(0x40+16*k,frame[k])for k in range(3)]:struct.pack_into('>4f',image,j+off,*v)
  image[j+0x8c]=normals;image[j+0x8d]=edges;struct.pack_into('>I',image,j+0x90,ty)
 outputs=[]
 for compiled in [False,True]:
  c=basecpu();c.mem.add(region,len(image),image);c.setg(3,target);c.setg(4,region);c.setg(5,region+0x100)
  try:c.run(bundle.entry if compiled else 0x82BB64C0);outputs.append((c.g(3)&0xffffffff,c.mem.rd(region,len(image))))
  except Exception as e:errors.append({'case':i,'kind':kind,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  count=outputs[0][0];counts[count]=counts.get(count,0)+1
  rec={'case':i,'kind':kind,'count':count,'original_sha256':hashlib.sha256(outputs[0][1]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1][1]).hexdigest()};records.append(rec)
  if outputs[0]!=outputs[1]:bad.append(dict(rec,original_return=outputs[0][0],compiled_return=outputs[1][0],initial=image.hex(),original=outputs[0][1].hex(),compiled=outputs[1][1].hex(),difference=[hex(j)for j,(a,b)in enumerate(zip(outputs[0][1],outputs[1][1]))if a!=b]))
p=OUT/'cylinder-builder-v2-diagnostic.json';assert not p.exists();p.write_text(json.dumps({'compared':len(records),'mismatches':bad,'exceptions':errors,'counts':counts,'cases':records,'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n');print('compared',len(records),'bad',len(bad),'exceptions',len(errors),'counts',counts);print('first',[(x['case'],x['kind'],x['difference'][:20])for x in bad[:6]],errors[:2])
