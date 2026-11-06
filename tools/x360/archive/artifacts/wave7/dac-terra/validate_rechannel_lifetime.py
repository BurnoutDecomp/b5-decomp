import sys,json,random,struct,hashlib
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/dac-terra');sys.path.insert(0,str(out));from weak_bundle import F,WeakBundle
p={'rechannel_probe.cpp':out/'rechannel-kernel-build/rechannel_probe.obj','Rechannel.cpp':out/'rechannel-kernel-build/Rechannel.obj'};errors=[];rows=[];rng=random.Random(0x16c8);selfaddr=F.M.S.SELF;aux=F.M.S.AUX
for klass,orig,probe,obj,origvtable in [('Rechannel',0x82ba1c20,'?NativeRechannelLifetime','Rechannel.cpp',0x8217f4f4)]:
 b=WeakBundle([F.Coff(p['rechannel_probe.cpp'],probe),F.Coff(p[obj],'?CreateInstance@'+klass+'@')]);b.select(probe);weak=next((x for x in b.weak_resolved if x[0].startswith('??_E'+klass+'@')),None);assert weak,klass
 basevts={b.bases[(oi,sec)]+v for oi,o in enumerate(b.objects) for n,v,sec in o.symbols.values() if n.startswith('??_7PlugIn@') and (oi,sec) in b.bases}
 for i in range(128):
  initial=bytearray(rng.randbytes(0x3200));both=[];delete=i&1
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(selfaddr,initial);c.setg(3,selfaddr);c.setg(4,delete);trace=[]
   def free(x):trace.append(x.g(3)&0xffffffff)
   c.hooks[0x82c08fb0]=free
   for at,name in b.unresolved.items():
    if name.startswith('??3@YAXPAX'):c.hooks[at]=free
   if not compiled:F.put(c,selfaddr,origvtable)
   c.run(b.entry if compiled else orig)
   got=int.from_bytes(c.mem.rd(selfaddr,4),'big');expected=basevts if compiled else {0x820aa810}
   if got not in expected:errors.append({'class':klass,'case':i,'vtable':hex(got),'expected':[hex(x) for x in expected]})
   memory=bytearray(c.mem.rd(selfaddr,0x3200));memory[:4]=bytes(4);both.append((memory,trace))
  if both[0]!=both[1]:errors.append({'class':klass,'case':i,'trace':[both[0][1],both[1][1]]})
 rows.append({'class':klass,'original':hex(orig),'weak_fallback':weak,'cases':128,'object':str(p[obj]),'sha256':hashlib.sha256(p[obj].read_bytes()).hexdigest()})
report={'cases':128,'mismatches':errors,'rows':rows,'limits':'Actual compiled constructed class vtable slot3 calls for explicit destructor flags0 and virtual delete flags1 versus original scalar deleting destructors. Private test resolver follows COFF weak TagIndex SEARCH_LIBRARY only when no strong/library provider; operator delete controlled, base-vtable addresses normalized but asserted separately. No array/vector delete behavior, full game link, real heap or hardware claim.'};(out/'rechannel-lifetime-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print('128 cases',len(errors),'mismatches');print(errors[:1]);assert not errors
