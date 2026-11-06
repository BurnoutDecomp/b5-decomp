from pathlib import Path
import hashlib,json,random,struct,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/renderer-memory4'
j=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
p=Path(j['results'][0]['object']);o=F.Coff(p,'?Construct@BrnRendererMemory@@');bundle=F.Bundle([o]);bundle.select('?Construct@BrnRendererMemory@@')
SELF=F.M.S.SELF;AUX=F.M.S.AUX
create_specs=[('CreateAntiAliasBuffer',0x823F6B40,0),('CreateDownSampleBuffer',0x823F6E60,4),
    ('CreateShadowmapBuffer',0x823F6D98,1),('CreateBloomBuffer',0x823F7090,6),
    ('CreateDepthOfFieldBuffer',0x823F7200,7),('CreateWorkBuffer',0x823F7148,8),
    ('CreateEnvmapBuffer',0x823F6C88,3),('CreateParticleBuffer',0x823F72B8,9),
    ('CreateSunCoronaBuffer',0x823F73C8,10),('CreateBackBuffer',0x823F6F78,5)]
blob_info=json.loads((parent/'scratch/x360_sdk/parallel-native/native_textures/renderer/native-blit-programs.json').read_text(encoding='utf-8'))
rng=random.Random(0x823FCA38);errors=[]
for i in range(128):
    state=rng.randbytes(0x70);params=bytearray(rng.randbytes(100));w=[0,1,640,1280,1920,0xffffffff][i%6];h=[0,1,480,720,1080,0xffffffff][i%6]
    struct.pack_into('>II',params,12,w,h);flag=i%2;both=[]
    for compiled in [False,True]:
        c=F.cpu(bundle);c.mem.add(0x82F2C000,0x2000);c.mem.wr(SELF,state)
        F.put(c,AUX+0x1000,AUX+0x1100);F.put(c,0x82F2C814,AUX+0x1100);F.put(c,AUX+0x1110,0x84801000)
        sp=c.g(1)&0xffffffff;c.setg(3,SELF)
        for chunk in range(7):c.setg(4+chunk,int.from_bytes(params[chunk*8:chunk*8+8],'big'))
        c.mem.wr(sp+0x58,params[56:]);F.put(c,sp+0x8c,AUX+0x1000);c.mem.wr(sp+0x97,bytes([flag]))
        trace=[];allocations=0;programs=0
        def factory(name,slot):
            def run(x):
                a=[x.g(n)&0xffffffff for n in range(3,7 if name=='CreateBackBuffer' else 6 if name=='CreateAntiAliasBuffer' else 5)]
                trace.append((name,a));F.put(x,a[0]+slot*4,AUX+0x2000+slot*0x100)
                if name=='CreateParticleBuffer' and i%3==0:
                    F.put(x,SELF+0x60,h+7);x.mem.wr(SELF+0x65,b'\x19')
            return run
        def allocate(x):
            output=x.g(3)&0xffffffff;receiver=x.g(4)&0xffffffff;descriptor=x.g(5)&0xffffffff
            trace.append(('allocate',receiver,x.mem.rd(descriptor,40).hex(),x.g(6)&0xffffffff))
            n=sum(1 for t in trace if t[0]=='allocate')
            resource=[AUX+0x4000+n*0x100,0x100+n,AUX+0x8000+n*0x100,0x200+n,0x300+n]
            x.mem.wr(output,struct.pack('>5I',*resource));x.setg(3,output)
        def program_params(x):
            parameters=x.g(4)&0xffffffff;words=struct.unpack('>10I',x.mem.rd(parameters,40));sha=hashlib.sha256(x.mem.rd(words[0],words[2])).hexdigest()
            return [words[n] for n in [1,2,4,6,8,9]],sha
        def descriptor(x):
            values,sha=program_params(x);trace.append(('program_descriptor',values,sha))
            n=sum(1 for t in trace if t[0]=='program_descriptor');x.mem.wr(x.g(3)&0xffffffff,struct.pack('>10I',80+n*4,4,0,1,128+n*16,16,0,1,0,1));x.setg(3,x.g(3))
        def initialize(x):
            values,sha=program_params(x);trace.append(('program_initialize',x.mem.rd(x.g(3)&0xffffffff,20).hex(),values,sha))
            n=sum(1 for t in trace if t[0]=='program_initialize');x.setg(3,AUX+0x4000+n*0x100);F.put(x,SELF+0x44,0x12345678)
        def texture_descriptor(x):
            trace.append(('texture_descriptor',));x.mem.wr(x.g(3)&0xffffffff,struct.pack('>10I',36,4,0,1,0,1,0,1,0,1));x.setg(3,x.g(3))
        def graphics(x):x.setg(3,0x82F2C814)
        def base_descriptor(x):x.mem.wr(x.g(3)&0xffffffff,struct.pack('>II',0,1))
        c.hooks[0x84801000]=allocate;c.hooks[0x82B634B0]=descriptor;c.hooks[0x82B62478]=initialize;c.hooks[0x82B635C8]=texture_descriptor
        for name,address,slot in create_specs:
            c.hooks[address]=factory(name,slot);c.hooks[F.address(bundle,'?'+name+'@BrnRendererMemory@@')]=factory(name,slot)
        for at,name in bundle.unresolved.items():
            if name.startswith('?GetResourceDescriptor@ProgramBuffer@'):c.hooks[at]=descriptor
            elif name.startswith('?Initialize@ProgramBuffer@'):c.hooks[at]=initialize
            elif name.startswith('?GetResourceDescriptor@TextureState@'):c.hooks[at]=texture_descriptor
            elif name.startswith('?GetGraphicsAllocator@BrnResource@@'):c.hooks[at]=graphics
            elif name.startswith('??0BaseResourceDescriptor@'):c.hooks[at]=base_descriptor
        c.run(bundle.entry if compiled else 0x823FCA38)
        both.append((c.mem.rd(SELF,0x70).hex(),trace))
        if compiled:
            observed=[t[-1] for t in trace if t[0]=='program_descriptor']
            assert observed==[x['sha256'] for x in blob_info]
    if both[0]!=both[1]:errors.append(dict(index=i,state_differences=[n for n,(a,b) in enumerate(zip(bytes.fromhex(both[0][0]),bytes.fromhex(both[1][0]))) if a!=b],traces=[x[1] for x in both]))
result=dict(cases=128,errors=errors,object=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest(),shader_payloads=blob_info,
    scope='whole actual native Memory::Construct versus ARTIST823FCA38:100-byte by-value ABI,all112receiver bytes,tenfactory argument/order calls,four exact native shader payloads,descriptor/resource/Initialize flow,five allocator lanes and global graphics allocator',
    limits='target factories and ProgramBuffer/resource allocation are controlled service boundaries; their bodies and physical GPU are not proved by this constructor test')
(out/'memory-construct-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('128 native Memory::Construct cases; errors',len(errors));print([dict(index=x['index'],state_differences=x['state_differences']) for x in errors[:3]]);raise SystemExit(bool(errors))
