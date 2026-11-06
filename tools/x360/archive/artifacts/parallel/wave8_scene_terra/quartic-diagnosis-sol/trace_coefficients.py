from pathlib import Path
import ast,json
p=Path(__file__).with_name('trace_case1.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.ClassDef)and n.name=='StopTrace':break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent
c=F.cpu(bundle);c.__class__=CPU;c.site_seen=[0]*len(cylmodel.SITES);c.rest=set();c.max_steps=1000000;c.mem.add(0x50000000,64,coef);c.mem.add(0x50001000,64,initial);c.setg(3,0x50000000);c.setg(4,0x50001000)
class StopTrace(Exception):pass
iteration=0;trace=[];allregs=[]
class Observe(type(c)):
 def fetch(s,pc):
  global iteration
  w=super().fetch(pc)
  if pc==0x82bad3a0:
   iteration+=1
   if iteration==2:raise StopTrace()
  if iteration==1:
   if pc in [0x82bacb8c,0x82bad0d8,0x82bacbcc,0x82bad108,0x82bacbd4,0x82bad118]:
    trace.append({'pc':hex(pc),'vectors':{str(k):list(s.vr[k])for k in range(64)},'f32':{str(k):list(map(float,s.vf(k)))for k in range(64)}})
   if 0x82bacb00<=pc<=0x82bad120:allregs.append({'pc':hex(pc),'word':hex(w),'vectors':list(map(list,s.vr))})
  return w
c.__class__=Observe
try:c.run(0x82baca30)
except StopTrace:pass
p=OUT/'case1-coefficient-trace.json';assert not p.exists();p.write_text(json.dumps({'points':trace,'steps':allregs},indent=2)+'\n')
for t in trace:
 print(t['pc'],{k:t['f32'][str(k)][0]for k in [1,2,3,4,8,9,11,12,15,16,18,28,29,30,31,56,57,58,59,60,61]})
