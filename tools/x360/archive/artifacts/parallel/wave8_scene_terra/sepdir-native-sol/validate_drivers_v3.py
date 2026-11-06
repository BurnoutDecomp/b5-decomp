from pathlib import Path
import ast,hashlib,json,math,random,struct,inspect,importlib
p=Path(__file__).with_name('validate_rim_edge_v2.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='rng'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent;provider=OUT.parent/'volume-baseline'
row=json.loads((OUT/'candidate-v3/compile-results.json').read_text())['results'][0]
objects=[PackedCoff(Path(row['object']),'?FindBestSepDirWithCylinder@')]
for name in ['GPSphere','GPCapsule','GPTriangle','GPBox','GPCylinder']:
 path=provider/(name+('-nearest-v1.obj'if name=='GPTriangle'else'-table-wip.obj'))
 objects.append(PackedCoff(path,'?GetIntervals@'+name+'@'))
objects.append(PackedCoff(provider/'GPRegistration-table-wip.obj','?g_aGPVolumeMethods@'))
code=inspect.getsource(F.Bundle).replace('used, pending = set(), [(oi,sec)]', '''used, pending = set(), [(oi,sec)]
        for root,(other,_,section) in self.definitions.items():
            if root.startswith('?g_aGPVolumeMethods@'):
                pending.append((other,section))''')
code=code.replace('bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}', '''bases = {}
        cursor = 0x83800000
        for key in sorted(used):
            bases[key] = cursor
            cursor += max(16, (self.objects[key[0]].sections[key[1]][3] + 15) & ~15)''')
ns=dict(vars(importlib.import_module(F.Bundle.__module__)));exec(compile(code,'private-multi-root-SAT-bundle','exec'),ns)
bundle=ns['Bundle'](objects)
original_methods={1:0x82BA80C0,2:0x82BAFC10,3:0x82BBA6E0,4:0x82BA9238,5:0x82BAD938}
reports={};rng=random.Random(0x82BB76E8)
for name,original,cases in [('FindBestSeparatingDirection',0x82BB4660,64),('FindBestSepDirWithCylinder',0x82BB76E8,64),('FindBestSeparatingDirVolCyl',0x82BAA258,32)]:
 bundle.select('?'+name+'@');table=F.address(bundle,'?g_aGPVolumeMethods@');calls={F.address(bundle,'?GetIntervals@'+gp+'@'):ty for gp,ty in [('GPSphere',1),('GPCapsule',2),('GPTriangle',3),('GPBox',4),('GPCylinder',5)]}
 records=[];bad=[];errors=[]
 for i in range(cases):
  region=0x57000000;image=bytearray(rng.randbytes(0x600));target=region+0x400
  if name=='FindBestSeparatingDirection':types=[1+i%5,1+(i//5)%5]
  else:types=[5,1+i%5]if name=='FindBestSepDirWithCylinder'else[1+i%5,5]
  for j,ty in zip([0,0x100],types):
   angle=f32(rng.uniform(-3,3));co=f32(math.cos(angle));si=f32(math.sin(angle));frame=[[co,si,0,.2],[-si,co,0,-.3],[0,0,1,.4]]
   if ty==5:frame=[frame[2],frame[0],frame[1]]
   if i%7==0:frame=[[1,0,0,.2],[0,1,0,-.3],[0,0,1,.4]]
   pos=[f32(rng.uniform(-5,5))for _ in range(4)];dims=[f32(rng.uniform(.2,4))for _ in range(4)]
   if i%11==0:pos=[0,0,0,0]
   normals,edges={1:(0,0),2:(0,1),3:(1,3),4:(3,3),5:(0,1)}[ty]
   for off,v in [(0,pos),(0x70,dims)]+[(0x10+16*k,frame[k])for k in range(3)]+[(0x40+16*k,frame[k])for k in range(3)]:struct.pack_into('>4f',image,j+off,*v)
   image[j+0x8c]=normals;image[j+0x8d]=edges;struct.pack_into('>I',image,j+0x90,ty)
  outputs=[];traces=[]
  for compiled in [False,True]:
   c=basecpu();initial=bytearray(image)
   for j,ty in zip([0,0x100],types):
    raw=c.mem.rd(table+ty*16,16)if compiled else F.M.V.x360rd.rd(0x82F918F0+ty*16,16)
    initial[j+0xa4:j+0xb4]=raw
   c.mem.add(region,len(initial),initial);c.setg(3,target);c.setg(4,region);c.setg(5,region+0x100);trace=[]
   class Observe(type(c)):
    def fetch(s,pc):
     calltype=calls.get(pc)if compiled else next((t for t,a in original_methods.items()if a==pc),None)
     if calltype:
      receiver=s.g(3)&0xffffffff;dirs=s.g(4)&0xffffffff;count=s.g(5)&0xffffffff
      trace.append({'type':calltype,'this':receiver,'count':count,'directions':s.mem.rd(dirs,16*count).hex()})
     return super().fetch(pc)
   c.__class__=Observe
   try:
    c.run(bundle.entry if compiled else original)
    final=c.mem.rd(region,len(initial));assert final[:0x400]==initial[:0x400]and final[0x410:]==initial[0x410:]
    outputs.append((list(c.vr[1]),final[0x400:0x410]));traces.append(trace)
   except Exception as e:errors.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
  if len(outputs)==2:
   rec={'case':i,'types':types,'original':outputs[0][0],'compiled':outputs[1][0],'original_direction':outputs[0][1].hex(),'compiled_direction':outputs[1][1].hex(),'member_dispatches':len(traces[0])};records.append(rec)
   if outputs[0]!=outputs[1]or traces[0]!=traces[1]:bad.append(dict(rec,initial=image.hex(),original_trace=traces[0],compiled_trace=traces[1]))
 reports[name]={'compared':len(records),'mismatches':bad,'exceptions':errors,'cases':records}
 print(name,'compared',len(records),'bad',len(bad),'exceptions',len(errors),flush=True);print('first',bad[:1],errors[:1],flush=True)
p=OUT/'drivers-v3-composed-diagnostic.json';assert not p.exists();p.write_text(json.dumps({'functions':reports,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'unresolved_symbols':bundle.unresolved,'scope':'Whole actual SAT drivers, actual original/compiled interval methods and real GPRegistration PMF table. True retail result v1 and out r3, physical GP receivers. Only image-specific genuine method-code relocations differ; no callback substitution. Dispatch candidates/counts/receivers and all output/input/guard bytes compared. No hardware estimate or full collision/program claim.'},indent=2)+'\n')
