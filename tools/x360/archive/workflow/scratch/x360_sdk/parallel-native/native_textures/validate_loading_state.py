from pathlib import Path
import sys,json,struct,hashlib,random
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
r=next(r for r in json.loads((out/'compile-results.json').read_text(encoding='utf-8'))['results'] if r['source'].endswith('BrnLoadingScreenRenderer.cpp'))
p=Path(r['object']);o=F.Coff(p,'?AddCommand@LoadingScreenRenderer@');b=F.Bundle([o]);rng=random.Random(0x360823A)
errors=[];groups=[]
class TextureCPU(F.AtomicCPU):
    def step(self,pc,w,nxt,depth):
        if w>>26==31 and (w>>1)&1023==457:
            rt=(w>>21)&31;ra=(w>>16)&31;rb=(w>>11)&31
            assert self.g(rb)!=0
            result=self.g(ra)//self.g(rb);self.setg(rt,result)
            if w&1:self.setcr(0,bool(result>>63),bool(result and not result>>63),result==0)
            return nxt
        return super().step(pc,w,nxt,depth)

for kind,prefix,orig in [('command','?AddCommand@LoadingScreenRenderer@',0x823AAD48),('foreground','?RenderForeground@LoadingScreenRenderer@',0x823EDE18),('background','?RenderBackground@LoadingScreenRenderer@',0x823EDEB8)]:
    b.select(prefix)
    for i in range(128):
        data=bytearray(rng.randbytes(96));data[48]=i%2;data[22]=(i>>1)%2
        old=(1<<40)+i*100000;struct.pack_into('>Q',data,24,old)
        both=[]
        for compiled in (False,True):
            c=F.cpu(b);c.__class__=TextureCPU;c.mem.wr(F.M.S.SELF,data);trace=[];timer=[old+10000+(i%9)*10000,old+10100+(i%9)*10000]
            def time(x):x.setg(3,timer.pop(0))
            def freq(x):x.setg(3,1000000)
            def tod(x):x.setf(1,float(x.g(3)))
            def render(x):trace.append(('render',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
            def overlay(x):trace.append(('overlay',x.g(3)&0xffffffff,x.g(4)&0xffffffff))
            c.hooks[0x828D75A0]=time;c.hooks[0x828D75C8]=freq;c.hooks[0x82C0C670]=tod
            c.hooks[0x823E79C8]=render;c.hooks[0x823E8750]=overlay
            for at,name in b.unresolved.items():
                if name.startswith('?GetSystemTimerBaseTime64@'):c.hooks[at]=time
                elif name.startswith('?GetSystemTimerFrequency64@'):c.hooks[at]=freq
                elif name=='__u64tod':c.hooks[at]=tod
            for fragment,hook in [('?Render@LoadingScreenRenderer@',render),('?RenderBlackOverlay@LoadingScreenRenderer@',overlay)]:
                for oi,obj in enumerate(b.objects):
                    for name,val,sec in obj.symbols.values():
                        if name.startswith(fragment) and (oi,sec) in b.bases:c.hooks[b.bases[(oi,sec)]+val]=hook
            c.setg(3,F.M.S.SELF);c.setg(4,i%10 if kind=='command' else F.M.S.AUX)
            c.run(b.entry if compiled else orig)
            both.append((c.mem.rd(F.M.S.SELF,96).hex(),trace))
        if both[0]!=both[1]:errors.append(dict(kind=kind,index=i,results=both))
    groups.append(dict(kind=kind,cases=128));print(kind,'128checked',flush=True)
result=dict(groups=groups,cases=384,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='native AddCommand and foreground/background dispatch vs raw ARTIST;64bit timer >32bit, all state bytes, quantized timestep and render-call order',limits='render implementations are call boundaries; no GPU execution')
(out/'loading-state-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('errors',len(errors));print(errors[:1]);raise SystemExit(bool(errors))
