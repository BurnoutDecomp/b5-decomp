from pathlib import Path
import ast,hashlib,json,random,struct,sys
P=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(P.read_text(encoding='utf-8')).body:
 if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
 nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),globals())
OUT=Path(__file__).resolve().parent
report=json.loads((OUT/'replay-deps-v5/compile-results.json').read_text());rows={Path(r['source']).name:r for r in report['results']if r['passed']}
new_replay=json.loads((OUT/'prefix-deps-v8/compile-results.json').read_text())['results'][1];rows['BrnReplayDirector.cpp']=new_replay
extra=json.loads((OUT/'icetake-provider-v6/compile-results.json').read_text())['results'][0];rows['ICEDataICETake.cpp']=extra
rows['BrnGameStateSharedIO.cpp']=json.loads((OUT/'score-selective-v13/compile-results.json').read_text())['results'][0]
objects=[PackedCoff(Path(rows['BrnReplayDirector.cpp']['object']),'??0ReplayDirector@'),PackedCoff(Path(rows['ICEDataICETake.cpp']['object']),'??0ICETake@'),PackedCoff(Path(rows['BrnGameStateSharedIO.cpp']['object']),'??0CarScoreData@')]
bundle=PackedBundle(objects);bundle.select('??0ReplayDirector@')
vtables=[(0x690,0x8200a560,'??_7BehaviourGameplayExternal@'),(0x6e0,0x8200a150,'??_7CollisionPolicyAttachedToVehicle@'),(0x1200,0x82008d38,'??_7BehaviourGameplayBumper@')]
print('unresolved',bundle.unresolved,flush=True)
print('native-vtables',[(hex(o),hex(a),n,hex(F.address(bundle,n)))for o,a,n in vtables],flush=True)
rng=random.Random(0x827e2020);records=[];bad=[];exceptions=[]
compiled_calls={F.address(bundle,'??0ICETake@'):'ICETake',F.address(bundle,'??0CarScoreData@'):'CarScoreData'}
original_calls={0x822145e8:'ICETake',0x822a45a8:'CarScoreData'}
for i in range(16):
 self,base,size=0x57000100,0x57000000,0x2c00;initial=rng.randbytes(size);outputs=[];traces=[]
 for compiled in [False,True]:
  c=cpu(bundle);c.mem.add(base,size,initial);c.mem.wr(F.M.S.STACK,bytes([0x31+7*i])*0x20000);c.setg(3,self);trace=[]
  def memset(s):
   d,v,n=[s.g(k)&0xffffffff for k in [3,4,5]];s.mem.wr(d,bytes([v&255])*n);s.setg(3,d)
  for addr,name in bundle.unresolved.items():
   if name=='memset':c.hooks[addr]=memset
   elif name=='?MemClear@stdc@core@rw@@YAXPAXI@Z':
    def memclear(s):
     d,n=[s.g(k)&0xffffffff for k in [3,4]];s.mem.wr(d,bytes(n))
    c.hooks[addr]=memclear
  class Observe(type(c)):
   def fetch(s,pc):
    label=(compiled_calls if compiled else original_calls).get(pc)
    if label:trace.append({'callee':label,'receiver':hex(s.g(3)&0xffffffff)})
    return super().fetch(pc)
   def exec_vector(s,pc,w,mn):
    if w>>26==4 and(w&0x7f3)in[0x403,0x443]:
     vd=((w>>21)&31)|(((w>>2)&3)<<5);ra,rb=(w>>16)&31,(w>>11)&31;ea=((s.g(ra)if ra else 0)+s.g(rb))&0xffffffff
     if(w&0x7f3)==0x403:n=16-(ea&15);raw=s.mem.rd(ea,n)+bytes(16-n)
     else:n=ea&15;raw=bytes(16-n)+(s.mem.rd(ea-n,n)if n else b'')
     s.vr[vd]=struct.unpack('>4I',raw);return True
    return super().exec_vector(pc,w,mn)
  c.__class__=Observe
  try:
   c.run(bundle.entry if compiled else 0x827e2020);result=bytearray(c.mem.rd(base,size));ptrs=[]
   for off,native,symbol in vtables:
    actual=int.from_bytes(result[0x100+off:0x104+off],'big');expected=F.address(bundle,symbol)if compiled else native
    assert actual==expected,(i,compiled,hex(off),hex(actual),hex(expected));ptrs.append({'offset':hex(off),'symbol':symbol,'actual':hex(actual)})
    # Exclude only the three independently checked image-specific vtable addresses;
    # all receivers, other object bytes and guards remain physically identical.
    result[0x100+off:0x104+off]=b'VTBL'
   outputs.append(bytes(result));traces.append(trace)
  except Exception as e:exceptions.append({'case':i,'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))});break
 if len(outputs)==2:
  rec={'case':i,'original_sha256':hashlib.sha256(outputs[0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1]).hexdigest(),'calls':traces};records.append(rec)
  if outputs[0]!=outputs[1]or traces[0]!=traces[1]:bad.append(dict(rec,differences=[hex(n-0x100)for n,(a,b)in enumerate(zip(*outputs))if a!=b],initial=initial.hex(),original=outputs[0].hex(),compiled=outputs[1].hex()))
p=OUT/'replay-ctor-v14-comparison.json';assert not p.exists();p.write_text(json.dumps({'compared':len(records),'mismatches':bad,'exceptions':exceptions,'cases':records,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual complete compiled ReplayDirector ctor links actual newly compiled ICETake and canonical CarScoreData constructors. Physical object/child receivers/call order and all object/guard bytes match, except three image-specific vtable pointer addresses individually checked by exact type-symbol identity. No engine-body callback substitution. Standard memset and rw::core::stdc::MemClear are controlled byte services if called. No field offset normalization; no whole Director/GameModule or replay update/boot claim.'},indent=2)+'\n');print('compared',len(records),'bad',len(bad),'exceptions',len(exceptions));print('first',[(x['case'],x['differences'][:30],x['calls'])for x in bad[:2]],exceptions[:2])
