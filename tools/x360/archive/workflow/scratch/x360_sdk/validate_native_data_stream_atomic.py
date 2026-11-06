import os
"""Native data-stream CAS publication/fences compared with complete ARTIST functions."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp'
OUT=ROOT/'build/x360/native-frame-data-copy'
REPORT=Path(os.environ.get('NATIVE_FRAME_REPORT',str(OUT/'compile-results.json')))
row=next(r for r in json.loads(REPORT.read_text())['results'] if r['source'].endswith('CgsDataStreamResultReader.cpp'))
path=Path(row['object']);bundle=F.Bundle([F.Coff(path,'?Begin@DataStreamResultReader@')])
OBJ=F.M.S.SELF;AUX=F.M.S.AUX
rng=random.Random(360_1030_8)
identity=json.loads((ROOT.parent/'progress/identity.json').read_text())
errors=[];groups=[]
for prefix,original,kind in [('?Construct@DataStreamResultReader@',0x8286A0D0,'construct'),
                            ('?Begin@DataStreamResultReader@',0x828681D8,'begin'),
                            ('?End@DataStreamResultReader@',0x828682D8,'end'),
                            ('?ReadResult@DataStreamResultReader@',0x82868418,'read')]:
    bundle.select(prefix)
    for index in range(128):
        image=bytearray(rng.randbytes(0x100));data=rng.randbytes(0x4000)
        status=rng.getrandbits(64)&~(15<<48)
        struct.pack_into('>Q',image,0,status)
        stride=([0,128,256,512] if kind=="construct" else [-128,0,128,256])[index%4]
        struct.pack_into('>Iii',image,8,AUX+0x2000,4096,stride)
        struct.pack_into('>I',image,0x18,AUX+0x3000)
        image[0x20]=int(kind=='end')
        struct.pack_into('>ii',image,0x24,index%8,(index//8)%8)
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.failures=index%3;c.mem.wr(OBJ,image);c.mem.wr(AUX,data);trace=[]
            if kind=='begin' and index%4==0:c.interfere=(OBJ,status^0x123456789ABCDEF0)
            def copy(x):
                dest,source,count=[x.g(i)&0xFFFFFFFF for i in [3,4,5]]
                trace.append(('copy',dest,source,count));x.mem.wr(dest,x.mem.rd(source,count));x.setg(3,dest)
            for name in ['memcpy','XMemCpy']:
                for old in identity.get(name,{}).get('x360_addrs',[]):c.hooks[int(old,16)]=copy
            for at,name in bundle.unresolved.items():
                if name in ['memcpy','XMemCpy']:c.hooks[at]=copy
            c.setg(3,OBJ);c.setg(4,AUX)
            if kind=='construct':
                c.setg(4,AUX+0x2000);c.setg(5,4096);c.setg(6,stride);c.setg(7,AUX+0x3000);c.setg(8,2048)
            try:c.run(bundle.entry if compiled else original)
            except Exception:
                print("fixture context",kind,index,compiled,hex(c.last_pc),[(r,hex(c.g(r))) for r in [3,4,5,6,18,19,20,21,22,11]],flush=True)
                raise
            both.append((c.g(3)&0xFFFFFFFF if kind=='read' else None,c.mem.rd(OBJ,0x100),c.mem.rd(AUX,0x4000),trace,c.msr,c.atomic_trace,c.fence_trace))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[o[0] for o in both],
                                         'trace':[o[3] for o in both],'atomic':[o[5] for o in both],
                                         'fences':[o[6] for o in both],
                                         'bytes':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12]})
    groups.append({'kind':kind,'cases':128});print(kind,'128 checked',flush=True)
report={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,
        'object':str(path),'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
        'scope':'complete actual native Construct/Begin/End/ReadResult vs ARTIST; full object/buffers,return,copy arguments,MSR,all32/64 conditional stores andmemory fences at exact object-state boundaries; valid aligned buffers,valid nonnegative construction strides;positive/zero/negative read strides,producer changing64bit status beforeCAS and failed reservations',
        'limits':'copy service controlled; assertion paths and physical hardware concurrency not exercised'}
(OUT/'data-stream-comparison.json').write_text(json.dumps(report,indent=2)+'\n')
print(report['cases'],'checks;',len(errors),'mismatches')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
