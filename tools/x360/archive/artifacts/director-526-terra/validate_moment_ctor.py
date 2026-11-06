from pathlib import Path
import json,struct,hashlib,random,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'moment-constructor/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnMainDirector.cpp')); sym='??0MainDirector@BrnDirector@@QAA@XZ'; b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
vt='??_7?$AbstractPool@$0EG@$0BE@UVector4@vpu@math@rw@@@BrnDirector@@6B@'
for (oi,sec),base in b.bases.items():
 ob=b.objects[oi];body=b.images[base]
 for at,idx,kind in ob.relocations(sec):
  name,_,_=ob.symbols[idx]
  if name!=vt:continue
  addr=0x820CF070;w=struct.unpack_from('>I',body,at)[0]
  if kind in (0x10,0x11):w=(w&0xffff0000)|(((addr+0x8000)>>16 if kind==0x10 else addr)&0xffff)
  elif kind==2:w=addr
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x827E4AB8.json'))['assembly'].splitlines();SELF=F.M.S.SELF;rng=random.Random(0x827e4ab8)
for i in range(64):
 poison=rng.randbytes(0x57f0); sentinels=[rng.randbytes(4) for _ in range(5)];outs=[]
 for compiled in (False,True):
  c=F.cpu(b); c.mem.add(SELF+0x20000,0x20000);c.mem.wr(SELF,b'\0'*0x20000);c.mem.wr(SELF+0x20000,b'\0'*0x20000);base=SELF+(0x17270 if compiled else 0x172d0);c.mem.wr(base,poison); offsets=([0x40,0x330b0,0x353d0,0x353d4,0x353d8] if compiled else [0x40,0x33100,0x35420,0x35424,0x35428]);
  for off,raw in zip(offsets,sentinels):c.mem.wr(SELF+off,raw)
  c.setg(3,SELF)
  if compiled:
   for at in b.unresolved:c.hooks[at]=lambda cc:None
   for n,(oi,v,sec) in b.definitions.items():
    if (oi,sec) in b.bases and any(k in n for k in ['??0Arbitrator@','??0BehaviourManager@']):c.hooks[b.bases[(oi,sec)]+v]=lambda cc:None
   c.run(b.entry)
  else:
   for l in orig:
    if '  bl ' not in l:continue
    p=l.split();name=p[2]
    if name.startswith('__save') or name.startswith('__rest'):continue
    c.hooks[target(int(p[0],16))]=lambda cc:None
   c.run(0x827e4ab8)
  assert all(c.mem.rd(SELF+off,4)==raw for off,raw in zip(offsets,sentinels)), ('initializer store',compiled,i)
  outs.append(c.mem.rd(base,0x57f0))
 if outs[0]!=outs[1]:raise AssertionError((i,[hex(k) for k,(x,y) in enumerate(zip(*outs)) if x!=y][:20]))
r={'cases':64,'bytes_per_case':0x57f0,'untouched_fields_per_case':5,'original_offset':'0x172d0','candidate_offset':'0x17270','object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Original and actual compiled MainDirector constructor under no-op subconstructor hooks; compare 0x57F0 moment pool bytes and five independently poisoned fields, with moment pool vtable relocation normalized to ARTIST. No whole MainDirector constructor or initial allocation semantics claim.'};(O/'moment-constructor-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
