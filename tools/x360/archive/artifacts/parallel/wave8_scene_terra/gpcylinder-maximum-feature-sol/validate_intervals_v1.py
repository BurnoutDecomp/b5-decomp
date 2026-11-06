from pathlib import Path
import ast,hashlib,json,random,struct,sys
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent;stage=sys.argv[1];row=json.loads((OUT/stage/'compile-results.json').read_text())['results'][0]
obj=PackedCoff(Path(row['object']),'?GetInterval@GPCylinder@');bundle=PackedBundle([obj]);rng=random.Random(0x82BAD7F8);groups=[]
for mode,prefix,original in [('single','?GetInterval@GPCylinder@',0x82BAD7F8),('batch','?GetIntervals@GPCylinder@',0x82BAD938)]:
    bundle.select(prefix);bad=[]
    for index in range(192):
        gp,directions,out=0x57000100,0x58000020,0x59000020
        image=bytearray(rng.randbytes(0x200));vectors=bytearray(rng.randbytes(0x80));result_initial=rng.randbytes(0x180);count=index%6
        rows=[[rng.uniform(-8,8)for _ in range(4)]for _ in range(5)]
        if index%7==0:rows[4]=[0.,-0.,0.,1.]
        for offset,v in zip([0,0x10,0x20,0x30,0x40,0x70],rows[:4]+[rows[1],rows[4]]):struct.pack_into('>4f',image,0x100+offset,*v)
        for i in range(6):struct.pack_into('>4f',vectors,16+i*16,*[rng.uniform(-5,5)for _ in range(4)])
        if index%11==0:
            struct.pack_into('>4f',image,0x140,1.,0.,0.,3.)
            struct.pack_into('>4f',vectors,16,(-1. if index%2 else 1.),0.,0.,2.)
        if index%11==1:struct.pack_into('>4f',image,0x140,0.,0.,0.,-2.)
        results=[]
        for compiled in [False,True]:
            c=cpu(bundle);c.mem.add(gp-0x100,len(image),image);c.mem.add(directions-16,len(vectors),vectors);c.mem.add(out-16,len(result_initial),result_initial);c.setg(3,gp)
            if mode=='single':
                c.setg(4,out);c.vr[1]=struct.unpack_from('>4I',vectors,16)
            else:c.setg(4,directions);c.setg(5,count);c.setg(6,out)
            c.run(bundle.entry if compiled else original)
            results.append((c.mem.rd(out-16,len(result_initial)),c.mem.rd(gp-0x100,len(image)),c.mem.rd(directions-16,len(vectors))))
        if results[0]!=results[1]:bad.append({'case':index,'count':count,'output_offsets':[hex(i-16)for i,(a,b)in enumerate(zip(results[0][0],results[1][0]))if a!=b]})
    groups.append({'method':mode,'cases':192,'original':hex(original),'mismatches':bad})
report={'stage':stage,'groups':groups,'object':str(obj.path),'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual whole GPCylinder CPP; original and actual const-member ABI: r3/v1/r4 for single, r3/r4/r5/r6 for batch. Full input/output bytes, Interval30 stride, unchanged padding and16-byte guards; batch counts0..5 and all four finite lanes including signed-zero extents. No register/layout normalization, no SIMD hardware or full collision/game claim.'}
p=OUT/(stage+'-interval-retention-v1.json');assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print([(g['method'],g['cases'],len(g['mismatches']))for g in groups]);assert not any(g['mismatches']for g in groups)
