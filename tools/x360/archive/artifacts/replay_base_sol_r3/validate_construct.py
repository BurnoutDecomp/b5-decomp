from pathlib import Path
import ast,hashlib,json,random,struct,sys
OUT=Path(__file__).resolve().parent
fixture=Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for n in ast.parse(fixture.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(fixture),'exec'),globals())
stage=sys.argv[1];expect_bad='baseline'in stage
report=json.loads((OUT/stage/'compile-results.json').read_text())
rows={Path(r['source']).name:r for r in report['results']if r['passed']}
objects=[PackedCoff(Path(rows['BrnReplayBaseSerialiser.cpp']['object']),'?Construct@BaseSerialiser@'),
         PackedCoff(Path(rows['BrnReplayRaceCarEntitySerialiser.cpp']['object']),'?Construct@RaceCarEntitySerialiser@'),
         PackedCoff(Path(rows['CgsStringUtils.cpp']['object']),'?StrCpy@CgsCore@@')]
bundle=PackedBundle(objects);rng=random.Random(0x8264C280);errors=[];groups=[]
for kind,prefix,original,cases in [('base','?Construct@BaseSerialiser@',0x8264C280,256),('derived','?Construct@RaceCarEntitySerialiser@',0x8264C4C0,256)]:
    bundle.select(prefix);mismatches=[]
    for index in range(cases):
        base,self,name=0x57000000,0x57000100,0x58000000
        initial=bytearray(rng.randbytes(0x400));struct.pack_into('>II',initial,self-base+0x18,0xDEADBEEF,0xA55A5AA5)
        length=index%32;text=bytes(rng.choice(range(1,256))for _ in range(length))+b'\0';namebytes=bytes([0xA7])*16+text+bytes([0x6B])*48
        nameptr=0 if index%9==0 else name+16
        params=[index%11,index%2,[0,1,0x1B000,0x7FFFFFFF][index%4],[0,1,0x1B000,0x7FFFFFFF][(index//4)%4],nameptr,index%2]
        results=[]
        for compiled in [False,True]:
            c=cpu(bundle);c.mem.add(base,len(initial),initial);c.mem.add(name,len(namebytes),namebytes)
            c.setg(3,self)
            if kind=='base':
                for reg,value in zip(range(4,10),params):c.setg(reg,value)
            c.run(bundle.entry if compiled else original)
            results.append((c.mem.rd(base,len(initial)),c.mem.rd(name,len(namebytes))))
        if results[0]!=results[1]:
            differing=[i for i,(a,b)in enumerate(zip(results[0][0],results[1][0]))if a!=b]
            mismatches.append({'case':index,'self_relative_offsets':[hex(i-(self-base))for i in differing]})
            if expect_bad:assert differing==list(range(self-base+0x18,self-base+0x20)),(kind,index,differing)
    assert len(mismatches)==(cases if expect_bad else 0),(kind,len(mismatches))
    groups.append({'kind':kind,'original':hex(original),'cases':cases,'mismatches':mismatches})
result={'stage':stage,'groups':groups,'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual compiled BaseCtor, derivedCtor and (where called) real reconstructed StrCpy provider are linked. No original Base body is substituted. Compare all object/guard and name-source bytes, null/default and lengths0..31, actual skip0/1 and ordinary ids/contexts/sizes. Original Construct is void-shaped and its incidental r3 is excluded; legacy reconstructed s32 return remains separate declaration debt. Invalid-length/assert continuation, allocation, hardware and whole replay/game behavior are not claimed.'}
p=OUT/(stage+'-constructor-comparison.json');assert not p.exists();p.write_text(json.dumps(result,indent=2)+'\n')
print([(g['kind'],g['cases'],len(g['mismatches']))for g in groups])
