from pathlib import Path
import ast,hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
P=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scripts/validate_quartic_native_v2_model.py')
nodes=[]
for n in ast.parse(P.read_text()).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='out'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
OUT=Path(__file__).resolve().parent;D=OUT.parent/'volume-baseline';obj=D/'LineSeg-current-v8.obj';bundle=F.Bundle([F.Coff(obj,'?SolveQuarticRoots@')]);bundle.select('?SolveQuarticRoots@')
rng=random.Random(0x82baca30)
for i in range(2):
 coefficients=[rng.uniform(-3,3),rng.uniform(.5,2),rng.uniform(-1,1),rng.uniform(-.3,.3),rng.uniform(.01,.2)];coef=struct.pack('>5f',*coefficients)+rng.randbytes(44);initial=rng.randbytes(64)
class StopTrace(Exception):pass
records={}
for compiled in [False,True]:
 c=F.cpu(bundle);c.__class__=CPU;c.site_seen=[0]*len(cylmodel.SITES);c.rest=set();c.max_steps=1000000;c.mem.add(0x50000000,64,coef);c.mem.add(0x50001000,64,initial)
 for addr,name in bundle.unresolved.items():
  if name.startswith('__save'):c.hooks[addr]=lambda cpu:None
  elif name.startswith('__rest'):c.rest.add(addr);c.hooks[addr]=lambda cpu:None
 c.setg(3,0x50000000);c.setg(4,0x50001000);trace=[];iteration=0
 class Observe(type(c)):
  def fetch(s,pc):
   global iteration
   w=super().fetch(pc);op=w>>26
   event=None;vd=vb=None
   if op==4 and w&0x7ff in [458,394]:
    event='log'if w&0x7ff==458 else'exp';vd=(w>>21)&31;vb=(w>>11)&31
   elif op==6 and w&0x7f0 in [0x6f0,0x6b0]:
    event='log'if w&0x7f0==0x6f0 else'exp';vd=((w>>21)&31)|(((w>>2)&3)<<5);vb=((w>>11)&31)|((w&3)<<5)
   if event:trace.append({'event':event,'pc':hex(pc),'word':hex(w),'vd':vd,'vb':vb,'input':list(s.vr[vb]),'input_f32':[float(x)for x in s.vf(vb)],'iteration':iteration})
   if op==59 and ((w>>1)&31)==18:
    fd,fa,fb=(w>>21)&31,(w>>16)&31,(w>>11)&31
    trace.append({'event':'divide','pc':hex(pc),'word':hex(w),'fd':fd,'fa':fa,'fb':fb,'numerator':s.f(fa),'denominator':s.f(fb),'root':s.f(29)if compiled else s.f(0),'iteration':iteration})
    iteration+=1
    if iteration==3:raise StopTrace()
   if not compiled and pc in [0x82BAD344,0x82BAD378,0x82BAD380]:
    trace.append({'event':'derivative','pc':hex(pc),'word':hex(w),'vectors':{str(k):list(s.vr[k])for k in [0,5,10,11,24,27,28,31,56,57,63]},'f32':{str(k):list(map(float,s.vf(k)))for k in [0,5,10,11,24,27,28,31,56,57,63]},'iteration':iteration})
   return w
 c.__class__=Observe
 try:c.run(bundle.entry if compiled else 0x82baca30)
 except StopTrace:pass
 records['compiled'if compiled else'original']=trace
p=OUT/'case1-logexp-trace.json';assert not p.exists();p.write_text(json.dumps({'coef':coef[:20].hex(),'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'model_source':str(P),'model_source_sha256':hashlib.sha256(P.read_bytes()).hexdigest(),'records':records,'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest()},indent=2)+'\n')
for name,trace in records.items():
 print(name)
 for e in trace:
  if e['event']in ['log','exp']:print(e['iteration'],e['event'],e['pc'],e['input_f32'][0])
  elif e['event']=='divide':print(e)
  elif e['pc']=='0x82bad344':print('v56scale',e['f32']['56'][0],'v5poly',e['f32']['5'][0])
