from pathlib import Path
import ast,hashlib,json,random,struct,sys
P=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(P.read_text(encoding='utf-8')).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
OUT=Path(__file__).resolve().parent


row=json.loads((OUT/'score-copy-v22/compile-results.json').read_text())['results'][0]
objects=[PackedCoff(Path(row['object']),'??4CarScoreData@')];bundle=PackedBundle(objects);bundle.select('??4CarScoreData@')
original_asm=json.loads(Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x821F4740.json').read_text())['assembly']
import re
float_offsets=sorted({int(m,0)for m in re.findall(r'lfs\s+f\d+,\s*(0x[0-9A-Fa-f]+|\d+)\(r4\)',original_asm)})
rng=random.Random(0x821f4740);records=[];bad=[];exceptions=[]
for i in range(40):
 base,size,dest,source=0x57000000,0x800,0x57000100,0x57000400;source=dest if i>=32 else source;source_offset=source-base;initial=bytearray(rng.randbytes(size))
 for off in float_offsets:initial[source_offset+off:source_offset+4+off]=struct.pack('>f',rng.uniform(-10000,10000))
 for off in [0x44,0x45,0x68,0x69,0x6b,0xbc,0xd9]:initial[source_offset+off]=rng.randrange(2)
 outputs=[];returns=[]
 for compiled in [False,True]:
  c=cpu(bundle);c.mem.add(base,size,bytes(initial));c.setg(3,dest);c.setg(4,source)
  def memcpy(s):
   d,a,n=[s.g(k)&0xffffffff for k in [3,4,5]];s.mem.wr(d,s.mem.rd(a,n));s.setg(3,d)
  for addr,name in bundle.unresolved.items():
   if name=='memcpy':c.hooks[addr]=memcpy
  try:c.run(bundle.entry if compiled else 0x821f4740);outputs.append(c.mem.rd(base,size));returns.append(c.g(3)&0xffffffff)
  except Exception as e:exceptions.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  rec={'case':i,'self_assignment':i>=32,'original_sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1]).hexdigest(),'returned_dest':[hex(v)for v in returns]};records.append(rec)
  if outputs[0]!=outputs[1]or returns!=[dest,dest]:bad.append(dict(rec,differences=[hex(n-0x100)for n,(a,b)in enumerate(zip(*outputs))if a!=b]))
p=OUT/'score-assignment-v23-comparison.json';assert not p.exists();p.write_text(json.dumps({'compared':len(records),'mismatches':bad,'exceptions':exceptions,'cases':records,'float_source_offsets':list(map(hex,float_offsets)),'object':str(objects[0].path),'object_sha256':hashlib.sha256(objects[0].path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual selective provider. First32 cases preserve the prior negative inputs; eight additional cases use identical source/destination objects. Finite source floats, canonical source bool bytes, separately poisoned source/destination/guards. Original and actual compiled r3/r4 physically identical; returned r3 checked. memcpy is a controlled standard byte service. No header/type/layout change; no nonfinite/hardware/global source claim.'},indent=2)+'\n');print('compared',len(records),'bad',len(bad),'exceptions',exceptions[:2]);print('first',bad[:1]);print('float offsets',float_offsets)
