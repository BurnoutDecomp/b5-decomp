from pathlib import Path
import sys,json,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts');class_stop=type('Stop',(Exception,),{}); reports={}
for kind in ('64','128'):
 root=D/('traffic-align64-visits' if kind=='64' else 'traffic-align128-final');obj=Path(json.loads((root/'compile-results.json').read_text())['results'][0]['object']);o=F.Coff(obj,'TrafficRuntimeVisit');b=F.Bundle([o]);v,sec=next((v,sec) for n,v,sec in o.symbols.values() if n=='TrafficRuntimeVisit');b.definitions['?Visitor']=(0,v,sec);b.select('?Visitor');outs=[]
 for original in (True,False):
  c=F.cpu(b); base=0x41000000;trace=[]
  def hook(c):trace.append(c.g(3)&0xffffffff)
  if original:
   c.hooks[0x827521e8]=hook;c.setg(31,base);c.setg(26,base+0x76380);c.setg(27,96)
   c.instruction_observer=lambda pc,w: (_ for _ in ()).throw(class_stop()) if pc==0x82741444 else None
   try:c.run(0x8274142c)
   except class_stop:pass
  else:
   callback=0x84803000;c.hooks[callback]=hook;c.setg(3,base);c.setg(4,callback);c.run(b.entry)
  outs.append(trace)
 assert outs[0]==outs[1] and len(outs[0])==96,(kind,outs)
 reports[kind]={'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'addresses':len(outs[0]),'first':hex(outs[0][0]),'last':hex(outs[0][-1])}
print(reports);(D/'traffic-align128-final'/'array-visit-comparison.json').write_text(json.dumps({'passed':True,'candidates':reports,'scope':'Actual ARTIST Construct call loop versus independently compiled real typed-header diagnostic visitor, raw callback receiver r3 for every one of 96 elements, no address normalization. Constructor service controlled; no full Construct claim. Both inferred class-alignment candidates.'},indent=2)+'\n')
