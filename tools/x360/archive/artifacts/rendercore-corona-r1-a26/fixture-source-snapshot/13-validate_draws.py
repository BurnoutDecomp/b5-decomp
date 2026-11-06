from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F

class SunCPU(F.AtomicCPU):
    def step(self,pc,word,nxt,depth):
        if word>>26==31 and ((word>>1)&1023)==539: # srd
            source,dest,shift_reg=(word>>21)&31,(word>>16)&31,(word>>11)&31
            value=self.g(source)&0xffffffffffffffff;shift=self.g(shift_reg)&127
            result=value>>shift if shift<64 else 0
            self.setg(dest,result)
            if word&1:self.setcr(0,bool(result>>63),bool(result and not result>>63),result==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023)==794: # srad
            source,dest,shift_reg=(word>>21)&31,(word>>16)&31,(word>>11)&31
            value=self.g(source)&0xffffffffffffffff;shift=self.g(shift_reg)&127
            signed=(value&0x7fffffffffffffff)-(value&0x8000000000000000)
            result=signed>>min(shift,64)
            self.xer_ca=int(signed<0 and (shift>=64 or bool(value&((1<<shift)-1))))
            self.setg(dest,result)
            if word&1:self.setcr(0,result<0,result>0,result==0)
            return nxt
        if word>>26==31 and ((word>>1)&1023)==199: # stvewx
            source,ra,rb=(word>>21)&31,(word>>16)&31,(word>>11)&31
            address=((self.g(ra) if ra else 0)+self.g(rb))&0xffffffff
            F.put(self,address&~3,self.vr[source][(address>>2)&3])
            return nxt
        return super().step(pc,word,nxt,depth)

out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-sun-retail-917')
def object_path(folder,name):
    return Path(next(r['object'] for r in json.loads((out/folder/'compile-results.json').read_text())['results'] if Path(r['source']).name==name))
paths={'Sun':object_path('native1','BrnSunCorona.cpp'),
       'Device':object_path('native1','device.cpp'),
       'DescriptorBindingProbe':object_path('descriptor-probe','descriptor_binding_probe.cpp')}
bundle=F.Bundle([F.Coff(paths['Sun'],'?GenerateOcclusionBuffer@'),F.Coff(paths['Device'],'?BeginShaderStates@'),
                 F.Coff(paths['DescriptorBindingProbe'],'?SetVertexDescriptor@')])
SELF,AUX=F.M.S.SELF,F.M.S.AUX
SUN,TARGET,SOURCE,BACKEND_A,BACKEND_B,TX_A,TX_B=[SELF+n for n in [0x1000,0x2000,0x2400,0x3000,0x3200,0x4000,0x4100]]
GPU_A,GPU_B,VERTICES=AUX+0x10000,AUX+0x14000,AUX+0x18000
addresses={'?saDepthStencilStates@':0x8301090c,'?saRasterizerStates@':0x83010a38,
           '?saBlendStates@':0x83010f70,'?mpVertexDescriptor@Device@shadow@@':0x83010958,
           '?mbVertexProgramStateDirty@Device@shadow@@':0x83010a34,
           '__savefpr_27':0x82c0a5c4,'__restfpr_27':0x82c0a610}
original_setters={'DepthStencilState@':0x82276ad0,'RasterizerState@':0x82276b38,
                  'BlendState@':0x82276a68,'BlendMaterialState@':0x82276a68,'TextureState@':0x8227d158}
rng=random.Random(0x82400cf8);errors=[];checks=[]

# Independent signed/large-count cases for the newly needed SDK-mask opcode.
for value in [0,1,0x7fffffffffffffff,0x8000000000000000,0xffffffffffffffff]:
    for shift in [0,1,63,64,127]:
        bundle.select('?GenerateOcclusionBuffer@BrnSunCorona@')
        c=F.cpu(bundle);c.__class__=SunCPU;c.setg(7,value);c.setg(8,shift)
        c.step(0x82276908,0x7ce84634,0x8227690c,0)
        signed=(value&0x7fffffffffffffff)-(value&0x8000000000000000)
        assert c.g(8)&0xffffffffffffffff==(signed>>min(shift,64))&0xffffffffffffffff
        c.setg(8,value);c.setg(9,shift)
        c.step(0x8227690c,0x7d094c36,0x82276910,0)
        assert c.g(9)&0xffffffffffffffff==(value>>shift if shift<64 else 0)

