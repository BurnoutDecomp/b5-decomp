from pathlib import Path
import sys,json,hashlib,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts');rng=random.Random(827049);reports={}
for kind in ('64','128'):
 root=D/('traffic-align'+kind+'-inferred'); j=json.loads((root/'compile-results.json').read_text()); rec=next(x for x in j['results'] if 'PickPaintColour' in x['source']);obj=Path(rec['object']);sym='?PickPaintColourForVehicle@VehicleTypeRuntime@BrnTraffic@@';o=F.Coff(obj,sym);b=F.Bundle([o]);b.select(sym)
 for i in range(96):
  colours=[rng.randrange(0,20) for _ in range(20)];count=rng.randrange(1,21);seed=rng.getrandbits(32);palette=bytes(rng.getrandbits(8) for _ in range(20*16));outs=[]
  for native in (False,True):
   c=F.cpu(b);record=F.M.S.SELF;out=F.M.S.AUX;pal=out+0x100;c.mem.wr(record+0x48,bytes(colours));c.mem.wr(record+0x5c,bytes([count]));c.mem.wr(pal,palette)
   for r,v in [(3,out),(4,record),(5,seed),(6,20),(7,pal)]:c.setg(r,v)
   c.run(b.entry if native else 0x827049a8);outs.append((c.g(3),c.mem.rd(out,16)))
  assert outs[0]==outs[1],(kind,i,outs)
 reports[kind]={'cases':96,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest()}
print(reports);(D/'traffic-align128-visits'/'paint-getter-comparison.json').write_text(json.dumps({'passed':True,'candidates':reports,'scope':'Whole actual compiled PickPaintColourForVehicle vs ARTIST 827049A8 for 96 valid randomized palette/count/seed cases per candidate. Exact return and all output bytes; assertions-success only, VMX emulator.'},indent=2)+'\n')
