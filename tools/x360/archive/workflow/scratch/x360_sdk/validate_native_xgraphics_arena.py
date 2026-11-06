"""Complete actual native Arena routines versus ARTIST; exact relocated Grow bytes."""
import hashlib,json,random,struct
from pathlib import Path
import native_atomic_fixture as F
ROOT=Path(__file__).resolve().parents[2]/'b5-decomp';OUT=ROOT/'build/x360/native-xgraphics-arena'
row=next(r for r in json.loads((OUT/'compile-results.json').read_text())['results'] if r['source'].endswith('XGraphicsArena.cpp'))
path=Path(row['object']);obj=F.Coff(path,'?Grow@Arena@');bundle=F.Bundle([obj])
OBJ=F.M.S.SELF;AUX=F.M.S.AUX;COMPILER=AUX+0x1000;ALLOC=AUX+0x200;BLOCK=AUX+0x8000
exports=Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
free_export=json.loads((exports/'0x82C21F88.json').read_text())
memset_addr=next(int(r['address'],16) for r in free_export['xrefs_from'] if r['name']=='memset')
rng=random.Random(360_1031_1);errors=[];groups=[]
for prefix,original,kind in [('?Grow@Arena@',0x82C26AC8,'grow'),('?Malloc@Arena@',0x82C21EC0,'malloc'),('?Free@Arena@',0x82C21F88,'free')]:
    bundle.select(prefix)
    for index in range(256):
        image=bytearray(rng.randbytes(0x100));data=bytearray(rng.randbytes(0x14000))
        cursor=AUX+0x6000+(index%33)*4;old=cursor-(index%65)*4
        maximum=cursor+[-40,0,40,200][index%4];end=cursor+[-4,0,4,200,8192][index%5]
        previous=0 if index%5==0 else AUX+0x5000
        struct.pack_into('>IIIIII',image,0,COMPILER,previous,cursor,maximum,end,old)
        image[0x28]=int(index%3!=0)
        struct.pack_into('>II',data,0x15a0,ALLOC,0x87654320)
        size=[0,1,3,4,5,12247,12248,12249,0x7ffffff0,0x80000000,0xffffffff][index%11]
        if kind=='free':argument=old if index%4 else old+16
        else:argument=size
        both=[]
        for compiled in [False,True]:
            c=F.cpu(bundle);c.mem.wr(OBJ,image);c.mem.wr(AUX,data);trace=[]
            def allocate(x):
                trace.append(('allocate',x.g(3)&0xffffffff,x.g(4)&0xffffffff,x.mem.rd(OBJ,0x2c).hex()))
                if index%7==0:
                    F.put(x,OBJ+4,AUX+0x5100);F.put(x,OBJ+0xc,AUX+0x8100)
                x.setg(3,BLOCK)
            def zero(x):
                dest,value,size=[x.g(r)&0xffffffff for r in [3,4,5]]
                assert value==0 and size<=256,(dest,value,size)
                trace.append(('memset',dest,value,size,x.mem.rd(OBJ,0x2c).hex()))
                x.mem.wr(dest,bytes(size))
                if index%7==0:
                    F.put(x,OBJ+0x14,AUX+0x6100);F.put(x,OBJ+8,AUX+0x61a0)
                    F.put(x,OBJ+0x18,0x7fffffff);F.put(x,OBJ+0x20,0xfffffff0)
                x.setg(3,dest)
            c.hooks[ALLOC]=allocate;c.hooks[memset_addr]=zero
            for at,name in bundle.unresolved.items():
                if name=='memset':c.hooks[at]=zero
            c.setg(3,OBJ);c.setg(4,argument);c.run(bundle.entry if compiled else original)
            both.append((c.g(3)&0xffffffff if kind=='malloc' else None,c.mem.rd(OBJ,0x100),c.mem.rd(AUX,0x14000),trace))
        if both[0]!=both[1]:errors.append({'kind':kind,'index':index,'return':[b[0] for b in both],'trace':[b[3] for b in both],
                                         'object_diffs':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][1],both[1][1])) if a!=b][:12],
                                         'buffer_diffs':[(i,a,b) for i,(a,b) in enumerate(zip(both[0][2],both[1][2])) if a!=b][:12]})
    groups.append({'kind':kind,'cases':256});print(kind,'256 checked',flush=True)

# Apply actual original-address CRT relocations, then compare all108 bytes.
symbol,value,si=next(s for s in obj.symbols.values() if s[0].startswith('?Grow@Arena@') and s[2]>0)
section=obj.sections[si];code=bytearray(obj.data[section[4]+value:section[4]+section[3]])
targets={'__savegprlr_29':0x82C08EEC,'__restgprlr_29':0x82C08F3C}
relocated=[]
for at,index,kind in obj.relocations(si):
    name,_,_=obj.symbols[index];assert kind==6 and name in targets,(kind,name)
    at-=value;word=struct.unpack_from('>I',code,at)[0];delta=targets[name]-(0x82C26AC8+at)
    assert -(1<<25)<=delta<(1<<25)
    struct.pack_into('>I',code,at,(word&0xfc000003)|(delta&0x03fffffc))
    relocated.append({'offset':at,'symbol':name,'address':hex(targets[name])})
native=F.M.V.x360rd.rd(0x82C26AC8,108)
byte_match={'symbol':symbol,'native_address':'0x82C26AC8','size':len(code),'relocations':relocated,
            'candidate_sha256':hashlib.sha256(code).hexdigest(),'artist_sha256':hashlib.sha256(native).hexdigest(),
            'exact_match_after_original_address_relocations':code==native}
assert len(code)==108 and code==native,byte_match
result={'cases':sum(g['cases'] for g in groups),'groups':groups,'mismatches':errors,'object':str(path),
        'object_sha256':hashlib.sha256(path.read_bytes()).hexdigest(),'grow_byte_match':byte_match,
        'scope':'whole actual compiled Grow/Malloc/Free vs ARTIST;real5A0/5A4 Compiler callback,chunk chaining,alignment,32bit request/pointer/statistic wrap,recycling/high-water/cursor and post-memset reloads;whole objects/buffers and callback order;all108 Grow bytes after real CRT relocations',
        'limits':'allocation and memset callbacks controlled,including owner-field changes;no actual shader compilation or hardware allocation/runtime;SDK Compiler layout separately verified against101 CodeView size/offset records'}
(OUT/'arena-comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(result['cases'],'ARTIST checks;',len(errors),'mismatches;Grow108 bytes exactly match after relocation')
if errors:print(json.dumps(errors[:3],indent=2))
raise SystemExit(bool(errors))
