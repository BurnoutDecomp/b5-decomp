from pathlib import Path
import sys,json,struct,hashlib,random,math
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow');sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
r=next(r for r in json.loads((out/'compile-results.json').read_text(encoding='utf-8'))['results'] if r['source'].endswith('BrnLoadingScreenRenderer.cpp'));p=Path(r['object']);o=F.Coff(p,'?Render@LoadingScreenRenderer@');b=F.Bundle([o]);errors=[]
for kind,prefix,orig in [('render','?Render@LoadingScreenRenderer@',0x823E79C8),('overlay','?RenderBlackOverlay@LoadingScreenRenderer@',0x823E8750)]:
    b.select(prefix)
    for i in range(16):
        state=bytearray(96);struct.pack_into('>5I',state,0,0x100,0x200,0x300,0x400,i%6)
        state[20]=1;state[21]=(i>>2)%2;state[22]=1;state[23]=i%2;state[48]=(i>>3)%2;state[56]=(i>>2)%2
        for off,val in [(32,45.5),(36,3.25),(40,1),(44,[0,.4,.75,1][i%4]),(52,[0,.5,1.2,3.1][i%4]),(88,.016),(92,2.5)]:struct.pack_into('>f',state,off,val)
        both=[]
        for compiled in (False,True):
            c=F.cpu(b);c.mem.wr(F.M.S.SELF,state);c.mem.add(0x82FAE000,0x1000,F.M.V.x360rd.rd(0x82FAE000,0x1000));c.mem.add(0x83010F20,0x50);c.mem.add(0x82F241FC,4);F.put(c,0x82F241FC,i%2,1)
            lib=struct.pack('>20I',*[0x10000+j*16 for j in range(20)]);c.mem.wr(0x83010F20,lib)
            for address in range(0x82C4E320,0x82C4E420,0x20):c.run(address)
            c.run(0x82C4E420);trace=[]
            def begin(x):trace.append(('begin',))
            def end(x):trace.append(('end',))
            def aspect(x):pass
            def transform(x):trace.append(('transform',struct.unpack('>16f',x.mem.rd(x.g(4)&0xffffffff,64))))
            def setstate(x):trace.append(('state',x.g(4)&0xffffffff))
            def texture(x):trace.append(('texture',x.g(4)&0xffffffff))
            def render(x):
                primitive=x.g(4)&0xffffffff;data=x.mem.rd(x.g(5)&0xffffffff,(x.g(6)&0xffffffff)*20);verts=[]
                for offset in range(0,len(data),20):
                    a=struct.unpack_from('>2f4B2f',data,offset);verts.append(a)
                trace.append(('draw',primitive,verts))
            def sin(x):x.setf(1,math.sin(x.f(1)))
            def cos(x):x.setf(1,math.cos(x.f(1)))
            c.hooks.update({0x823ABED8:begin,0x823ABFD8:end,0x823DB448:aspect,0x823AC048:transform,0x82276E48:setstate,0x82276D08:setstate,0x82276DA8:setstate,0x823A6118:setstate,0x82276EE8:texture,0x823C7030:render,0x82C095D0:sin,0x82C096A0:cos})
            for at,name in b.unresolved.items():
                if name.startswith('?mgStateLibrary@'):c.mem.wr(at,lib)
                elif name.startswith('?BeginRendering@'):c.hooks[at]=begin
                elif name.startswith('?EndRendering@'):c.hooks[at]=end
                elif name.startswith('?TransformByAspectRatio@'):c.hooks[at]=aspect
                elif name.startswith('?SetTransform@'):c.hooks[at]=transform
                elif name.startswith('?SetState@'):c.hooks[at]=setstate
                elif name.startswith('?SetTexture@'):c.hooks[at]=texture
                elif name.startswith('?Render@'):c.hooks[at]=render
                elif name in ['sin','sinf']:c.hooks[at]=sin
                elif name in ['cos','cosf']:c.hooks[at]=cos
                elif name.startswith(('__savefpr_','__restfpr_')):c.hooks[at]=lambda x:None
            if kind=='render':F.put(c,F.address(b,'?gbUseRotatingLoadingArrow@'),i%2,1)
            c.setg(3,F.M.S.SELF);c.setg(4,F.M.S.AUX);c.run(b.entry if compiled else orig)
            both.append((c.mem.rd(F.M.S.SELF,96).hex(),trace))
        def equal(a,b):
            if isinstance(a,(list,tuple)) and isinstance(b,(list,tuple)):return len(a)==len(b) and all(equal(x,y) for x,y in zip(a,b))
            if isinstance(a,float) and isinstance(b,float):return abs(a-b)<=0.0002
            return a==b
        if not equal(both[0],both[1]):errors.append(dict(kind=kind,index=i,results=both))
result=dict(cases=32,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),scope='actual render and overlay instruction streams; original runtime UV initializer execution; shader/state/texture/draw order,transforms,colors,UVs and vertex locations; receiver ignored to isolate known foreign Im2d+4 ABI discrepancy;vertices tolerance0.0002',limits='controlled aspect and Im2d submission;no GPU pixels;actual class receiver adjustment remains a foreign blocker')
(out/'loading-render-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('errors',len(errors));print(errors[:1]);raise SystemExit(bool(errors))
