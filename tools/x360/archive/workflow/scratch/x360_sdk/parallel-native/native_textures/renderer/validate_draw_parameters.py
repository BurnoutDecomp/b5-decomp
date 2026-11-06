from pathlib import Path
import hashlib,json,random,struct,sys
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'));sys.path.insert(0,str(parent/'tools/re'))
import native_atomic_fixture as F
import x360rd as R
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-draw-parameters1';r=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('native_draw_indexed_parameters.cpp'));p=Path(r['object']);o=F.Coff(p,'NativeDrawParametersDispatch');setter=F.Coff(p,'NativeDrawParametersSet');rng=random.Random(0x82400540);SELF=F.M.S.SELF;AUX=F.M.S.AUX;errors=[]
w=R.u32(0x82400554);off=w&0x03fffffc;sdk=(0x82400554+off-(1<<26 if off&(1<<25) else 0))&0xffffffff
class Stop(Exception):pass
for i in range(128):
 values=[rng.getrandbits(32) for _ in range(4)];both=[]
 c=F.cpu(setter);c.setg(3,SELF+8)
 for n,value in enumerate(values):c.setg(4+n,value)
 c.run(setter.entry);assert c.mem.rd(SELF+8,16)==struct.pack('>4I',*values)
 for compiled in [False,True]:
  c=F.cpu(o);c.mem.add(0x83271000,0x1000);F.put(c,0x83271608,AUX);c.mem.wr(SELF+8,struct.pack('>4I',*values));trace=[]
  def observe(x):trace.append([x.g(n)&0xffffffff for n in range(3,8)]);raise Stop()
  c.hooks[sdk]=observe
  for at,name in o.unresolved.items():
   if name=='D3DDevice_DrawIndexedVertices':c.hooks[at]=observe
  if compiled:c.setg(3,AUX);c.setg(4,SELF+8)
  else:c.setg(30,SELF);c.setg(31,0x83270000)
  try:c.run(o.entry if compiled else 0x82400540)
  except Stop:pass
  both.append(trace)
 if both[0]!=both[1]:errors.append(dict(case=i,values=values,traces=both))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='actual four setters/getters and SDK consumer ABI versus ARTIST SkyRender82400540..554: primitive,base vertex,start index,index count;native default ctor separatelycovered by64wholeRendererC++ctors',limits='SDK draw is a controlled boundary;no rendering or vertex/index-resource claim')
(out/'draw-parameters-abi.json').write_text(json.dumps(result,indent=2)+'\n');print('128 native DrawIndexedParameters consumer ABI cases; errors',len(errors));raise SystemExit(bool(errors))
