from pathlib import Path
import ast,hashlib,importlib,inspect,json,random,struct,sys
OUT=Path(__file__).resolve().parent
fixture=Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(fixture.read_text(encoding='utf-8')).body:
    if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='stage'for t in node.targets):break
    nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fixture),'exec'),globals())
# The imported prefix assigns HERE only; OUT remains this artifact directory.
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
def branch_target(pc,word):
    assert word>>26==18
    disp=word&0x3FFFFFC
    if disp&0x2000000:disp-=0x4000000
    return (disp if word&2 else pc+disp)&0xFFFFFFFF
class StopBoundary(Exception):pass
reports={k:json.loads((OUT/('candidate-'+k+'-v2/compile-results.json')).read_text())for k in ['class128','class64','members128','members64']}
def row(kind,leaf):return next(r for r in reports[kind]['results']if r['source'].endswith(leaf))
world=[]
mutex=branch_target(0x827E59C4,F.M.V.x360rd.u32(0x827E59C4))
for kind in reports:
    r=row(kind,'BrnWorldModule.cpp');assert r['passed']
    obj=PackedCoff(Path(r['object']),'??0WorldModule@BrnWorld@@')
    at,idx,relkind=next(x for x in obj.relocations(obj.fn[2])if obj.symbols[x[1]][0].startswith('??0RaceCarEntityModule@BrnWorld@@'))
    assert relkind==6
    fnbase=obj.entry-obj.fn[1];word=struct.unpack_from('>I',obj.images[fnbase],at)[0]
    entry=branch_target(fnbase+at,word)
    mismatches=[];samples=[]
    for index in range(32):
        base=0x57000000+index*0x2000;self=base+0x80;results=[]
        for compiled in [False,True]:
            c=cpu(obj);c.mem.add(base,0x1000,bytes([index])*0x1000);trace=[]
            def mutex_hook(s):trace.append(('mutex',s.g(3)&0xFFFFFFFF,s.g(4)&0xFFFFFFFF,s.g(5)&0xFFFFFFFF))
            def rce_hook(s):trace.append(('racecar',s.g(3)&0xFFFFFFFF));raise StopBoundary()
            c.hooks[mutex]=mutex_hook
            for address,name in obj.unresolved.items():
                if name.startswith('??0RWMutex@Thread@EA@@'):c.hooks[address]=mutex_hook
            c.hooks[entry if compiled else 0x827E5160]=rce_hook
            c.setg(3,self)
            try:c.run(obj.entry if compiled else 0x827E5998)
            except StopBoundary:pass
            else:raise AssertionError('World constructor did not reach original/candidate RCE entry')
            results.append(trace)
        if results[0]!=results[1]:mismatches.append(index)
        if index<2:samples.append({'index':index,'original':results[0],'candidate':results[1]})
    expected_negative=kind in ['class64','members64']
    assert len(mismatches)==(32 if expected_negative else 0),(kind,mismatches)
    world.append({'kind':kind,'cases':32,'mismatches':mismatches,'expected_negative':expected_negative,'samples':samples,'object':r['object'],'object_sha256':sha(r['object'])})

# Resolve the compiled Derived::Construct external call to the actual original
# BaseSerialiser::Construct machine code, without an emulated substitute body.
code=inspect.getsource(F.Coff)
needle='bases = {sec: 0x83800000 + i*0x10000 for i, sec in enumerate(sorted(used))}'
assert code.count(needle)==1
code=code.replace(needle,'''bases = {}
        cursor = 0x83800000
        for sec in sorted(used):
            bases[sec] = cursor
            cursor += max(16, (self.sections[sec][3] + 15) & ~15)''')
code=code.replace('assert s[3] < 0x10000','assert s[3] < 0x1000000')
needle='elif crt:';assert code.count(needle)==1
code=code.replace(needle,"elif symbol.startswith('?Construct@BaseSerialiser@BrnReplays@@'):\n                    address = 0x8264C280\n                elif crt:")
ns=dict(vars(importlib.import_module(F.Coff.__module__)));exec(compile(code,'native-original-base-link','exec'),ns)
NativeBaseCoff=ns['Coff']
r=row('class128','BrnReplayRaceCarEntitySerialiser.cpp');path=Path(r['object']);rng=random.Random(0x18600128)
method_results=[]
for label,prefix,original,cases in [
    ('get_static','?GetStaticLayout@RaceCarEntitySerialiser@',0x822A3400,256),
    ('construct_with_original_base','?Construct@RaceCarEntitySerialiser@',0x8264C4C0,256),
    ('check_clear_controlled_frame','?CheckStaticLayoutCleared@RaceCarEntitySerialiser@',0x82657A48,128)]:
    obj=(NativeBaseCoff if label.startswith('construct') else PackedCoff)(path,prefix)
    errors=[]
    for index in range(cases):
        base,self=0x57000000,0x57000100;frame=0x58000000
        initial=bytearray(rng.randbytes(0x400));frames=rng.randbytes(0x1B020) if label.startswith('check') else b''
        struct.pack_into('>I',initial,self-base+0x24,[0x1B000,0x1B010,0x7FFFFFFF][index%3])
        pointer=0 if index%4==0 else (frame if label.startswith('check') else rng.getrandbits(32))
        struct.pack_into('>I',initial,self-base+0x20,pointer)
        initial[self-base+0x5C]=(index>>1)&1
        results=[]
        for compiled in [False,True]:
            c=cpu(obj);c.mem.add(base,len(initial),initial);trace=[]
            if frames:c.mem.add(frame,len(frames),frames)
            def clear(s):
                address=s.g(3)&0xFFFFFFFF;trace.append(address);F.put(s,address,0x11223344^index)
            if label.startswith('check'):
                clear_addr=branch_target(0x82657A84,F.M.V.x360rd.u32(0x82657A84));c.hooks[clear_addr]=clear
                for address,name in obj.unresolved.items():
                    if name.startswith('?Clear@RaceCarSerialiserFrame@'):c.hooks[address]=clear
            c.setg(3,self);c.run(obj.entry if compiled else original)
            returned=c.g(3)&0xFFFFFFFF if label in ['get_static','construct_with_original_base'] else None
            results.append((returned,c.mem.rd(base,len(initial)),c.mem.rd(frame,len(frames)) if frames else b'',trace))
        if results[0]!=results[1]:errors.append(index)
    assert not errors,(label,errors)
    method_results.append({'method':label,'original':hex(original),'cases':cases,'mismatches':errors,'symbol':obj.fn[0]})
result={'world_prefix':world,'methods':method_results,'provider_object':str(path),'provider_object_sha256':sha(path),
        'fixture_sha256':sha(fixture),'verifier_sha256':sha(__file__),
        'limits':'Unmodified original/candidate World constructor execution stops at its genuine RaceCar constructor entry; two RWMutex services controlled and raw receiver/argument traces compared. No vtable/store or later World containment parity claim. Derived GetStaticLayout valid signed-size inputs include null/arbitrary return pointers; whole Derived Construct links the actual original BaseSerialiser::Construct (compiled Base body is not certified). CheckStaticLayoutCleared uses actual original/candidate control flow and data, with Frame::Clear services controlled. Complete mapped data/guards and required return registers compared; volatile/dead registers excluded. Scalar integer/byte PPC software model, no floating estimates or hardware/gameplay claim. A fitting alignment remains a constrained reconstruction, not independent original attribute evidence.'}
p=OUT/'candidate-native-boundaries.json';assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({'world':[(r['kind'],len(r['mismatches']))for r in world],'method_cases':sum(r['cases']for r in method_results),'mismatches':0},indent=2))
