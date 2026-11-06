from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/director-526-terra')
paths={}
for d in ['ice-geometry-overlay','replay-score']:
 for row in json.loads((out/d/'compile-results.json').read_text())['results']:
  if row['passed']: paths[Path(row['source']).name]=Path(row['object'])
b=F.Bundle([F.Coff(paths['BrnReplayDirector.cpp'],'??0ReplayDirector@'),F.Coff(paths['BrnGameStateSharedIO.cpp'],'??0CarScoreData@')]);b.select('??0ReplayDirector@')
vt={'??_7BehaviourGameplayExternal@Camera@BrnDirector@@6B@':0x8200A560,'??_7CollisionPolicyAttachedToVehicle@Camera@BrnDirector@@6B@':0x8200A150,'??_7BehaviourGameplayBumper@Camera@BrnDirector@@6B@':0x82008D38}
for (oi,sec),base in b.bases.items():
 obj=b.objects[oi];body=b.images[base]
 for at,index,kind in obj.relocations(sec):
  name,_,_=obj.symbols[index]
  if name not in vt:continue
  address=vt[name];word=struct.unpack_from('>I',body,at)[0]
  if kind in [0x10,0x11]:word=(word&0xffff0000)|(((address+0x8000)>>16 if kind==0x10 else address)&0xffff)
  elif kind==2:word=address
  else:raise AssertionError((name,kind))
  struct.pack_into('>I',body,at,word)
rng=random.Random(0x827E2020);errors=[];self=F.M.S.SELF
for i in range(64):
 image=rng.randbytes(0x4000);both=[]
 for compiled in (False,True):
  c=F.cpu(b);c.mem.wr(self,image);c.setg(3,self)
  if compiled:
   for at,name in b.unresolved.items():
    if name.startswith('??0ICETake@'):
     c.hooks[at]=lambda x:x.run(0x822145E8)
  c.run(b.entry if compiled else 0x827E2020)
  both.append((c.mem.rd(self,0x4000),c.g(3)&0xffffffff))
 if both[0]!=both[1]:errors.append({'case':i,'diff':[hex(n) for n,(a,z) in enumerate(zip(both[0][0],both[1][0])) if a!=z]})
result={'cases':64,'errors':errors,'vtables':{n:hex(a) for n,a in vt.items()},'objects':{n:{'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for n,p in paths.items()},'scope':'entire 0x4000 random-filled region and return, native compiled Replay plus compiled Score vs ARTIST constructor; real original ICE constructor executed on both sides','limits':'ICE service uses original ARTIST code on both sides; vtable relocation identities normalized to original addresses; no gameplay or native launch proof'}
p=out/'replay-ctor-comparison-ice.json';p.write_text(json.dumps(result,indent=2)+'\n');print('64 replay comparisons, errors',len(errors));print(errors[:1]);sys.exit(bool(errors))


