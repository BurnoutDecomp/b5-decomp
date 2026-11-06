from pathlib import Path
import ast,json,struct,inspect
p=Path(__file__).with_name('trace_case1.py');nodes=[]
for n in ast.parse(p.read_text(encoding='utf-8-sig')).body:
 if isinstance(n,ast.ClassDef)and n.name=='StopTrace':break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(p),'exec'),globals())
OUT=Path(__file__).resolve().parent
c=F.cpu(bundle);c.__class__=CPU;c.site_seen=[0]*len(cylmodel.SITES);c.rest=set();c.max_steps=1000000;c.mem.add(0x50000000,64,coef);c.mem.add(0x50001000,64,initial);c.setg(3,0x50000000);c.setg(4,0x50001000)
class StopTrace(Exception):pass
iteration=0;trace=[]
class Observe(type(c)):
 def fetch(s,pc):
  global iteration
  w=super().fetch(pc)
  if pc==0x82bad3a0:
   iteration+=1
   if iteration==2:raise StopTrace()
  if iteration==1 and pc in [0x82bacf40,0x82bacfe0,0x82bacfe4,0x82bad010,0x82bad014]:
   sp=s.g(1)&0xffffffff;ea=s.g(9)&0xffffffff
   trace.append({'pc':hex(pc),'word':hex(w),'sp':hex(sp),'r9':hex(ea),'f12':s.f(12),'at_r9':s.mem.rd(ea,16).hex(),'at_var170':s.mem.rd(sp-0x170,16).hex(),'v57':list(s.vr[57]),'v89':list(s.vr[89])})
  return w
 def exec_vector(s,pc,w,mn):
  ret=super().exec_vector(pc,w,mn)
  if iteration==1 and pc==0x82bacfe0:
   trace.append({'event':'after actual dispatch','ret':ret,'v57':list(s.vr[57]),'v89':list(s.vr[89])})
  return ret
c.__class__=Observe
try:c.run(0x82baca30)
except StopTrace:pass
for row in trace:print(row)
print('MRO',[(x.__module__,x.__name__,'exec_vector'in x.__dict__)for x in type(c).__mro__]);print('mnemonic',F.M.V.x360rd.u32(0x82bacfe0)&0x7f3)
p=OUT/'case1-lvlx-dispatch-trace.json';assert not p.exists();p.write_text(json.dumps({'trace':trace,'mro':[(x.__module__,x.__name__)for x in type(c).__mro__]},indent=2)+'\n')
