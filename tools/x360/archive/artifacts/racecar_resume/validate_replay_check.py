from pathlib import Path
import sys,json,hashlib,random
root=Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_resume');out=root/'replay_frame_clear_v2';sys.path.insert(0,'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk');import native_atomic_fixture as F
report=json.loads((out/'compile-results.json').read_text());byname={Path(x['source']).name:Path(x['object']) for x in report['results'] if x['passed']};a=F.Coff(byname['BrnReplayRaceCarEntitySerialiser.cpp'],'?CheckStaticLayoutCleared@RaceCarEntitySerialiser@');b=F.Coff(byname['BrnReplayRaceCarSerialiserFrame.cpp'],'?Clear@RaceCarSerialiserFrame@');bundle=F.Bundle([a,b]);bundle.select('?CheckStaticLayoutCleared@RaceCarEntitySerialiser@');SER=0x41000000;LAYOUT=0x42000000;SIZE=0x1afc0
orig_get=0x822a3400;orig_clear=0x82657748;native_get=F.address(bundle,'?GetStaticLayout@RaceCarEntitySerialiser@');native_clear=F.address(bundle,'?Clear@RaceCarSerialiserFrame@')
def run(data,frame,already,hasbuf,compiled):
 c=F.cpu(bundle);c.mem.add(SER,0x100,data);c.mem.add(LAYOUT,SIZE,frame);c.mem.wr(SER+0x5c,bytes([already]));trace=[]
 def get(cpu):
  trace.append(['get',cpu.g(3)&0xffffffff]);cpu.setg(3,LAYOUT if hasbuf else 0)
 def clear(cpu):
  addr=cpu.g(3)&0xffffffff;trace.append(['clear',addr]);assert addr in (LAYOUT,LAYOUT+0xd7e0);cpu.mem.wr(addr,b'\x42');cpu.setg(3,0)
 c.hooks[orig_get if not compiled else native_get]=get;c.hooks[orig_clear if not compiled else native_clear]=clear;c.setg(3,SER);c.run(bundle.entry if compiled else 0x82657a48);return c.mem.rd(SER,0x100),c.mem.rd(LAYOUT,SIZE),trace
rng=random.Random(3601031);errors=[]
for i in range(64):
 data=bytes(rng.getrandbits(8) for _ in range(0x100));frame=bytes(rng.getrandbits(8) for _ in range(SIZE));already=i&1;hasbuf=(i>>1)&1;a1=run(data,frame,already,hasbuf,False);a2=run(data,frame,already,hasbuf,True)
 if a1!=a2:errors.append({'case':i,'original_trace':a1[2],'native_trace':a2[2]})
res={'cases':64,'mismatches':errors,'original':'82657A48','native_object':str(a.path),'native_sha256':hashlib.sha256(a.path.read_bytes()).hexdigest(),'scope':'Complete CheckStaticLayoutCleared logic: four combinations of prior flag and buffer presence, all serializer bytes, both frames and ordered GetStaticLayout/Clear calls; Clear and getter controlled. r3 is ignored, as in sole original caller 8230DA90.'};(root/'replay-check-transition-comparison.json').write_text(json.dumps(res,indent=2),encoding='utf8');print('transition cases',64,'mismatches',len(errors));assert not errors,errors[:1]
