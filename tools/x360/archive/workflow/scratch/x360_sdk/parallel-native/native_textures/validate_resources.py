from pathlib import Path
import sys,json,struct,hashlib,random
parent=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0,str(parent/'scratch/x360_sdk'))
import native_atomic_fixture as F
out=parent/'b5-decomp/build/x360/parallel/native_textures/final'
report=json.loads((out/'compile-results.json').read_text(encoding='utf-8'))
paths={Path(r['source']).name:Path(r['object']) for r in report['results'] if r['passed']}
objs=[F.Coff(paths['texture.cpp'],'?Initialize@Texture@'),F.Coff(paths['texture2d.cpp'],'?Initialize@Texture2D@')]
bundle=F.Bundle(objs); rng=random.Random(0x360B629D8)
exports=Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
originals={}
for addr in ['82B60B98','82B62B20','82B629D8','82B610D8','82B60D28','82B62CB8']:
    j=json.loads((exports/('0x'+addr+'.json')).read_text(encoding='utf-8'))
    originals.update({r['name']:int(r['address'],16) for r in j['xrefs_from']})
SELF=F.M.S.SELF;AUX=F.M.S.AUX;failures=[];groups=[]
def args(c,n):
    return [c.g(r)&0xffffffff for r in range(3,3+min(n,8))]+[int.from_bytes(c.mem.rd((c.g(1)&0xffffffff)+0x54+8*i,4),'big') for i in range(max(0,n-8))]
