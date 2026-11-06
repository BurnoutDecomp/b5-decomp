from pathlib import Path
import ast,hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
P=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scripts/validate_quartic_native_v2_model.py');nodes=[]
for n in ast.parse(P.read_text()).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='out'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
class CorrectLoadCPU(CPU):
 def exec_vector(self,pc,w,mn):
  if w>>26==4 and(w&0x7f3)in[0x403,0x443]:
   self.corrected_loads+=1
   return cylmodel.Cpu.exec_vector(self,pc,w,mn)
  return super().exec_vector(pc,w,mn)
OUT=Path(__file__).resolve().parent;D=OUT.parent/'volume-baseline';obj=D/'LineSeg-current-v8.obj';b=F.Bundle([F.Coff(obj,'?SolveQuarticRoots@')]);b.select('?SolveQuarticRoots@')
rng=random.Random(0x82baca30);bad=[];errors=[];cases=[];counts={'accepted':0,'rejected':0};loads={'original':0,'compiled':0}
for i in range(64):
 coefficients=[rng.uniform(-3,3),rng.uniform(.5,2),rng.uniform(-1,1),rng.uniform(-.3,.3),rng.uniform(.01,.2)];coef=struct.pack('>5f',*coefficients)+rng.randbytes(44);initial=rng.randbytes(64);results=[]
 for compiled in(False,True):
  c=F.cpu(b);c.__class__=CorrectLoadCPU;c.corrected_loads=0;c.site_seen=[0]*len(cylmodel.SITES);c.rest=set();c.max_steps=1000000;c.mem.add(0x50000000,64,coef);c.mem.add(0x50001000,64,initial)
  for addr,name in b.unresolved.items():
   if name.startswith('__save'):c.hooks[addr]=lambda cpu:None
   elif name.startswith('__rest'):c.rest.add(addr);c.hooks[addr]=lambda cpu:None
  c.setg(3,0x50000000);c.setg(4,0x50001000)
  try:c.run(b.entry if compiled else 0x82baca30)
  except Exception as ex:errors.append({'case':i,'compiled':compiled,'error':repr(ex)});break
  results.append((c.g(3)&0xffffffff,c.mem.rd(0x50001000,64),c.mem.rd(0x50000000,64)));loads['compiled'if compiled else'original']+=c.corrected_loads
 if len(results)<2:continue
 counts['accepted'if results[0][0]else'rejected']+=1
 rec={'case':i,'coef':coef[:20].hex(),'original_return':results[0][0],'compiled_return':results[1][0],'original_root':results[0][1][:4].hex(),'compiled_root':results[1][1][:4].hex()};cases.append(rec)
 if results[0]!=results[1]:bad.append(rec)
report={'counts':counts,'compared':len(cases),'mismatches':bad,'exceptions':errors,'cases':cases,'corrected_loads':loads,'original_negative_report':str(D/'quartic-native-v2-model-comparison.json'),'original_negative_report_sha256':hashlib.sha256((D/'quartic-native-v2-model-comparison.json').read_bytes()).hexdigest(),'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'model_change':'Only op4 lvlx128/lvrx128 dispatch is prioritized ahead of shadowing legacy handler; use existing cylmodel decoder whose destination high bits come from (word>>2)&3. All product source/object unchanged, exact same original64 cases. Software log/exp/estimate and controlled compiler runtime-save model remain; no hardware claim.'}
p=OUT/'quartic-correct-load-same64.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('compared',len(cases),'counts',counts,'bad',len(bad),'errors',len(errors),'loads',loads);print('first',bad[:3],errors[:2])
