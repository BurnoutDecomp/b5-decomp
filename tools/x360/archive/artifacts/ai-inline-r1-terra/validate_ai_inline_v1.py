from pathlib import Path
import json,random,struct,sys,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
O=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ai-inline-r1-terra');obj=O/'ai_inline_probes.obj';A=F.M.S.AUX;S=F.M.S.SELF;CODE=A+0x5000;AI=A+0x1000;CORNERS=A+0x2000

def bundle(name):
 b=F.Bundle([F.Coff(obj,'?'+name)]);b.select('?'+name);return b
bs={n:bundle(n) for n in ['ai_prev_left','ai_prev_right','ai_current_left','ai_current_right','ai_entrance','ai_exit','ai_set_entrance','ai_set_exit','ai_set_corners']}
def fbits(x):return struct.pack('>f',x)
def call_get(n,state):
 b=bs[n];c=F.cpu(b);c.mem.wr(S,state);c.setg(3,S);c.run(b.entry);return struct.pack('>4I',*c.vr[1])
def perm_orig(raw):
 b=bs['ai_exit'];c=F.cpu(b);c.mem.add(CODE,0x1000);c.mem.wr(CODE,struct.pack('>II',F.M.V.x360rd.u32(0x82780688),0x4e800020));c.vr[124]=struct.unpack('>4I',raw);c.run(CODE);return struct.pack('>4I',*c.vr[123])
def setter(n,state,value):
 b=bs[n];c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(A,value);c.setg(3,S);c.setg(4,A);c.run(b.entry);return c.mem.rd(S,len(state))
def exit_orig(portal,value):
 b=bs['ai_exit'];c=F.cpu(b);c.mem.add(CODE,0x1000);c.mem.wr(CODE,struct.pack('>III',F.M.V.x360rd.u32(0x8278f5d8),F.M.V.x360rd.u32(0x8278f5e0),0x4e800020));c.vr[13]=struct.unpack('>4I',portal);c.vr[0]=struct.unpack('>4I',value);c.run(CODE);return struct.pack('>4I',*c.vr[13])
def corners(compiled,state,values):
 b=bs['ai_set_corners'];c=F.cpu(b);c.mem.wr(S,state);c.mem.wr(AI+8,struct.pack('>I',CORNERS));c.mem.wr(CORNERS,values)
 if compiled:c.setg(3,S);c.setg(4,AI);c.run(b.entry)
 else:
  c.mem.add(CODE,0x1000);code=b''.join(struct.pack('>I',F.M.V.x360rd.u32(pc)) for pc in range(0x8278ee08,0x8278ee84,4));c.mem.wr(CODE,code+struct.pack('>I',0x4e800020));c.setg(30,AI);c.setg(31,S);c.setg(20,0x20);c.setg(21,0x30);c.run(CODE)
 return c.mem.rd(S,len(state))
rng=random.Random(0x8278ee08);checks=0
for i in range(256):
 state=rng.randbytes(0xb0);value=rng.randbytes(16)
 for name,off in [('ai_prev_left',0),('ai_prev_right',16),('ai_exit',0x40)]:
  if call_get(name,state)!=perm_orig(state[off:off+16]):raise AssertionError((i,name))
  checks+=1
 for name,off in [('ai_current_left',0),('ai_current_right',16),('ai_entrance',0x40)]:
  if call_get(name,state)!=state[off:off+16]:raise AssertionError((i,name))
  checks+=1
 for name in ['ai_set_exit','ai_set_entrance']:
  out=setter(name,state,value);expected=bytearray(state);expected[0x40:0x50]=exit_orig(state[0x40:0x50],value) if name.endswith('exit') else value[:8]+state[0x48:0x50]
  if out!=bytes(expected):raise AssertionError((i,name))
  checks+=1
 # Include signed zero, subnormal, extreme finite, and ordinary finite numbers.
 words=[0,0x80000000,1,0x80000001,0x00800000,0x80800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff]
 vals=b''.join(struct.pack('>I',rng.choice(words) if i%3==0 else struct.unpack('>I',fbits(rng.uniform(-1e5,1e5)))[0]) for _ in range(8))
 a=corners(False,state,vals);z=corners(True,state,vals)
 if a!=z:raise AssertionError((i,'corners',[(hex(k),hex(x),hex(y)) for k,(x,y) in enumerate(zip(a,z)) if x!=y][:12]))
 checks+=1
r={'iterations':256,'checks':checks,'scope':'Actual-header noinline probes; raw 16-byte current/previous/portal getters and setters, full section state; original ARTIST 82780688 permute and 8278F5D8/E0 exit insertion; corners compared to original 8278EE08..80 full section including vector subtract, signed zero, subnormal, finite extremes, VMX emulator model. Entrance insertion checked against exact raw XY/preserved ZW lane semantics. No whole World/game claim.','object':str(obj),'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'source':str(O/'ai_inline_probes.cpp'),'source_sha256':hashlib.sha256((O/'ai_inline_probes.cpp').read_bytes()).hexdigest()};(O/'inline-comparison-v1.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
