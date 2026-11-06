from pathlib import Path
import hashlib,json,random,struct,sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F

class RenderCPU(F.AtomicCPU):
    def step(self,pc,word,nxt,depth):
        if word>>26==31 and word&1 and ((word>>1)&1023)==444:
            source=(word>>21)&31;dest=(word>>16)&31;rb=(word>>11)&31
            value=(self.g(source)|self.g(rb))&0xffffffffffffffff
            self.setg(dest,value);self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023) in [27,539]:
            xo=(word>>1)&1023;source=(word>>21)&31;dest=(word>>16)&31;rb=(word>>11)&31
            shift=self.g(rb)&127
            value=0 if shift>=64 else ((self.g(source)<<shift) if xo==27 else (self.g(source)>>shift))
            self.setg(dest,value)
            if word&1:
                value&=0xffffffffffffffff;self.setcr(0,bool(value>>63),bool(value and not value>>63),value==0)
            return nxt
        return super().step(pc,word,nxt,depth)

out=parent/'b5-decomp/build/x360/parallel/native_textures/rendercore3'
report=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
paths={Path(r['source']).name:Path(r['object']) for r in report['results'] if r['passed']}
objects=[F.Coff(paths[n],p) for n,p in [
    ('vertexbuffer.cpp','?Initialize@VertexBuffer@'),('pixelbuffer.cpp','?Initialize@PixelBuffer@'),
    ('meshhelper.cpp','?Initialize@MeshHelper@'),('vertexbufferhelper.cpp','?CalculateStride@'),
    ('vertexformat.cpp','?VertexFormatGetStride@'),('device.cpp','?UnSetResource@Device@')]]
bundle=F.Bundle(objects)
SELF=F.M.S.SELF;AUX=F.M.S.AUX
exports=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')

# Bind native data relocations to the original globals, like a linker placing
# these declarations at their original addresses. No executable instruction is
# replaced: only the COFF's genuine external/static data relocation operands.
globals_map={'gpD3DDevice':0x83271608,'gOriginalColorBuffer':0x83271100,
             'gOriginalDepthStencilBuffer':0x83271140,'guTilingEnabled':0x8327161C,
             'nextEDRAM':0x832716C0,'nextHierarchicalZ':0x832716C4}

def select(prefix):
    bundle.select(prefix)
    for (oi,sec),base in bundle.bases.items():
        obj=bundle.objects[oi];body=bundle.images[base]
        for at,index,kind in obj.relocations(sec):
            name,_,_=obj.symbols[index]
            address=next((a for n,a in globals_map.items() if n in name),None)
            if address is None:continue
            word=struct.unpack_from('>I',body,at)[0]
            if kind in [0x10,0x11]:
                imm=(address+0x8000)>>16 if kind==0x10 else address
                word=(word&0xffff0000)|(imm&0xffff)
            elif kind==2:word=address
            else:raise AssertionError(('data relocation kind',name,kind))
            struct.pack_into('>I',body,at,word)

specs=[
 ('vb_initialize','?Initialize@VertexBuffer@',0x82B62ED8),
 ('vb_descriptor','?GetResourceDescriptor@VertexBuffer@',0x82B63778),
 ('vb_parameters','?GetParameters@VertexBuffer@',0x82B61130),
 ('vb_protect','?Xbox2CheckPhysicalMemoryFlags@VertexBuffer@',0x82B61148),
 ('vb_unset','?Xbox2UnSet@VertexBuffer@',0x82B63058),
 ('vb_lock','?Lock@VertexBuffer@',0x8227D0A0),
 ('vb_destruct','?Destruct@VertexBuffer@',0x82B62F70),
 ('vb_release','?Release@VertexBuffer@',0x82B62FF8),
 ('helper_stride','?CalculateStride@VertexBufferHelper@',0x82B63E08),
 ('helper_size','?CalculateBufferSize@Parameters@VertexBufferHelper@',0x823F8BF0),
 ('helper_lock','?Lock@VertexBufferHelper@',0x82B63FA8),
 ('helper_unlock','?Unlock@VertexBufferHelper@',0x82B63FF0),
 ('mesh_initialize','?Initialize@MeshHelper@',0x82B63EB8),
 ('mesh_descriptor','?GetResourceDescriptor@MeshHelper@',0x82B64070),
 ('mesh_dispatch','??$Dispatch@VDevice@renderengine@@@MeshHelper@',0x8227B530),
 ('mesh_release','?Release@MeshHelper@',0x82AD5078),
 ('pixel_initialize','?Initialize@PixelBuffer@',0x82B621A8),
 ('pixel_edram','?Xbox2SetBaseEDRAM@PixelBuffer@',0x82B60870),
 ('pixel_hiz','?Xbox2SetBaseHierarchicalZ@PixelBuffer@',0x82B60898),
 ('pixel_resolve','?Xbox2ResolveTo@PixelBuffer@',0x82B62300)]

