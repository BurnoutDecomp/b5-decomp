from pathlib import Path
import json,random,sys,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xaudio-xenon-native-terra/final');row=json.loads((out/'compile-results.json').read_text())['results'][0];obj=F.Coff(Path(row['object']),'??1CXenonRenderer@XAUDIO@@');b=F.Bundle([obj]);b.select('??1CXenonRenderer@XAUDIO@@');SELF=F.M.S.SELF;MASK=0xffffffff;nc=F.address(b,'guXBMCaptureCursor');nm=F.address(b,'guXBMResetMarker');oc=0x82f37684;om=0x832be604
sym={name:at for at,name in b.unresolved.items()};prefix=lambda x:next(a for n,a in sym.items() if x in n);actual={prefix('?XenonRendererVTable@'):0x82109098,prefix('?XenonRendererRouterVTable@'):0x8210907c,prefix('?ObjectRefCountVTable@'):0x82108ff4}
def branch(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x3fffffc;return (pc+(d-0x4000000 if d&0x2000000 else d))&MASK
def run(comp,cursor,client,result,fails,msr,seed):
 c=F.cpu(b);c.mem.add(0x82f37000,0x1000);c.mem.add(0x832be000,0x1000);c.failures=fails;c.msr=msr;c.setg(13,0);data=bytearray(random.Random(seed).randbytes(0x40));struct.pack_into('>I',data,0x18,client);c.mem.wr(SELF,data);cp=nc if comp else oc;mp=nm if comp else om;F.put(c,cp,cursor);F.put(c,mp,0x87654321);calls=[]
 def unregister(x):calls.append([x.g(3)&MASK,int.from_bytes(x.mem.rd(cp,4),'big'),int.from_bytes(x.mem.rd(mp,4),'big'),x.msr]);x.setg(3,result)
 c.hooks[branch(0x82962f44)]=unregister;c.hooks[prefix('?XAudioUnregisterRenderDriverClient@')]=unregister;c.setg(3,SELF);c.run(b.entry if comp else 0x82962ec0);own=bytearray(c.mem.rd(SELF,0x40))
 if comp:
  for off in (0,4):
   x=int.from_bytes(own[off:off+4],'big');assert x in actual,(off,hex(x));struct.pack_into('>I',own,off,actual[x])
 return {'owner':own.hex(),'cursor':int.from_bytes(c.mem.rd(cp,4),'big'),'marker':int.from_bytes(c.mem.rd(mp,4),'big'),'atomic':[(w,val,ok) for addr,w,val,ok in c.atomic_trace],'msr':c.msr,'calls':calls}
errors=[];i=0
for cursor in (0,1,0x1800,0xffffffff,0x12345678):
 for client in (0,0x40012340):
  for result in (0,-1,1):
   for fail in (0,1,3):
    for msr in (0,0x8000):
     args=(cursor,client,result,fail,msr,i);a=run(False,*args);c=run(True,*args);i+=1
     if a!=c:errors.append({'args':args,'original':a,'native':c})
result={'scope':'Actual ARTIST vs native COFF destructor; unregister service controlled, vtable symbol addresses normalized to ARTIST values. Owner bytes, globals, atomic attempts, EE state and unregister call ordering compared. Real vtable identities and service implementation outside scope.','cases':i,'mismatches':errors,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'destructor-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',i,'mismatches',len(errors));print(json.dumps(errors[:1],indent=2)[:4000]);assert not errors
