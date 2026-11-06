from pathlib import Path
import hashlib,json,random,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-buffer-construction1';r=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('native_im_render_buffer_construct.cpp'));p=Path(r['object']);objects={k:F.Coff(p,'NativeImBufferConstruct'+k) for k in ['2d','3d','Untex']}
SELF=F.M.S.SELF;specs=[(0xaec,'2d'),(0x1094,'3d'),(0x1274,'Untex'),(0x1554,'3d'),(0x159c,'2d'),(0x1730,'3d'),(0x1778,'3d')];rng=random.Random(0x8240AA20);errors=[]
class StopRegion(Exception):pass
for i in range(128):
 image=rng.randbytes(0xca80);c=F.cpu(objects['2d']);c.mem.wr(SELF,image);c.setg(31,SELF);c.setg(30,0)
 def stop(x):raise StopRegion()
 c.hooks[0x823FCD90]=stop
 try:c.run(0x8240AA20)
 except StopRegion:pass
 else:raise AssertionError('original did not reach exact Corona boundary')
 for offset,kind in specs:
  compiled=F.cpu(objects[kind]);compiled.mem.wr(SELF,image);compiled.setg(3,SELF+offset);compiled.run(objects[kind].entry)
  original=c.mem.rd(SELF+offset,0x44);actual=compiled.mem.rd(SELF+offset,0x44)
  if original!=actual:errors.append(dict(case=i,offset=hex(offset),kind=kind,different_bytes=[hex(n) for n,(a,b) in enumerate(zip(original,actual)) if a!=b]))
  assert actual[0x35:0x3c]==image[offset+0x35:offset+0x3c]
  assert actual[0x42:0x44]==image[offset+0x42:offset+0x44]
result=dict(cases=896,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='three actual generic header bodies against seven ARTIST8240AA20..ACCC inline regions,all68bytes;preserve render-start counter+38 and natural padding',limits='original region stops before Corona::Construct;this proves the stream reset itself,not Prepare/storage allocation or command dispatch')
(out/'buffer-construct-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('896 native buffer Construct comparisons; errors',len(errors));print(errors[:3]);raise SystemExit(bool(errors))
