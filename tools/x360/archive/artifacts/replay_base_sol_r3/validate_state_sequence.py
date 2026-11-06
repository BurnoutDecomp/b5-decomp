from pathlib import Path
import ast,hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
source=OUT/'validate_construct.py';nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
stage=sys.argv[1];report=json.loads((OUT/stage/'compile-results.json').read_text());rows={Path(r['source']).name:r for r in report['results']if r['passed']}
objects=[PackedCoff(Path(rows['BrnReplayBaseSerialiser.cpp']['object']),'?Construct@BaseSerialiser@'),PackedCoff(Path(rows['BrnReplayRaceCarEntitySerialiser.cpp']['object']),'?Construct@RaceCarEntitySerialiser@'),PackedCoff(Path(rows['CgsStringUtils.cpp']['object']),'?StrCpy@CgsCore@@')]
bundle=PackedBundle(objects);rng=random.Random(0x5C18600);errors=[]
def target(pc):
    w=F.M.V.x360rd.u32(pc);assert w>>26==18;d=w&0x3FFFFFC
    if d&0x2000000:d-=0x4000000
    return (d if w&2 else pc+d)&0xFFFFFFFF
clear_address=target(0x82657A84);memcpy_addresses={target(0x8264C264),target(0x8264C15C)}
for index in range(128):
    base,self,static,dynamic=0x57000000,0x57000100,0x58000010,0x59000010
    object_initial=rng.randbytes(0x400);static_initial=rng.randbytes(0x1B020);dynamic_initial=rng.randbytes(0x1B020);results=[]
    mode=index%8
    for compiled in [False,True]:
        # Each select repacks actual sections. Construct fresh CPUs for a common
        # union of section images so the composed calls retain real program state.
        prefixes=['?Construct@RaceCarEntitySerialiser@','?CheckStaticLayoutCleared@RaceCarEntitySerialiser@','?Read@RaceCarEntitySerialiser@','?Write@RaceCarEntitySerialiser@','?GetStaticLayout@RaceCarEntitySerialiser@']
        selected={}
        for prefix in prefixes:
            bundle.select(prefix);selected[prefix]=(bundle.entry,dict(bundle.images),dict(bundle.unresolved))
        # Run each actual call on one CPU while replacing its linked section views.
        bundle.select(prefixes[0]);c=cpu(bundle);c.mem.add(base,len(object_initial),object_initial);c.mem.add(static-16,len(static_initial),static_initial);c.mem.add(dynamic-16,len(dynamic_initial),dynamic_initial);trace=[];returns=[]
        def copy(s):
            d,a,n=(s.g(r)&0xFFFFFFFF for r in [3,4,5]);trace.append(('memcpy',d,a,n));s.mem.wr(d,s.mem.rd(a,n));s.setg(3,d)
        def clear(s):
            d=s.g(3)&0xFFFFFFFF;trace.append(('clear',d));s.mem.wr(d,bytes([0x31+index%32])*16)
        def run(prefix,original,compare_return=False):
            if compiled:
                entry,images,unresolved=selected[prefix]
                for address,data in images.items():
                    try:c.mem.wr(address,data)
                    except Exception:c.mem.add(address,len(data),data)
                # Reloaded compiled sections may occupy different extents at the
                # same base, so resolve only this call's real external services.
                for address,name in unresolved.items():
                    if name=='memcpy':c.hooks[address]=copy
                    elif name.startswith('?Clear@RaceCarSerialiserFrame@'):c.hooks[address]=clear
                c.setg(3,self);c.run(entry)
            else:
                c.setg(3,self);c.run(original)
            if compare_return:returns.append(c.g(3)&0xFFFFFFFF)
        for address in memcpy_addresses:c.hooks[address]=copy
        c.hooks[clear_address]=clear
        run(prefixes[0],0x8264C4C0)
        run(prefixes[1],0x82657A48) # no static allocation: no clear
        F.put(c,self+0x20,static);F.put(c,self+8,dynamic)
        run(prefixes[1],0x82657A48) # live then previous, once
        run(prefixes[1],0x82657A48) # flag suppresses repeat
        F.put(c,self,mode);F.put(c,self+0x10,0xD7E0 if mode==5 else 0);F.put(c,self+0x14,0)
        run(prefixes[2],0x8264C518,True)
        run(prefixes[3],0x8264C618,True)
        run(prefixes[4],0x822A3400,True)
        results.append((c.mem.rd(base,len(object_initial)),c.mem.rd(static-16,len(static_initial)),c.mem.rd(dynamic-16,len(dynamic_initial)),trace,returns))
    if results[0]!=results[1]:errors.append({'case':index,'mode':mode,'original_trace':results[0][3],'compiled_trace':results[1][3],'original_returns':results[0][4],'compiled_returns':results[1][4]})
assert not errors,errors[:2]
result={'stage':stage,'cases':128,'mismatches':errors,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual compiled reconstructed Base and Derived providers linked for constructor/getter/lazy-clear/read/write, never redirecting BaseCtor to original code. One composed state per case, all8 modes, full object/static/dynamic buffers and16-byte guards, actual return registers for read/write/getter and ordered traces. Frame::Clear and standard memcpy are controlled services with identical raw arguments; frame-clearing internals are not certified. Construction incidental r3 excluded as void-shaped original contract. No invalid asserts, generic allocator, synchronization, hardware or full replay/game claim.'}
p=OUT/(stage+'-state-sequence.json');assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n');print('state sequences',128,'mismatches',len(errors))
