from pathlib import Path
import ast,hashlib,json,math,random,struct
p=Path(__file__).with_name('validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent
reports={}
for name,original,ncases in [('FindBestSepDir_FindCandidates',0x82BB4038,96),('FindBestSeparatingDirectionBoxBox',0x82BB47F0,64),('FindBestSeparatingDirectionTriBox',0x82BB4CA8,64),('FindBestSeparatingDirectionSphSph',0x82BAA1F0,64)]:
 bundle.select('?'+name+'@');rng=random.Random(original);records=[];bad=[];errors=[]
 for i in range(ncases):
  region=0x57000000;image=bytearray(rng.randbytes(0x800));target=region+0x300
  for j in [0,0x100]:
   v=[[f32(rng.uniform(-3,3))for _ in range(4)]for _ in range(8)]
   if i%8==0:v[4:7]=[[1,0,0,.5],[0,1,0,-.5],[0,0,1,.2]]
   if i%8==1:v[4:7]=[[1,0,0,.3]]*3
   if i%8==2:v[4:7]=[[0,0,0,.3]]*3
   if i%8==3:v[0]=[0,0,0,0]
   if i>=ncases-8:v[0][i%4]=frombits([0x7fc12345,0x7f800000,0xff800000,1][i%4])
   for k,rowv in enumerate(v):struct.pack_into('>4f',image,j+16*k,*rowv)
   image[j+0x8c]=(i+(1 if j else 0))%4;image[j+0x8d]=(i//4+(1 if j else 0))%4
  outputs=[]
  for compiled in [False,True]:
   c=basecpu();c.mem.add(region,len(image),image);c.setg(3,target);c.setg(4,region);c.setg(5,region+0x100)
   try:
    c.run(bundle.entry if compiled else original)
    ret=(c.g(3)&0xffffffff)if name=='FindBestSepDir_FindCandidates'else list(c.vr[1])
    outputs.append((ret,c.mem.rd(region,len(image))))
   except Exception as e:errors.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
  if len(outputs)==2:
   rec={'case':i,'original_return':outputs[0][0],'compiled_return':outputs[1][0],'original_sha256':hashlib.sha256(outputs[0][1]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1][1]).hexdigest()};records.append(rec)
   if outputs[0]!=outputs[1]:bad.append(dict(rec,initial=image.hex(),original=outputs[0][1].hex(),compiled=outputs[1][1].hex(),difference=[hex(j)for j,(a,b)in enumerate(zip(outputs[0][1],outputs[1][1]))if a!=b]))
  
 reports[name]={'compared':len(records),'mismatches':bad,'exceptions':errors,'cases':records}
 print(name,'compared',len(records),'bad',len(bad),'exceptions',len(errors));print('first',[(x['case'],x['difference'][:10],x['original_return'],x['compiled_return'])for x in bad[:3]],errors[:1])
p=OUT/'kernel-v2-diagnostic.json';assert not p.exists();p.write_text(json.dumps({'functions':reports,'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
