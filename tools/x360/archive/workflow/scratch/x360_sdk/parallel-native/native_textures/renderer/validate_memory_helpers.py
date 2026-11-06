from pathlib import Path
import hashlib,json,random,struct,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-memory4'
j=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
p=Path(j['results'][0]['object']);o=F.Coff(p,'?Construct@BrnRendererMemory@@');bundle=F.Bundle([o])
SELF=F.M.S.SELF;AUX=F.M.S.AUX
specs=[('CreateAntiAliasBuffer',0x823F6B40),('CreateDownSampleBuffer',0x823F6E60),
    ('CreateShadowmapBuffer',0x823F6D98),('CreateBloomBuffer',0x823F7090),
    ('CreateDepthOfFieldBuffer',0x823F7200),('CreateWorkBuffer',0x823F7148),
    ('CreateEnvmapBuffer',0x823F6C88),('CreateParticleBuffer',0x823F72B8),
    ('CreateSunCoronaBuffer',0x823F73C8),('GetShadowMapBuffer',0x823F4910)]
rng=random.Random(0x823F6B40);errors=[];groups=[]
class AssertStop(Exception):pass
for name,address in specs:
    bundle.select('?'+name+'@BrnRendererMemory@@')
    # Link the external CgsRenderTarget C++ constructor to its actual ARTIST
    # body; this dependency executes real native instructions on both sides.
    for (oi,sec),base in bundle.bases.items():
        obj=bundle.objects[oi];body=bundle.images[base]
        for at,index,kind in obj.relocations(sec):
            symbol,_,_=obj.symbols[index]
            if not symbol.startswith('??0CgsRenderTarget@@'):continue
            assert kind==6
            word=struct.unpack_from('>I',body,at)[0];delta=0x823F47A0-(base+at)
            struct.pack_into('>I',body,at,(word&0xfc000003)|(delta&0x03fffffc))
    for i in range(128):
        state=bytearray(rng.randbytes(0x70));obj_bytes=rng.randbytes(0x110)
        w=[0,1,31,80,640,1280,1920,0xffffffff][i%8];h=[0,1,15,32,480,720,1080,0xffffffff][(i//2)%8]
        struct.pack_into('>II',state,0x5c,w,h);index=[0,1,2,3,4,0xffffffff][i%6];both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.mem.wr(SELF,state);c.mem.wr(AUX+0x4000,obj_bytes);trace=[]
            def allocate(x):trace.append(('new',x.g(3)&0xffffffff));x.setg(3,AUX+0x4000)
            def construct(x):
                trace.append(('construct',x.g(3)&0xffffffff,x.g(4)&0xffffffff,
                    x.mem.rd(x.g(3)&0xffffffff,0x110).hex(),x.mem.rd(SELF,0x70).hex()))
                F.put(x,AUX+0x4108,AUX+0x6000)
            def begin_assert(x):trace.append(('assert_begin',))
            def fire_assert(x):trace.append(('assert',F.cstring(x,x.g(3)&0xffffffff),F.cstring(x,x.g(4)&0xffffffff),x.g(5)&0xffffffff))
            def end_assert(x):trace.append(('assert_end',));raise AssertStop()
            c.hooks.update({0x82C08DF0:allocate,0x827ECC28:construct,
                0x82817548:begin_assert,0x82820810:fire_assert,0x82817558:end_assert})
            for at,symbol in bundle.unresolved.items():
                if symbol.startswith('??2@'):c.hooks[at]=allocate
                elif symbol.startswith('?Construct@CgsRenderTarget@@'):c.hooks[at]=construct
                elif symbol.startswith('?BeginAssert@'):c.hooks[at]=begin_assert
                elif symbol.startswith('?FireAssert@'):c.hooks[at]=fire_assert
                elif symbol.startswith('?EndAssert@'):c.hooks[at]=end_assert
            c.setg(3,SELF);c.setg(4,index if name=='GetShadowMapBuffer' else AUX+0x1000);c.setg(5,i%2)
            stopped=False
            try:c.run(bundle.entry if compiled else address)
            except AssertStop:stopped=True
            ret=c.g(3)&0xffffffff if name=='GetShadowMapBuffer' and not stopped else None
            both.append((ret,stopped,c.mem.rd(SELF,0x70).hex(),c.mem.rd(AUX+0x4000,0x110).hex(),trace))
        if both[0]!=both[1]:errors.append(dict(function=name,index=i,parent_differences=[n for n,(a,b) in enumerate(zip(bytes.fromhex(both[0][2]),bytes.fromhex(both[1][2]))) if a!=b],object_differences=[n for n,(a,b) in enumerate(zip(bytes.fromhex(both[0][3]),bytes.fromhex(both[1][3]))) if a!=b],traces=[x[4] for x in both]))
    groups.append(dict(function=name,original=hex(address),cases=128));print(name,'128 checked;',len(errors),'total mismatches',flush=True)
result=dict(cases=1280,errors=errors,groups=groups,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),
    scope='actual whole native factory/getter functions versus ARTIST; real original CgsRenderTarget C++ constructor executes on both sides; all272target bytes,112parent bytes,new sizes,virtual Construct arguments/store timing and native assert records compared',
    limits='resource realization Construct and operator new controlled; CreateBackBuffer section-state aliasing awaits postfx recovery and is excluded; current original global plan/shadow dimensions, no physical GPU/boot')
(out/'memory-helper-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('1280 native helper cases; errors',len(errors));print([dict(function=x['function'],index=x['index'],parent=x['parent_differences'],object=x['object_differences'],traces=x['traces'] if x['function']=='GetShadowMapBuffer' else 'saved in report') for x in errors[:4]]);raise SystemExit(bool(errors))
