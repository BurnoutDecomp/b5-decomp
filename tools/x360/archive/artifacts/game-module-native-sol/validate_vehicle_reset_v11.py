from pathlib import Path
import ast,hashlib,json,random,struct,sys
P=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(P.read_text(encoding='utf-8')).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
OUT=Path(__file__).resolve().parent

vr=json.loads((OUT/'prefix-deps-v8/compile-results.json').read_text())['results'];sr=json.loads((OUT/'replay-deps-v5/compile-results.json').read_text())['results']
vrow=next(r for r in vr if Path(r['source']).name=='BrnDirectorVehicleTracker.cpp');srow=next(r for r in sr if Path(r['source']).name=='BrnGameStateSharedIO.cpp')
objects=[PackedCoff(Path(vrow['object']),'?Construct@VehicleTracker@BrnDirector@@'),PackedCoff(Path(srow['object']),'?ClearData@CarScoreData@')]
bundle=PackedBundle(objects);bundle.select('?Construct@VehicleTracker@BrnDirector@@')
rng=random.Random(0x821f77a8);records=[];bad=[];exceptions=[]
for i in range(32):
 self,base,size=0x57000100,0x57000000,0x500;initial=rng.randbytes(size);outputs=[];traces=[]
 for compiled in [False,True]:
  c=cpu(bundle);c.mem.add(base,size,initial);c.setg(3,self);trace=[]
  target=F.address(bundle,'?ClearData@CarScoreData@')if compiled else 0x821f2b28
  class Observe(type(c)):
   def fetch(s,pc):
    if pc==target:trace.append({'callee':'CarScoreData::ClearData','receiver':hex(s.g(3)&0xffffffff)})
    return super().fetch(pc)
  c.__class__=Observe
  try:c.run(bundle.entry if compiled else 0x821f77a8);outputs.append(c.mem.rd(base,size));traces.append(trace)
  except Exception as e:exceptions.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  rec={'case':i,'original_sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1]).hexdigest(),'calls':traces};records.append(rec)
  if outputs[0]!=outputs[1]or traces[0]!=traces[1]:bad.append(dict(rec,differences=[hex(n-0x100)for n,(a,b)in enumerate(zip(*outputs))if a!=b]))
p=OUT/'vehicle-reset-v11-comparison.json';assert not p.exists();p.write_text(json.dumps({'compared':len(records),'mismatches':bad,'exceptions':exceptions,'cases':records,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual complete VehicleTracker runtime Construct calls real canonical CarScoreData::ClearData. All physical object and guard bytes compared, no receiver rebinding, no engine hook substitution. No VehicleTracker::Update, journal sampling, whole Director/GameModule or boot claim.'},indent=2)+'\n');print('compared',len(records),'bad',len(bad),'exceptions',exceptions[:2]);print('first',bad[:1])
