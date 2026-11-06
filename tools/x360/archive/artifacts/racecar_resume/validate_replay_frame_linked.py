from pathlib import Path
import sys,json,hashlib,random
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');out=root/'replay_frame_clear_final';sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
report=json.loads((out/'compile-results.json').read_text());byname={Path(x['source']).name:Path(x['object']) for x in report['results'] if x['passed']};a=F.Coff(byname['BrnReplayRaceCarSerialiserFrame.cpp'],'?Clear@RaceCarSerialiserFrame@');b=F.Coff(byname['BrnActiveRaceCarRenderParams.cpp'],'?Reset@RenderParams@ActiveRaceCar@');c1=F.Coff(root/'replay_output_clear_probes/ActiveClear.obj','?Clear@RCEntityActiveRaceCarOutputInterface@');c2=F.Coff(root/'replay_output_clear_probes/GlobalClear.obj','?Clear@RCEntityGlobalRaceCarOutputInterface@');bundle=F.Bundle([a,b,c1,c2]);bundle.select('?Clear@RaceCarSerialiserFrame@');print('unresolved',bundle.unresolved)
FRAME=0x41000000;SIZE=0xd7e0
original=json.loads(Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x82657748.json').read_text(encoding='utf8'))
def noop(c):c.setg(3,0)
def memzero(c):
 addr=c.g(3)&0xffffffff;value=c.g(4)&255;size=c.g(5)&0xffffffff;assert size==0x14a0,(hex(addr),value,size);c.mem.wr(addr,bytes([value])*size);c.setg(3,addr)
def reject(c):raise AssertionError('assert path')
def run(data,compiled):
 c=F.cpu(bundle);c.mem.add(FRAME,SIZE,data);c.setg(3,FRAME)
 for x in original['xrefs_from']:
  name=x['name'];addr=int(x['address'],16)
  if name.startswith('__') or 'RCEntityActiveRaceCarOutputInterface::Clear' in name or 'RCEntityGlobalRaceCarOutputInterface::Clear' in name:continue
  c.hooks[addr]=memzero if name=='memset' else reject if 'Assert::' in name else noop
 for addr,name in bundle.unresolved.items():
  c.hooks[addr]=memzero if name=='memset' else reject if 'Assert' in name else noop
 c.run(bundle.entry if compiled else 0x82657748);return c.mem.rd(FRAME,SIZE),c.g(3)&0xffffffff
rng=random.Random(3601031);m=[]
for i in range(128):
 data=bytes(rng.getrandbits(8) for _ in range(SIZE));old=run(data,False);new=run(data,True)
 if old[0]!=new[0]:
  m.append({'case':i,'diffs':[hex(j) for j in range(SIZE) if old[0][j]!=new[0][j]][:50],'old_return':hex(old[1]),'new_return':hex(new[1])})
res={'cases':128,'mismatches':m,'clear_object':str(a.path),'clear_sha256':hashlib.sha256(a.path.read_bytes()).hexdigest(),'reset_object':str(b.path),'reset_sha256':hashlib.sha256(b.path.read_bytes()).hexdigest(),'scope':'full 0xD7E0 bytes; incidental return register ignored, compiled Clear plus actual compiled RenderParams::Reset vs ARTIST 82657748, actual source-extracted active/global output Clear and real ARTIST callees executed, memset faithfully modeled; assert paths excluded'};(root/'replay-frame-clear-linked-comparison.json').write_text(json.dumps(res,indent=2),encoding='utf8');print('cases',128,'mismatches',len(m));assert not m