original_calls={}
for addr in [a for _,_,a in specs]+[0x82B61FE0]:
    p=exports/f'0x{addr:08X}.json'
    if p.exists():
        d=json.loads(p.read_text(encoding='utf-8'))
        for x in d['xrefs_from']:original_calls[x['name']]=int(x['address'],16)
sys.path.insert(0,str(parent/'tools/re'))
import x360rd
unlock_word=x360rd.u32(0x82B63FF0)
assert unlock_word>>26==18
unlock_delta=unlock_word&0x03fffffc
if unlock_delta&0x02000000:unlock_delta-=0x04000000
original_calls['D3DVertexBuffer_Unlock']=0x82B63FF0+unlock_delta

def args(c,n):
    return [c.g(r)&0xffffffff for r in range(3,3+min(n,8))]+[
        int.from_bytes(c.mem.rd((c.g(1)&0xffffffff)+0x54+8*i,4),'big') for i in range(max(0,n-8))]

def setargs(c,values):
    for i,v in enumerate(values):
        if i<8:c.setg(3+i,v)
        else:F.put(c,(c.g(1)&0xffffffff)+0x54+8*(i-8),v)

rng=random.Random(0xB5B62ED8)
failures=[];groups=[]
for kind,prefix,original in specs:
    select(prefix)
    for i in range(128):
        header=bytearray(rng.randbytes(0x400));aux=bytearray(rng.randbytes(0x2000))
        base=[0,3,AUX+0x1000,(AUX+0x1000)|3,0xfffffffc][i%5]
        common=[0,1,0x100001,0x200001,0x80100001][i%5]
        size=[0,1,16,4096,0x7fffffff,0xffffffff][i%6]
        typ=[0,1,2,0xffffffff][i%4]
        struct.pack_into('>I',header,0,common);struct.pack_into('>I',header,8,0 if i%4==0 else i+1)
        struct.pack_into('>III',header,24,base,rng.getrandbits(32),size)
        struct.pack_into('>I',header,36,i+0x100)
        struct.pack_into('>I',header,48,typ)
        struct.pack_into('>5I',aux,0,SELF,0,AUX+0x1000,0,0)
        struct.pack_into('>II',aux,0x100,rng.getrandbits(32),size)
        formats=[2761657,2892709,1712218,1712291,1713030]
        nfmt=i%16
        struct.pack_into('>III',aux,0x400,0,0,[0,1,3,0xffffffff][i%4])
        for j in range(16):struct.pack_into('>I',aux,0x40C+4*j,formats[(i+j)%len(formats)] if j<nfmt else 0xffffffff)
        struct.pack_into('>II',aux,0x44C,rng.getrandbits(32),[0,4,20,0x80000000,0xffffffff][i%5])
        ni=i%2;nv=i%17
        struct.pack_into('>I',aux,0x500,AUX+0x700 if ni else 0)
        for j in range(17):struct.pack_into('>I',aux,0x504+4*j,AUX+0x800+j*0x20 if j<nv else 0)
        if kind=='mesh_dispatch':
            ni=i%3;nv=i%17;struct.pack_into('>II',header,0,ni,nv)
            for j in range(ni+nv):struct.pack_into('>I',header,8+4*j,AUX+0x700+j*0x20)
        if kind.startswith('pixel'):
            width=[0,1,31,32,128,4096,0xffffffff][i%7]
            height=[0,1,15,16,256,8192,0xffffffff][i%7]
            multisample=[0,1,2,52,0xffffffff][i%5]
            fmt=[0x18280043,0x18280015,0x18280020,0x18280025][(i//2)%4]
            struct.pack_into('>7I',aux,0x100,typ,(i//4)%2,width,height,fmt,multisample,rng.getrandbits(32))
        edram=[0,1,0x7ff,0x800,0xffffffff][i%5]
        hiz=[0,1,0xe0f,0xe10,0xffffffff][i%5]
        initial_bindings=[0]*16
        if i%4:
            initial_bindings[i%16]=SELF
            if i%3==0:initial_bindings[(i+1)%16]=SELF
            initial_bindings[(i+8)%16]=AUX+0x800
        fresh_vertex=rng.randbytes(32);fresh_surface=rng.randbytes(48)
        results=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.__class__=RenderCPU;c.mem.add(0x83271000,0x1000)
            c.mem.wr(SELF,header);c.mem.wr(AUX,aux)
            F.put(c,0x83271608,0x90000000);F.put(c,0x832716C0,edram);F.put(c,0x832716C4,hiz)
            F.put(c,0x8327161C,(i//4)%2)
            this=SELF
            if kind=='pixel_resolve' and i%3==0:
                this=0x83271140 if typ==1 else 0x83271100;c.mem.wr(this,header[:52])
            bindings=list(initial_bindings);trace=[]
            def memset(x):
                d,v,n=args(x,3);x.mem.wr(d,bytes([v&255])*n);x.setg(3,d)
            def descriptor_ctor(x):x.mem.wr(x.g(3)&0xffffffff,struct.pack('>II',0,1))
            def isset(x):
                a=args(x,2);trace.append(('is_set',a));x.setg(3,int(a[0] in bindings or i%9==0))
            def getstream(x):
                dev,stream,offset,stride=args(x,4);trace.append(('get_stream',dev,stream))
                F.put(x,offset,0);F.put(x,stride,0);x.setg(3,bindings[stream])
            def release(x):
                a=args(x,1)[0];trace.append(('release',a))
                if a==SELF:
                    value=int.from_bytes(x.mem.rd(a+4,4),'big');F.put(x,a+4,value-1)
                if i%11==0:F.put(x,0x83271608,0x90001000)
                x.setg(3,3)
            def setstream(x):
                a=args(x,5);trace.append(('set_stream',a,x.g(8)&0xffffffffffffffff));bindings[a[1]]=a[2]
            def setindices(x):trace.append(('set_indices',args(x,2)))
            def pagesize(x):trace.append(('page_size',args(x,1)));x.setg(3,[0,4096,4097,65536][(i//4)%4])
            def protect(x):trace.append(('protect',args(x,1)));x.setg(3,[0,4,0x200,0x400,0x600][(i//5)%5])
            def lock(x):trace.append(('lock',args(x,4)));x.setg(3,0 if i%13==0 else AUX+0x1000)
            def unlock(x):trace.append(('unlock',args(x,1)))
            def block(x):trace.append(('block',args(x,1)));F.put(x,SELF+32,size^i)
            def invalidate(x):trace.append(('invalidate',args(x,4)))
            def set_vertex(x):
                a=args(x,5);trace.append(('set_vertex',a,x.mem.rd(a[4],40).hex()))
                x.mem.wr(a[4],fresh_vertex);F.put(x,a[4]+24,base)
            def surface_size(x):
                trace.append(('surface_size',args(x,4)))
                if i%9==0:F.put(x,0x832716C0,0x7f0)
                x.setg(3,[0,1,64,0x800,0x801,0xffffffff][i%6])
            def set_surface(x):
                a=args(x,7);trace.append(('set_surface',a[:4],x.mem.rd(a[4],12).hex(),a[5],x.mem.rd(a[5],52).hex()))
                x.mem.wr(a[5],fresh_surface);F.put(x,a[6],0x1234)
            def set_depth(x):trace.append(('set_depth',args(x,2)))
            def set_target(x):trace.append(('set_target',args(x,3)))
            def width(x):trace.append(('width',args(x,1)));x.setg(3,[0,1,32,160,4096,0xffffffff][i%6])
            def height(x):trace.append(('height',args(x,1)));x.setg(3,[0,1,16,120,8192,0xffffffff][i%6])
            def end_tiling(x):
                a=args(x,8);trace.append(('end_tiling',a[:5],struct.pack('>f',x.f(1)).hex(),a[6:8]));x.setg(3,0)
            def resolve(x):
                a=args(x,11);trace.append(('resolve',a[:2],x.mem.rd(a[2],16).hex(),a[3:8],struct.pack('>f',x.f(1)).hex(),a[9:11]))
            hooks={'memset':memset,'D3DResource_IsSet':isset,'D3DDevice_GetStreamSource':getstream,
                   'D3DResource_Release':release,'D3DDevice_SetStreamSource':setstream,'D3DDevice_SetIndices':setindices,
                   'XMemGetPageSize':pagesize,'XQueryMemoryProtect':protect,'D3DVertexBuffer_Lock':lock,
                   'D3DVertexBuffer_Unlock':unlock,'D3DResource_BlockUntilNotBusy':block,
                   'D3DDevice_InvalidateGpuCache':invalidate,'XGSetVertexBufferHeader':set_vertex,
                   'XGSetSurfaceHeader':set_surface,
                   'D3DDevice_SetDepthStencilSurface':set_depth,'D3DDevice_SetRenderTarget':set_target,
                   'renderengine::Texture::GetWidth':width,'renderengine::Texture::GetHeight':height,
                   'D3DDevice_EndTiling':end_tiling,'D3DDevice_Resolve':resolve}
            for name,address in original_calls.items():
                if name in hooks:c.hooks[address]=hooks[name]
            for at,name in bundle.unresolved.items():
                plain=name.split('@')[0].lstrip('_')
                if plain in hooks:c.hooks[at]=hooks[plain]
                elif name.startswith('??0BaseResourceDescriptor@'):c.hooks[at]=descriptor_ctor
                elif name.startswith('?GetWidth@Texture@'):c.hooks[at]=width
                elif name.startswith('?GetHeight@Texture@'):c.hooks[at]=height
            if kind in ['vb_initialize','pixel_initialize']:setargs(c,[AUX,AUX+0x100])
            elif kind=='vb_descriptor':setargs(c,[AUX+0x200,AUX+0x100])
            elif kind=='vb_parameters':setargs(c,[SELF,AUX+0x100])
            elif kind=='vb_lock':setargs(c,[SELF,i%16,AUX+0x200,rng.randrange(16) if False else (i*0x1234567)&0xffffffff,0 if i%2 else size])
            elif kind=='helper_stride':setargs(c,[AUX+0x40C,nfmt])
            elif kind=='helper_size':setargs(c,[AUX+0x400])
            elif kind=='helper_lock':setargs(c,[SELF,AUX+0x200,AUX+0x400,i%16,i,i+1 if i%2 else 0])
            elif kind=='helper_unlock':setargs(c,[SELF,AUX+0x200,AUX+0x400])
            elif kind=='mesh_initialize':setargs(c,[AUX,AUX+0x500])
            elif kind=='mesh_descriptor':setargs(c,[AUX+0x200,AUX+0x500])
            elif kind=='mesh_dispatch':setargs(c,[SELF,AUX+0x1800])
            elif kind in ['pixel_edram','pixel_hiz']:setargs(c,[SELF,[0,1,0x7fff,0x8000,0xffff,0xffffffff][i%6]])
            elif kind=='pixel_resolve':
                setargs(c,[this,AUX+0x1000,i%16,(i*31)&0xffffffff,(i*17)&0xffffffff,i%5,i%6,AUX+0x1f00,0,0x12340000+i])
                c.setf(1,[0.,1.,-1.,0.125][i%4])
            else:setargs(c,[SELF])
            try:c.run(bundle.entry if compiled else original)
            except Exception as error:
                raise RuntimeError((kind,i,compiled,hex(getattr(c,'last_pc',0)),error)) from error
            ret=c.g(3)&0xffffffff if kind in ['vb_initialize','pixel_initialize','vb_lock','helper_lock','helper_stride','mesh_initialize'] else None
            results.append((ret,c.mem.rd(SELF,0x400).hex(),c.mem.rd(AUX,0x2000).hex(),
                            c.mem.rd(0x83271000,0x1000).hex(),trace,bindings))
        if results[0]!=results[1]:
            failures.append(dict(function=kind,index=i,returns=[r[0] for r in results],traces=[r[4] for r in results],
                self_differences=[j for j,(a,b) in enumerate(zip(bytes.fromhex(results[0][1]),bytes.fromhex(results[1][1]))) if a!=b][:24],
                aux_differences=[j for j,(a,b) in enumerate(zip(bytes.fromhex(results[0][2]),bytes.fromhex(results[1][2]))) if a!=b][:24],
                global_differences=[j for j,(a,b) in enumerate(zip(bytes.fromhex(results[0][3]),bytes.fromhex(results[1][3]))) if a!=b][:24]))
    groups.append(dict(function=kind,original=hex(original),symbol=bundle.symbol,cases=128))
    print(kind,'128 checked;',len(failures),'total mismatches',flush=True)
    if failures and failures[-1]['function']==kind:print(json.dumps(next(f for f in failures if f['function']==kind),indent=2),flush=True)

result=dict(cases=sum(g['cases'] for g in groups),groups=groups,failures=failures,
            objects=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths.values()],
            data_relocation_bindings={n:hex(a) for n,a in globals_map.items()},
            scope='whole actual native COFF functions versus ARTIST raw instructions; matching object/parameter/global memory, defined returns, full SDK argument sequence and stream-binding state; game helper/stride/unset calls execute real bodies on both sides',
            limits='controlled SDK mapping/protection/surface/GPU services, bounded provided memory; no physical GPU, full link, boot or byte-match claim')
(out/'dataflow-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('cases',result['cases'],'mismatches',len(failures));print(json.dumps(failures[:3],indent=2))
raise SystemExit(bool(failures))
