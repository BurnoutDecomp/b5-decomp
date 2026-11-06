from pathlib import Path
import json,hashlib,struct,random,sys
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/xcam-native-terra/final'); rows=json.loads((out/'compile-results.json').read_text())['results'];sites=[('XCamCameraConnector.cpp',0x82986270,31,1),('XCamDecoder.cpp',0x82988168,31,1),('XCamEncoder.cpp',0x82984368,31,1),('XCamVideo.cpp',0x829809ec,9,-1)];rng=random.Random(0x3602026);samples=[0,1,0xffffffff,0x7fffffff,0x80000000]+[rng.getrandbits(32) for _ in range(27)];results=[];errors=[]
def run(code,ptrreg,value,msr,failures):
 c=F.M.new_cpu();c.__class__=F.AtomicCPU;c.msr=msr;c.reservation=None;c.failures=failures;c.atomic_trace=[];c.fence_trace=[];c.setg(13,0);c.setg(ptrreg,0x40000000);c.setg(3,0x13579bdf);c.mem.add(0x40000000,0x100);F.put(c,0x40000000,value);c.mem.add(0x85000000,0x100,code+bytes.fromhex('4e800020'));trace=[]
 def observe(pc,w):
  if pc<0x8500001c:trace.append((w,c.msr,c.mem.rd(0x40000000,4).hex()))
 c.instruction_observer=observe;c.run(0x85000000);return {'word':c.mem.rd(0x40000000,4).hex(),'msr':c.msr,'r3':c.g(3),'atomic':c.atomic_trace,'trace':trace}
for name,addr,reg,delta in sites:
 row=next(x for x in rows if Path(x['source']).name==name);p=Path(row['object']);obj=p.read_bytes();words=[F.M.V.x360rd.u32(addr+i*4) for i in range(7)];original=b''.join(w.to_bytes(4,'big') for w in words);hits=[i for i in range(len(obj)) if obj.startswith(original,i)];assert len(hits)==1,(name,len(hits));native=obj[hits[0]:hits[0]+28];count=0
 for v in samples:
  for msr in (0,0x8000):
   for failures in (0,1,3):
    a=run(original,reg,v,msr,failures);b=run(native,reg,v,msr,failures);count+=1
    if a!=b or int(a['word'],16)!=((v+delta)&0xffffffff) or a['msr']!=msr or a['r3']!=0x13579bdf or len(a['atomic'])!=failures+1:errors.append([name,v,msr,failures,a,b]);break
 results.append({'source':name,'original_address':hex(addr),'original_words':[hex(w) for w in words],'object':str(p),'object_sha256':hashlib.sha256(obj).hexdigest(),'object_file_offset':hex(hits[0]),'cases':count})
result={'scope':'Actual ARTIST and native object seven-instruction atomic loops only. Fixture injects 0, 1, or 3 failed reservations, checks memory, interrupt-enable restoration, trace ordering, wrap, and preserved r3. Called methods, full enclosing function, vector ABI, hardware concurrency are excluded.','sites':results,'cases':sum(x['cases'] for x in results),'mismatches':errors};(out/'atomic-loop-comparison.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));assert not errors
