from pathlib import Path
import sys,json,hashlib,random
P=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(P/'scratch/x360_sdk'));import native_atomic_fixture as F
D=Path('D:/Reverse/BP-X360-Experimental-Artifacts/gui-event-provider-final');j=json.loads((D/'compile-results.json').read_text());obj=Path(j['results'][0]['object']);rng=random.Random(0x823b1cc8);cases=0
for name,orig in [('?Construct@GuiOverlayRequest@BrnGui@@QAAXPBD@Z',0x823b1cc8),('?AddMessageParam@GuiOverlayRequest@BrnGui@@',0x82472a18),('?GetMessageParam@GuiOverlayRequest@BrnGui@@',0x824eb948),('?GetButton1Param@GuiOverlayRequest@BrnGui@@',0x824eba78),('?GetButton2Param@GuiOverlayRequest@BrnGui@@',0x824ebb38)]:
 o=F.Coff(obj,name);b=F.Bundle([o]);b.select(name)
 for i in range(64):
  initial=bytearray(rng.getrandbits(8) for _ in range(288));initial[0x118:0x11c]=(1).to_bytes(4,'big');initial[0x11c]=initial[0x11d]=1
  for k in [0x0c,0x50,0x94,0xd8]:initial[k:k+64]=bytes([65+(i%20)])*63+b'\0'
  text=bytes([97+(i%20)])*min(i,63)+b'\0';outs=[]
  for native in (False,True):
   c=F.cpu(b);base=F.M.S.SELF;arg=F.M.S.AUX;c.mem.wr(base,initial);c.mem.wr(arg,text+b'\0'*80)
   def hashfn(x):x.setg(3,0x1122334455667788)
   def sprint(x):
    dst=x.g(3)&0xffffffff;cap=x.g(4);textptr=x.g(6)&0xffffffff;raw=bytearray()
    while len(raw)<cap-1:
     ch=x.mem.rd(textptr+len(raw),1)[0]
     if ch==0:break
     raw.append(ch)
    x.mem.wr(dst,bytes(raw)+b'\0');x.setg(3,len(raw))
   c.hooks[0x82815a20]=hashfn;c.hooks[0x821f14f8]=sprint
   for a,n in b.unresolved.items():
    if 'CgsIDCompress' in n:c.hooks[a]=hashfn
    elif 'SPrintf' in n:c.hooks[a]=sprint
   c.setg(3,base)
   if 'Construct' in name:c.setg(4,arg)
   elif 'AddMessage' in name:c.setg(4,2);c.setg(5,arg)
   elif 'GetMessage' in name:c.setg(4,arg);c.setg(5,0)
   else:c.setg(4,arg)
   try:c.run(b.entry if native else orig)
   except Exception:print('fail',name,i,native,hex(c.last_pc));raise
   outs.append((c.mem.rd(base,288),c.mem.rd(arg,80)))
  assert outs[0]==outs[1],(name,i,outs);cases+=1
out={'passed':True,'cases':cases,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest(),'scope':'Five actual whole-provider object methods vs ARTIST valid-input methods, randomized full 288-byte request and output, 64 cases each including text lengths 0..63. Hash and SPrintf services controlled; no assertion-failure or full runtime claim.'};(D/'overlay-method-comparison.json').write_text(json.dumps(out,indent=2)+'\n');print(out)

