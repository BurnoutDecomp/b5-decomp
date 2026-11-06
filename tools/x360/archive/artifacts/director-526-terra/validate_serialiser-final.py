from pathlib import Path
import json,random,hashlib,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'final-owned/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnReplayDirectorSerialiser.cpp'));SELF=F.M.S.SELF;FRAME=F.M.S.AUX+0x3000
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
def effects(c,n,t):
 if 'GetStaticLayout' in n:c.setg(3,FRAME);t.append('layout')
 elif 'BaseSerialiser__Lock' in n or '?Lock@BaseSerialiser' in n:t.append('lock')
 elif 'BaseSerialiser__Unlock' in n or '?Unlock@BaseSerialiser' in n:t.append('unlock');c.setg(3,0)
 elif 'BaseSerialiser__Write' in n or '?Write@BaseSerialiser' in n:t.append(('write',c.g(4)==FRAME,c.g(5)))
 elif 'BaseSerialiser__Read' in n or '?Read@BaseSerialiser' in n:t.append(('read',c.g(4)==FRAME,c.g(5)))
 elif 'Camera__Construct' in n or '?Construct@Camera@' in n:t.append(('camera',c.g(3)==FRAME))
 elif any(x in n for x in ['BeginAssert','FireAssert','EndAssert']):raise AssertionError(('assert',n))
rng=random.Random(0x82650438); count=0;traces=[]
for method,original in [('Read',0x82650340),('Write',0x82650438)]:
 sym='?'+method+'@DirectorSerialiser@BrnReplays@@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym);orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x%08X.json'%original))['assembly'].splitlines()
 for mode in range(8):
  for latch in (0,1):
   for i in range(8):
    initial=bytearray(rng.randbytes(0x60));initial[:4]=mode.to_bytes(4,'big');initial[0x5c]=latch;frame=rng.randbytes(0x180); both=[]
    for compiled in (False,True):
     c=F.cpu(b);c.mem.wr(SELF,initial);c.mem.wr(FRAME,frame);c.setg(3,SELF);trace=[]
     if compiled:
      for a,n in b.unresolved.items():c.hooks[a]=lambda cc,name=n:effects(cc,name,trace)
      for n,(oi,v,sec) in b.definitions.items():
       if (oi,sec) in b.bases and '?GetStaticLayout@DirectorSerialiser' in n:c.hooks[b.bases[(oi,sec)]+v]=lambda cc,name=n:effects(cc,name,trace)
      c.run(b.entry)
     else:
      for l in orig:
       if '  bl ' not in l:continue
       x=l.split();n=x[2]
       if n.startswith('__save') or n.startswith('__rest'):continue
       c.hooks[target(int(x[0],16))]=lambda cc,name=n:effects(cc,name,trace)
      c.run(original)
     both.append((c.mem.rd(SELF,0x60),c.mem.rd(FRAME,0x180),trace))
    if both[0]!=both[1]:raise AssertionError((method,mode,latch,i,both[0][2],both[1][2]))
    count+=1
   traces.append({'method':method,'mode':mode,'latch':latch,'trace':both[0][2]})
r={'cases':count,'traces':traces,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Actual original vs compiled DirectorSerialiser Read/Write, modes0–7 and latch0/1, 8 poisoned cases each; compare 0x60 serialiser bytes, 0x180 frame bytes, and ordered service calls/transfer lengths. All lock, static-layout, camera construction and base Read/Write services mocked; return value not compared, no streaming or playback end-to-end claim.'};(O/'serialiser-comparison-final.json').write_text(json.dumps(r,indent=2)+'\n');print('passed',count)