for kind,prefix,original in [('initialize','?Initialize@Texture@',0x82B629D8),('initialize2d','?Initialize@Texture2D@',0x82B62E78),('descriptor2d','?GetResourceDescriptor@Texture2D@',0x823C4CB8),('lock','?Lock@Texture@',0x82B62B20),('unlock','?Unlock@Texture@',0x82B62CB8),('protect','?Xbox2CheckPhysicalMemoryFlags@Texture@',0x82B60D28)]:
    bundle.select(prefix)
    for i in range(128):
        header=rng.randbytes(52);aux=bytearray(rng.randbytes(0x2000));newheader=rng.randbytes(52)
        fmt=[0x18280043,0x1a200154,0x1a200052,0x1a20000b][i%4]
        width=[0,1,2,3,32,65,160,512,2048][i%9];height=[0,1,2,7,64,120,512,1024][i%8]
        levels=[0,1,2,5,16][i%5];flags=i%4;ty=i%5
        struct.pack_into('>IIIII',aux,0,SELF,0,AUX+0x1000,0,0)
        pars=[ty,flags,width,height,6,levels,fmt] if kind=='initialize' else [flags,width,height,levels,fmt]
        struct.pack_into('>'+'I'*len(pars),aux,0x100,*pars)
        # Cover all native resource kinds while preserving randomized other header fields.
        h=bytearray(header);w=struct.unpack_from('>I',h,48)[0];w=(w&~0x600)|([0,1,1,3,2][ty]<<9);struct.pack_into('>I',h,48,w)
        w=struct.unpack_from('>I',h,32)[0];w=(w&~0x400)|(0x400 if ty==2 else 0);struct.pack_into('>I',h,32,w);header=bytes(h)
        results=[]
        for compiled in (False,True):
            c=F.cpu(bundle);c.mem.wr(SELF,header);c.mem.wr(AUX,aux);trace=[]
            def memset(x):
                d,v,n=args(x,3);x.mem.wr(d,bytes([v&255])*n);x.setg(3,d)
            def ctor(x):
                d=x.g(3)&0xffffffff;x.mem.wr(d,struct.pack('>II',0,1))
            def protect(x):trace.append(('protect',args(x,1)));x.setg(3,[4,0x200,0x400,0x600][i%4])
            def unset(x):trace.append(('unset',args(x,1)))
            def pagesize(x):trace.append(('pagesize',args(x,1)));x.setg(3,[4096,65536,4096,8192][i%4])
            def sdk(name):
                def call(x):
                    if name.startswith('XGSet'):
                        count={'XGSetTextureHeader':12,'XGSetCubeTextureHeader':10,'XGSetLineTextureHeader':10,'XGSetArrayTextureHeader':13,'XGSetVolumeTextureHeader':12}[name]
                        a=args(x,count)
                        textureIndex={'XGSetTextureHeader':9,'XGSetCubeTextureHeader':7,'XGSetLineTextureHeader':7,'XGSetArrayTextureHeader':10,'XGSetVolumeTextureHeader':9}[name]
                        d=a[textureIndex]
                        trace.append((name,a[:textureIndex]))
                        if d:x.mem.wr(d,newheader)
                        if a[textureIndex+1]:F.put(x,a[textureIndex+1],0x1000 if i%3 else 0x3000)
                        if a[textureIndex+2]:F.put(x,a[textureIndex+2],0x2000)
                        x.setg(3,0x3000)
                    elif name.endswith('LockRect'):
                        a=args(x,6 if ('Array' in name or 'Cube' in name) else 5);p=3 if len(a)==6 else 2
                        trace.append((name,a[:p]+a[p+1:]));x.mem.wr(a[p],struct.pack('>II',256+i*4,AUX+0x1000))
                    elif name.endswith('LockBox'):
                        a=args(x,5);trace.append((name,a[:2]+a[3:]));x.mem.wr(a[2],struct.pack('>III',256+i*4,0x8000,AUX+0x1000))
                    elif 'Unlock' in name:
                        trace.append((name,args(x,3 if ('Array' in name or 'Cube' in name) else 2)))
                return call
            hooks={'memset':memset,'XQueryMemoryProtect':protect,'XMemGetPageSize':pagesize,'renderengine::Device::UnSetResource':unset}
            for name,address in originals.items():
                if name in hooks:c.hooks[address]=hooks[name]
                elif name.startswith(('XGSet','D3D')):c.hooks[address]=sdk(name)
            for at,name in bundle.unresolved.items():
                plain=name.split('@')[0].lstrip('_')
                if plain in hooks:c.hooks[at]=hooks[plain]
                elif name.startswith('?UnSetResource@Device@'):c.hooks[at]=unset
                elif name.startswith('??0BaseResourceDescriptor@'):c.hooks[at]=ctor
                elif plain.startswith(('XGSet','D3D')):c.hooks[at]=sdk(plain)
            if kind in ['initialize','initialize2d']:
                c.setg(3,AUX);c.setg(4,AUX+0x100)
            elif kind=='descriptor2d':
                c.setg(3,AUX+0x200);c.setg(4,AUX+0x100)
            elif kind=='lock':
                for reg,val in [(3,SELF),(4,flags),(5,i%8),(6,i%6),(7,AUX+0x200)]:c.setg(reg,val)
            elif kind=='unlock':
                c.setg(3,SELF);c.setg(4,AUX+0x200)
            else:c.setg(3,SELF)
            c.run(bundle.entry if compiled else original)
            ret=c.g(3)&0xffffffff if kind.startswith('initialize') or kind=='lock' else None
            results.append((ret,c.mem.rd(SELF,52).hex(),c.mem.rd(AUX,0x2000).hex(),trace))
        if results[0]!=results[1]:failures.append(dict(kind=kind,index=i,returns=[r[0] for r in results],headers=[r[1] for r in results],traces=[r[3] for r in results],auxdifferences=[j for j,(a,b) in enumerate(zip(bytes.fromhex(results[0][2]),bytes.fromhex(results[1][2]))) if a!=b][:40]))
    groups.append(dict(function=kind,cases=128));print(kind,'128 checked',flush=True)
result=dict(groups=groups,cases=sum(g['cases'] for g in groups),failures=failures,objects=[dict(path=str(p),sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in paths.values()],scope='whole actual compiled functions and ARTIST instruction streams with controlled SDK header/lock/protection callbacks; all object and auxiliary writes, SDK argument trace and observable returns compared',limits='SDK callbacks controlled; GPU and physical allocation not executed')
(out/'resource-comparison.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
print('failures',len(failures));print(failures[:2]);raise SystemExit(bool(failures))
