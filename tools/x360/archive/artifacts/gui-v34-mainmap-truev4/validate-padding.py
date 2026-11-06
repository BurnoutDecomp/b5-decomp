from pathlib import Path
import json,hashlib,random,struct,sys
P=Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path(__file__).parent;obj=Path(json.loads((D/'compile-results.json').read_text())['results'][0]['object']);o=F.Coff(obj,'?CalculateViewPaddingOffset@MainMapComponent@BrnGui@@');b=F.Bundle([o]);b.select('?CalculateViewPaddingOffset@MainMapComponent@BrnGui@@');rng=random.Random(1257)
for i in range(96):
 rects=[[rng.uniform(-10000,10000) for _ in range(4)] for n in range(2)];outs=[]
 for native in (False,True):
  c=F.cpu(b);base=F.M.S.SELF;ret=F.M.S.AUX;c.mem.wr(base+0x620,struct.pack('>4f',*rects[0]));c.mem.wr(base+0x630,struct.pack('>4f',*rects[1]));c.mem.wr(ret,bytes.fromhex('aabbccdd'*4));c.setg(3,ret);c.setg(4,base);c.run(b.entry if native else 0x82447d38);outs.append(c.mem.rd(ret,16))
 assert outs[0]==outs[1],(i,rects,[v.hex() for v in outs])
report={'passed':True,'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Extracted verbatim CalculateViewPaddingOffset method probe against ARTIST 82447D38; actual r3 output and r4 receiver, full 16-byte output, random finite rects. Probe is not the whole MainMap CPP; exception and runtime coverage excluded.'};(D/'padding-comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(report)
