from pathlib import Path
import ast, hashlib, json, random, struct, sys
OUT=Path(__file__).resolve().parent
source=OUT/'validate_construct.py';nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage' for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
stage=sys.argv[1];negative=stage=='final-v3'
rows={Path(r['source']).name:r for r in json.loads((OUT/stage/'compile-results.json').read_text())['results'] if r['passed']}
names=[('BrnReplayBaseSerialiser.cpp','?Construct@BaseSerialiser@')]
names += [('CgsStringUtils.cpp','?StrCpy@CgsCore@@')] if negative else [('CgsStrStream.cpp','??0StrStream@')]
objects=[PackedCoff(Path(rows[n]['object']),p)for n,p in names]
bundle=PackedBundle(objects);bundle.select('?Construct@BaseSerialiser@')

def target(pc):
    w=F.M.V.x360rd.u32(pc);assert w>>26==18;d=w&0x3FFFFFC
    if d&0x2000000:d-=0x4000000
    return (d if w&2 else pc+d)&0xFFFFFFFF

def z(c,p):
    a=bytearray()
    while (v:=c.mem.rd(p+len(a),1)[0]):
        a.append(v);assert len(a)<4096
    return bytes(a)

rng=random.Random(0xC2802065);mismatches=[];cases=[]
for index in range(192):
    length=[0,1,30,31,32,33,63,127][index%8]
    null=index%16==0
    name=bytes(rng.choice(range(1,256))for _ in range(length))+b'\0'
    base,self,namebase,message=0x57000000,0x57000100,0x58000000,0x59000010
    initial=rng.randbytes(0x500);namebytes=b'\xA7'*16+name+b'\x6B'*32;msgbytes=rng.randbytes(0x820)
    results=[]
    for compiled in [False,True]:
        c=cpu(bundle);c.mem.add(base,len(initial),initial);c.mem.add(namebase,len(namebytes),namebytes);c.mem.add(message-16,len(msgbytes),msgbytes)
        c.mem.add(0x84800000,0x1000);c.mem.add(0x82F32260,8)
        F.put(c,0x82F32260,message);F.put(c,0x82F32264,2048)
        trace=[];services=[]
        def begin(s):trace.append(('begin',s.mem.rd(base,len(initial)).hex()))
        def fire(s):trace.append(('fire',z(s,s.g(3)&0xFFFFFFFF).hex(),z(s,s.g(4)&0xFFFFFFFF).decode('ascii'),s.g(5)&0xFFFFFFFF))
        def end(s):trace.append(('end',))
        def cat(s):
            d,p,n=(s.g(r)&0xFFFFFFFF for r in [3,4,5]);old=z(s,d);text=z(s,p)
            services.append(('strncat',d,text.hex(),n));s.mem.wr(d+len(old),text[:n]+b'\0');s.setg(3,d)
        def fmt(s):
            d,n,p,args=(s.g(r)&0xFFFFFFFF for r in [3,4,5,6]);f=z(s,p)
            assert f==b'%u',f
            value=struct.unpack('>Q',s.mem.rd(args,8))[0]&0xFFFFFFFF
            text=str(value).encode();services.append(('vsnprintf',n,f.decode(),value))
            if n:s.mem.wr(d,text[:n-1]+b'\0')
            s.setg(3,len(text))
        c.hooks[0x82817548]=begin;c.hooks[0x82820810]=fire;c.hooks[0x82817558]=end
        c.hooks[target(0x82817764)]=fmt;c.hooks[target(0x82815EB4)]=cat
        for at,symbol in bundle.unresolved.items():
            if symbol.startswith('?gpcMessageBuffer@'):F.put(c,at,message)
            elif symbol.startswith('?giMessageBufferSize@'):F.put(c,at,2048)
            elif symbol.startswith('?BeginAssert@'):c.hooks[at]=begin
            elif symbol.startswith('?FireAssert@'):c.hooks[at]=fire
            elif symbol.startswith('?EndAssert@'):c.hooks[at]=end
            elif symbol=='strncat':c.hooks[at]=cat
            elif symbol=='vsnprintf':c.hooks[at]=fmt
        for reg,value in zip(range(3,10),[self,index%11,index%2,0x14000,0x3D480,0 if null else namebase+16,index%2]):c.setg(reg,value)
        c.run(bundle.entry if compiled else 0x8264C280)
        results.append((c.mem.rd(base,len(initial)),c.mem.rd(namebase,len(namebytes)),c.mem.rd(message-16,len(msgbytes)),trace,services))
    failed=results[0]!=results[1]
    should_assert=not null and length>=32
    assert len(results[0][3])==(3 if should_assert else 0),(index,results[0][3])
    if should_assert:
        msg=b'String '+name[:-1]+b' is too long. Buffer size = 32, string length = '+str(length).encode()+b'\n'
        assert results[0][3][1][1]==msg.hex()
        assert results[0][3][1][2:] == ('..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h',65)
    if failed:mismatches.append({'case':index,'length':length,'null':null,'original_assert_events':len(results[0][3]),'compiled_assert_events':len(results[1][3]),'object_differing_offsets':[hex(i-(self-base))for i,(a,b)in enumerate(zip(results[0][0],results[1][0]))if a!=b]})
    assert failed==(negative and should_assert),(index,length,negative,failed)
    cases.append({'case':index,'length':length,'null':null,'asserted':should_assert})
report={'stage':stage,'negative_control':negative,'cases':len(cases),'mismatches':mismatches,'case_parameters':cases,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Complete original Base Construct versus actual compiled Base and real reconstructed StrStream provider. No Base/stream method is substituted. Begin/Fire/End assert services are controlled to return, with identical message/file/line events; standard vsnprintf (%u) and strncat are controlled byte services and compare content/count/value arguments (temporary stack addresses differ). Full poisoned object/source/message buffers with guards are compared, including native complete terminator copy after a returned length>=32 assertion. Out-of-bounds name continuation is observed only in deliberately mapped diagnostic storage, not declared safe production input. No actual assertion UI/stack collection, general format/stream validity, allocator, hardware or whole replay behavior claim.'}
path=OUT/(stage+'-name-guard-v1.json');assert not path.exists();path.write_text(json.dumps(report,indent=2)+'\n')
print('name guard',len(cases),'mismatches',len(mismatches),'negative',negative)