def ptr(c,n):return c.g(n)&0xffffffff
def rebase():
    for (oi,sec),base in bundle.bases.items():
        obj=bundle.objects[oi];body=bundle.images[base]
        for at,ix,kind in obj.relocations(sec):
            name=obj.symbols[ix][0];targets=[v for p,v in addresses.items() if name.startswith(p)]
            if not targets:continue
            target=targets[0];raw=struct.unpack_from('>I',obj.data,obj.sections[sec][4]+at)[0]
            if kind in [0x10,0x11]:word=(raw&0xffff0000)|(((target+0x8000)>>16 if kind==0x10 else target)&0xffff)
            elif kind==2:word=target+raw
            elif kind==6:word=(raw&0xfc000003)|((target-(base+at))&0x03fffffc)
            else:raise AssertionError((name,kind))
            struct.pack_into('>I',body,at,word)
def link_native(c,at,target):
    c.hooks.pop(at,None)
    c.mem.wr(at,struct.pack('>4I',0x3d800000|((target+0x8000)>>16),0x398c0000|(target&0xffff),0x7d8903a6,0x4e800420))

for method,original in [('GenerateOcclusionBuffer',0x82400cf8),('RenderOccludedFlare',0x824010a8)]:
    bundle.select('?'+method+'@BrnSunCorona@');rebase()
    gp_compiled=F.address(bundle,'?gpD3DDevice@')
    begin_shader=F.address(bundle,'?BeginShaderStates@Device@renderengine@')
    for index in range(512):
        sun=bytearray(rng.randbytes(0x80));scene=rng.randbytes(0x3000)
        gpu_a=rng.randbytes(0x3000);gpu_b=rng.randbytes(0x3000);vertex_seed=rng.randbytes(0x100)
        struct.pack_into('>I',sun,0x18,AUX+0x6000)
        for offset,n in [(0x1c,0),(0x20,1),(0x28,2),(0x2c,3)]:struct.pack_into('>I',sun,offset,AUX+0x7000+n*0x100)
        handle=bytes([index%256,2,index%3,(index%8)])
        sun[0x24:0x28]=handle;sun[0x30:0x34]=handle
        for offset,value in [(0x38,0.5+index%7),(0x3c,2.0),(0x40,0.25+index%3),(0x44,0.0125*(index%5+1)),(0x48,-0.25+index/512.0),(0x4c,0.75-index/1024.0)]:struct.pack_into('>f',sun,offset,value)
        sun[0x50]=1 if index%4 else 0;sun[0x51]=1 if index%5 else 0
        width=[0,1,640,1280,0xffffffff,0x80000000,1920,2560][index%8]
        height=[1,0,480,720,0xffffffff,0x80000000,1080,1440][index%8]
        colour=struct.pack('>4f',0.125*(index%7),0.0625*(index%11),0.25*(index%5),0.0)
        white=0.5+(index%5)*0.25;brightness=-0.125+index/256.0;override=index%2
        both=[]
        for compiled in (False,True):
            c=F.cpu(bundle);c.__class__=SunCPU
            c.mem.add(0x83010000,0x2000);c.mem.add(0x83271608,4)
            c.mem.wr(SUN,sun);c.mem.wr(TARGET,scene)
            F.put(c,TARGET+0x108,BACKEND_A);F.put(c,SOURCE+0x108,BACKEND_B)
            F.put(c,SOURCE+4,width);F.put(c,SOURCE+8,height)
            F.put(c,BACKEND_A+0x2c,TX_A);F.put(c,BACKEND_B+0x8c,TX_B)
            F.put(c,TX_A+0x20,AUX+0x9000);F.put(c,TX_B+0x20,AUX+0x9100)
            c.mem.wr(GPU_A,gpu_a);c.mem.wr(GPU_B,gpu_b);c.mem.wr(VERTICES,vertex_seed)
            def set_device(value):F.put(c,0x83271608,value);F.put(c,gp_compiled,value)
            set_device(GPU_A)
            for table,count in [(0x8301090c,5),(0x83010a38,3),(0x83010f70,9)]:
                for n in range(count):F.put(c,table+n*4,AUX+0x1000+table%0x1000+n*0x100)
            for cache,table,slot in [(0x83010a28,0x8301090c,1),(0x83010a2c,0x83010a38,2),(0x83010964,0x83010f70,0 if method=='GenerateOcclusionBuffer' else 5)]:
                F.put(c,cache,int.from_bytes(c.mem.rd(table+slot*4,4),'big') if index%3==0 else 0)
            for lock in [0x83010904,0x83010905,0x83010907]:c.mem.wr(lock,bytes([1 if index%11==0 else 0]))
            F.put(c,0x8301095c,int.from_bytes(sun[0x1c if method=='GenerateOcclusionBuffer' else 0x28:0x20 if method=='GenerateOcclusionBuffer' else 0x2c],'big') if index%4==0 else 0)
            F.put(c,0x83010960,int.from_bytes(sun[0x20 if method=='GenerateOcclusionBuffer' else 0x2c:0x24 if method=='GenerateOcclusionBuffer' else 0x30],'big') if index%6==0 else 0xffffffff)
            F.put(c,0x83010958,AUX+0x6000 if index%7==0 else 0)
            tx=TX_B if method=='GenerateOcclusionBuffer' else TX_A
            if index%9==0:
                F.put(c,0x83010968,tx);F.put(c,0x830109a8,tx);F.put(c,0x830109e8,int.from_bytes(c.mem.rd(tx+0x20,4),'big'))
            c.mem.wr(ptr(c,1)-0x4000,bytes([index&255])*0x4000)
            c.setg(3,SUN);c.setg(4,TARGET);c.setg(5,SOURCE)
            if method=='RenderOccludedFlare':
                c.setf(1,white);c.setf(2,brightness)
                # Both executions receive the same retail v1/f1/f2/r7 window.
                # Poison old paired-GPR vector and r9-bool slots to catch false ABI.
                c.vr[1]=struct.unpack('>4I',colour);c.setg(7,override)
                for reg in [5,6,8,9,10]:c.setg(reg,0xd300000000000000 | (index<<8) | reg)
            trace=[]
            def context():return(c.mem.rd(SUN,0x80).hex(),c.mem.rd(0x83010900,0x180).hex(),c.mem.rd(0x83010f70,36).hex(),hashlib.sha256(c.mem.rd(GPU_A,0x3000)+c.mem.rd(GPU_B,0x3000)).hexdigest())
            def observe(pc,word):
                if pc==(begin_shader if compiled else 0x822768d0):trace.append(('shader_handle',c.mem.rd(ptr(c,3),4).hex(),context()))
            def assert_begin(cpu):trace.append(('assert_begin',context()))
            def assert_fire(cpu):trace.append(('assert_fire',F.cstring(cpu,ptr(cpu,3)),F.cstring(cpu,ptr(cpu,4)),ptr(cpu,5),context()))
            def assert_end(cpu):trace.append(('assert_end',context()))
            def begin(cpu):
                trace.append(('target_begin',ptr(cpu,3),context()))
                if index%13==1:F.put(cpu,SOURCE+0x108,BACKEND_A)
            def end(cpu):
                trace.append(('target_end',ptr(cpu,3),context()))
                if index%13==2:cpu.mem.wr(SUN+0x50,b'\0')
            def apply(cpu,kind):
                trace.append(('apply_'+kind,ptr(cpu,3),context()))
                if index%13==3 and kind=='depth':F.put(cpu,0x83010a40,AUX+0xa100)
                if index%13==4 and kind=='raster':F.put(cpu,0x83010f70+(0 if method=='GenerateOcclusionBuffer' else 20),AUX+0xa200)
            def pixel(cpu):
                trace.append(('pixel',ptr(cpu,3),ptr(cpu,4),context()))
                if index%13==5:
                    F.put(cpu,SOURCE+4,17);struct.pack_into('>f',temp:=bytearray(4),0,3.25);cpu.mem.wr(SUN+0x40,temp)
            def sampler(cpu):trace.append(('sampler',ptr(cpu,3),ptr(cpu,4),ptr(cpu,5),context()))
            def texture(cpu):
                trace.append(('texture',ptr(cpu,3),ptr(cpu,4),ptr(cpu,5),cpu.g(6)&0xffffffffffffffff,context()))
            def flush(cpu):
                trace.append(('flush',context()))
                if index%13==6:set_device(GPU_B)
            def begin_vertices(cpu):
                trace.append(('begin_vertices',ptr(cpu,3),ptr(cpu,4),ptr(cpu,5),ptr(cpu,6),context()))
                if index%13==7:set_device(GPU_B)
                if index%13==8:
                    struct.pack_into('>f',temp:=bytearray(4),0,123.0);cpu.mem.wr(SUN+0x48,temp)
                cpu.setg(3,VERTICES)
            def end_vertices(cpu):trace.append(('end_vertices',ptr(cpu,3),cpu.mem.rd(VERTICES,80).hex(),context()))
            c.instruction_observer=observe
            c.hooks.update({0x82817548:assert_begin,0x82820810:assert_fire,0x82817558:assert_end,
                            0x827e7748:begin,0x827e7758:end,0x827e8150:lambda x:apply(x,'depth'),
                            0x827e8690:lambda x:apply(x,'raster'),0x827e7d10:lambda x:apply(x,'blend'),
                            0x829437a0:pixel,0x827e8950:sampler,0x82941550:texture,0x827e7a10:flush,
                            0x82944448:begin_vertices,0x82944978:end_vertices})
            if compiled:
                for at,name in bundle.unresolved.items():
                    if name.startswith('?SetState@Device@shadow@@'):
                        target=next(v for p,v in original_setters.items() if p in name);link_native(c,at,target)
                    elif name.startswith('?SetVertexProgram@'):link_native(c,at,0x82276ba0)
                    elif name.startswith('?SetPixelProgram@'):link_native(c,at,0x82276c00)
                    elif name.startswith('?BeginAssert@'):c.hooks[at]=assert_begin
                    elif name.startswith('?FireAssert@'):c.hooks[at]=assert_fire
                    elif name.startswith('?EndAssert@'):c.hooks[at]=assert_end
                    elif name.startswith('?Begin@CgsRenderTarget@'):c.hooks[at]=begin
                    elif name.startswith('?End@CgsRenderTarget@'):c.hooks[at]=end
                    elif name.startswith('?FlushVertexProgramState@'):c.hooks[at]=flush
                    elif name=='D3DDevice_BeginVertices':c.hooks[at]=begin_vertices
                    elif name=='D3DDevice_EndVertices':c.hooks[at]=end_vertices
            c.run(bundle.entry if compiled else original)
            both.append((c.mem.rd(SUN,0x80),c.mem.rd(TARGET,0x3000),c.mem.rd(0x83010000,0x2000),
                         c.mem.rd(GPU_A,0x3000),c.mem.rd(GPU_B,0x3000),c.mem.rd(VERTICES,0x100),
                         c.mem.rd(0x83271608,4),c.mem.rd(gp_compiled,4),trace))
        if both[0]!=both[1]:
            errors.append(dict(method=method,case=index,
                sun_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][0],both[1][0])) if a!=b],
                vertex_differences=[hex(n) for n,(a,b) in enumerate(zip(both[0][5],both[1][5])) if a!=b],
                first_trace_difference=next((n for n,(a,b) in enumerate(zip(both[0][-1],both[1][-1])) if a!=b),None),
                traces=[result[-1] for result in both]))
    checks.append(dict(method=method,cases=512))

