from pathlib import Path
import json,random,struct,hashlib,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra');j=json.loads((O/'parameters-typed-wip4/compile-results.json').read_text());paths={Path(x['source']).name:Path(x['object']) for x in j['results'] if x['passed']}
S=F.M.S.SELF;A=F.M.S.AUX; SOURCE=A; DATA=A+0x1000
rng=random.Random(0x821f9228); result={}
for kind,address,size in [('External',0x821f9228,0xb0),('Bumper',0x821f94c8,0x50)]:
 obj=paths['BrnBehaviourGameplay'+kind+'.cpp'];sym='?Set@Parameters@BehaviourGameplay'+kind+'@';b=F.Bundle([F.Coff(obj,sym)]);b.select(sym)
 orig=json.load(open('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x%08X.json'%address))['assembly'].splitlines()
 def target(pc):
  w=F.M.V.x360rd.u32(pc);d=w&0x03fffffc;d=d-0x04000000 if d&0x02000000 else d;return (pc+d)&0xffffffff
 def effect(c,n):
  raise AssertionError((kind,'unexpected call',n,hex(c.last_pc)))
 for i in range(128):
  initial=rng.randbytes(size); data=b''.join(struct.pack('>f',rng.uniform(0.1,90)) for _ in range(0x44//4));images=[]
  for compiled in (False,True):
   c=F.cpu(b);c.mem.wr(S,initial);c.mem.wr(SOURCE,struct.pack('>II',0x8abc0000,DATA));c.mem.wr(DATA,data);c.setg(3,S);c.setg(4,SOURCE)
   if compiled:
    for at,n in b.unresolved.items():c.hooks[at]=lambda cc,name=n:effect(cc,name)
    c.run(b.entry)
   else:
    for l in orig:
     if '  bl ' not in l:continue
     p=l.split();n=p[2]
     if n.startswith('__save') or n.startswith('__rest'):continue
     c.hooks[target(int(p[0],16))]=lambda cc,name=n:effect(cc,name)
    c.run(address)
   im=bytearray(c.mem.rd(S,size));im[4:8]=b'\0'*4;images.append(im)
  if images[0]!=images[1]:raise AssertionError((kind,i,[hex(k) for k,(x,y) in enumerate(zip(*images)) if x!=y][:20]))
 result[kind]={'cases':128,'object':str(obj),'sha256':hashlib.sha256(obj.read_bytes()).hexdigest()}
 print(kind,'128 pass')
result['scope']='Actual compiled typed instance parameter setters versus ARTIST, randomized positive data, full parameter memory with debug-name string pointers normalized; no assertion paths or wider gameplay.'
(O/'parameters-typed-comparison-wip.json').write_text(json.dumps(result,indent=2)+'\n')
