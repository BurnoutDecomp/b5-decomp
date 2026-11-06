from pathlib import Path
import json,random,sys,struct,hashlib
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xcam-native-terra/final');row=next(x for x in json.loads((out/'compile-results.json').read_text())['results'] if x['source'].endswith('XCamDecoder.cpp'));obj=F.Coff(Path(row['object']),'?DecodeFrame@CDecoder@XCAM@@');bundle=F.Bundle([obj]);bundle.select('?DecodeFrame@CDecoder@XCAM@@');SELF=F.M.S.SELF;AUX=F.M.S.AUX;CALLBACK=0x84803000;MASK=0xffffffff

def branch(pc):
 w=F.M.V.x360rd.u32(pc);d=w&0x3fffffc;d=d-0x4000000 if d&0x2000000 else d;return (pc+d)&MASK
orig={'set':branch(0x82988138),'decode':branch(0x82988148),'close':branch(0x829881b8),'init':branch(0x829881c0),'reset':branch(0x829881c8),'buffers':branch(0x82988200),'output':branch(0x82988224),'succeed':branch(0x82988234)}

def fixture(compiled,status,skip,fails,initial,i):
 c=F.cpu(bundle);c.failures=fails;c.msr=0x8000;trace=[];rng=random.Random(821+i);data=bytearray(rng.randbytes(0x200));struct.pack_into('>I',data,0,initial);struct.pack_into('>I',data,4,0x42345678);struct.pack_into('>I',data,0x10,AUX+0x100);c.mem.wr(SELF,data);F.put(c,AUX+0x100,AUX+0x200);F.put(c,AUX+0x204,CALLBACK)
 def hook(name):
  def f(x):
   if name=='set':trace.append([name,x.g(3)&MASK,x.g(4)&MASK])
   elif name=='decode':trace.append([name,x.g(3)&MASK,x.g(5)&MASK]);x.setg(3,status)
   elif name=='callback':trace.append([name,x.g(3)&MASK])
   elif name in ('close','reset'):trace.append([name,x.g(3)&MASK]);x.setg(3,0)
   elif name=='init':trace.append([name,x.g(3)&MASK]);x.setg(3,0)
   elif name=='buffers':
    trace.append([name,x.g(3)&MASK]);
    for k,reg in enumerate((4,5,6)):F.put(x,x.g(reg)&MASK,0x44000000+0x100*k);F.put(x,(x.g(reg)&MASK)+4,0x40+0x10*k)
   elif name=='output':trace.append([name,x.g(3)&MASK,x.g(4)&MASK]+[x.g(j)&MASK for j in range(5,11)]);x.setg(3,0)
   elif name=='succeed':trace.append([name,x.g(3)&MASK])
  return f
 c.hooks[CALLBACK]=hook('callback')
 for name,addr in orig.items():c.hooks[addr]=hook(name)
 mapping={'WMVideoDecSetErrorRecoveryFrameType':'set','WMVideoDecDecodeData':'decode','GetBuffersForWriting':'buffers','WMVideoDecPlanarYUVGetOutput':'output','BufferWriteSucceeded':'succeed','WMVideoDecReset':'reset','WMVideoDecClose':'close'}
 for at,name in bundle.unresolved.items():
  for frag,k in mapping.items():
   if frag in name:c.hooks[at]=hook(k);break
 c.hooks[F.address(bundle,'?InitializeCodec@CDecoder@XCAM@@')]=hook('init');c.setg(3,SELF);c.setg(4,skip);c.setg(5,17);c.run(bundle.entry if compiled else 0x82988118)
 return {'return':c.g(3)&MASK,'state':c.mem.rd(SELF,0x200).hex(),'trace':trace,'atomic':c.atomic_trace,'msr':c.msr}
errors=[];case=0
for status in (-100,0,1,3,4,11,9):
 for skip in (0,1):
  for fails in (0,1,3):
   for initial in (0,0xffffffff,0x7fffffff):
    a=fixture(False,status,skip,fails,initial,case);b=fixture(True,status,skip,fails,initial,case);case+=1
    if a!=b:errors.append({'status':status,'skip':skip,'fails':fails,'initial':initial,'original':a,'native':b})
result={'scope':'Whole actual DecodeFrame versus ARTIST under controlled codec, output and virtual notification services. State bytes, service call order/arguments, return, retry trace and interrupt state compared. Service implementations and concurrent hardware excluded.','cases':case,'mismatches':errors,'object':row['object'],'object_sha256':hashlib.sha256(Path(row['object']).read_bytes()).hexdigest()};(out/'decoder-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print('cases',case,'errors',len(errors));print(json.dumps(errors[:1],indent=2)[:7000]);assert not errors