result=dict(checks=checks,errors=errors,
    objects={n:dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n,p in paths.items()},
    scope='Complete Sun GenerateOcclusionBuffer/RenderOccludedFlare against ARTIST82400CF8/824010A8 with actual compiled Device::BeginShaderStates and SDK inline constant-bank writes, actual original shadow state/program/texture cache setters reached through linkage-only thunks, and actual existing descriptor-binding method body in a bounded genuine-header native probe. All80vertex bytes, shader-bank/dirty masks, Sun/target/global bytes and service order compare.',
    limits=['Target Begin/End, vertex-program Flush, low-level state/sampler appliers and SDK pixel/texture/vertex-ring functions are controlled; no GPU or boot claim.',
            'Whole foreign Shadow/API source closure is not delivered. The immutable917 legacy BlendMaterialState and unused TextureState void-pointer-return spellings are recorded; argument register contracts are unchanged and no return is consumed. Descriptor binding is an exact-source diagnostic probe only.',
            'Flare uses identical unnormalized retail v1/f1/f2/r7 input state for original and actual compiled Sun. Old paired-GPR and r9-bool slots are poisoned. Shared canonicalMath917 remains a private held dependency; no full game ABI/link/launch approval.'])
report=out/'native1/sun-draw-retail-comparison.json'
assert not report.exists(),'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps(dict(checks=checks,errors=len(errors),first_error=errors[:1]),indent=2))
raise SystemExit(bool(errors))
