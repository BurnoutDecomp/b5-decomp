from pathlib import Path
import json,struct,hashlib,random,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'helper-index/compile-results.json').read_text());obj=next(Path(x['object']) for x in j['results'] if x['source'].endswith('BrnMainDirector.cpp'));sym='??0BehaviourManager@Camera@BrnDirector@@QAA@XZ';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
vt={'??_7?$AbstractPool@$0PK@$07UVector4@vpu@math@rw@@@BrnDirector@@6B@':0x820cf074,'??_7?$AbstractPool@$0GE@$0BE@UVector4@vpu@math@rw@@@BrnDirector@@6B@':0x820cf078}
for (oi,sec),base in b.bases.items():
 ob=b.objects[oi];body=b.images[base]
 for at,idx,kind in ob.relocations(sec):
  name,_,_=ob.symbols[idx]
  if name not in vt:continue
  addr=vt[name];w=struct.unpack_from('>I',body,at)[0]
  if kind in (0x10,0x11):w=(w&0xffff0000)|(((addr+0x8000)>>16 if kind==0x10 else addr)&0xffff)
  elif kind==2:w=addr
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,w)
def target(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
s=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x827E26A8.json'))['assembly'].splitlines();SELF=F.M.S.SELF;rng=random.Random(0x827e26a8)
for i in range(64):
 poison=rng.randbytes(0x163d0);outs=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(SELF,poison);c.setg(3,SELF)
  if compiled:
   for at in b.unresolved:c.hooks[at]=lambda cc:None
   c.run(b.entry)
  else:
   c.run(0x827e26a8)
  outs.append(c.mem.rd(SELF,0x163d0))
 if outs[0]!=outs[1]:raise AssertionError((i,[hex(k) for k,(x,y) in enumerate(zip(*outs)) if x!=y][:20]))
r={'cases':64,'bytes_per_case':0x163d0,'object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Compare 0x163D0 bytes of original vs actual compiled BehaviourManager constructor, including actual nested array constructors, with two vtable relocations normalized. This is constructor-only evidence; does not prove lifecycle or gameplay.'};(O/'behaviour-helper-comparison.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
