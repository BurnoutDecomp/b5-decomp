from pathlib import Path
import sys,json,struct,hashlib,random
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
r=next(r for r in json.loads((out/'compile-results.json').read_text(encoding='utf-8'))['results'] if r['source'].endswith('BrnLoadingScreenRenderer.cpp'));p=Path(r['object'])
o=F.Coff(p,'?SetupLoadingScreenTexture@');b=F.Bundle([o]);b.select('?SetupLoadingScreenTexture@')
rng=random.Random(0x823C6B08);errors=[]
SELF=F.M.S.SELF;AUX=F.M.S.AUX;TEX=SELF+0x200;PIXELS=AUX+0x1000;SRC=AUX+0x400
for i in range(128):
    image=rng.randbytes(0x400);data=rng.randbytes(0x2000);width=[0,1,63.25,63.5,64,160.1,512,2048][i%8];height=[0,1,64,119.5,120,512,1024][i%7];size=[0,1,4,15,16,32,127,1024][i%8];both=[]
    for compiled in [False,True]:
        c=F.cpu(b);c.mem.wr(SELF,image);c.mem.wr(AUX,data);trace=[]
        def desc(x):
            output=x.g(3)&0xffffffff;params=x.g(4)&0xffffffff;trace.append(('descriptor',c.mem.rd(params,20).hex()))
            c.mem.wr(output,struct.pack('>10I',52,4,0,1,0x4000,4096,0,1,0,1));x.setg(3,output)
        def allocator(x):x.setg(3,0x82F2C870)
        def allocate(x):
            output=x.g(3)&0xffffffff;trace.append(('allocate',x.g(4)&0xffffffff,c.mem.rd(x.g(5)&0xffffffff,40).hex(),x.g(6)&0xffffffff))
            c.mem.wr(output,struct.pack('>5I',TEX,0,PIXELS,0,0));x.setg(3,output)
        def initialize(x):
            trace.append(('initialize',c.mem.rd(x.g(3)&0xffffffff,20).hex(),c.mem.rd(x.g(4)&0xffffffff,20).hex()));x.setg(3,TEX)
        def lock(x):
            trace.append(('lock',[x.g(j)&0xffffffff for j in range(3,7)]));c.mem.wr(x.g(7)&0xffffffff,struct.pack('>IIIHHHBBII',TEX,PIXELS,256,64,64,1,0,0,0,0));x.setg(3,1)
        def copy(x):
            dst,src,n=[x.g(j)&0xffffffff for j in range(3,6)];trace.append(('copy',dst,src,n));c.mem.wr(dst,c.mem.rd(src,n));x.setg(3,dst)
        def unlock(x):trace.append(('unlock',x.g(3)&0xffffffff,c.mem.rd(x.g(4)&0xffffffff,28).hex()))
        hooks={0x823C4CB8:desc,0x826651C0:allocate,0x82B62E78:initialize,0x82B62B20:lock,0x82926BA0:copy,0x82B62CB8:unlock}
        c.hooks.update(hooks)
        for at,name in b.unresolved.items():
            if name.startswith('?GetResourceDescriptor@Texture2D@'):c.hooks[at]=desc
            elif name.startswith('?GetEmbeddedTextureAllocator@'):c.hooks[at]=allocator
            elif name.startswith('?Initialize@Texture2D@'):c.hooks[at]=initialize
            elif name.startswith('?Lock@Texture@'):c.hooks[at]=lock
            elif name.startswith('?Unlock@Texture@'):c.hooks[at]=unlock
            elif name.startswith('XMemCpy'):c.hooks[at]=copy
        c.setg(3,SELF);c.setf(1,width);c.setf(2,height);c.setg(6,SRC);c.setg(7,size)
        c.run(b.entry if compiled else 0x823C6B08)
        both.append((c.g(3)&0xffffffff,c.mem.rd(SELF,0x400).hex(),c.mem.rd(AUX,0x2000).hex(),trace))
    if both[0]!=both[1]:errors.append(dict(index=i,trace=[r[3] for r in both],returns=[r[0] for r in both]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='actual native SetupLoadingScreenTexture vs ARTIST: float rounding,DXT5 format,all five allocation lanes,embedded allocator receiver,lock/copy/unlock order,byte count and full source/destination memory',limits='texture factory/allocator/lock services controlled; no physical GPU or linked assets')
(out/'loading-setup-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('128cases, errors',len(errors));print(errors[:2]);raise SystemExit(bool(errors))
