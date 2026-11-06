from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);o=F.Coff(obj,'?Prepare@MainMapComponent@BrnGui@@');b=F.Bundle([o]);b.select('?Prepare@MainMapComponent@BrnGui@@');symbol=[a for a,n in b.unresolved.items() if 'smv4WorldRect' in n];assert len(symbol)==1;rng=random.Random(9938)
for i in range(96):
 vals=struct.pack('>4f',*[rng.uniform(-10000,10000) for _ in range(4)]);outs=[]
 for native in (False,True):
  c=F.cpu(b);c.mem.add(0x82fb3000,0x1000);c.mem.wr(symbol[0] if native else 0x82fb31f0,vals);base=F.M.S.SELF;c.mem.wr(base+0x650,bytes.fromhex('aabbccdd'*4));c.mem.wr(base+0x67c,b'\x99');c.setg(3,base);c.run(b.entry if native else 0x8244f4a8);outs.append((c.g(3),c.mem.rd(base+0x650,16),c.mem.rd(base+0x67c,1)))
 assert outs[0]==outs[1],(i,[str(x) for x in outs])
r={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Extracted verbatim Prepare method probe against ARTIST 8244F4A8. Original global vs native unresolved symbol explicitly supplied same finite input; compares return, active byte, all 16 centre bytes. Not whole MainMap CPP or runtime proof.'};(D/'prepare-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(r)
