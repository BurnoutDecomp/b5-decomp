from pathlib import Path
import ast,hashlib,json,math,random,struct,sys
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent;stage=sys.argv[1]
row=json.loads((OUT/stage/'compile-results.json').read_text())['results'][0]
edge=OUT.parent/'volume-baseline/FeatureEdge-rebind-wip.obj'
objects=[PackedCoff(Path(row['object']),'?GetMaximumFeature@GPBox@'),PackedCoff(edge,'??0FeatureEdge@')]
bundle=PackedBundle(objects);bundle.select('?GetMaximumFeature@GPBox@')
epsilon=F.address(bundle,'VEC_EPSILON')
bits=lambda f:struct.unpack('>I',struct.pack('>f',f))[0]
f32=lambda f:struct.unpack('>f',struct.pack('>f',f))[0]
rng=random.Random(0x82BA8918);mismatches=[];counts={};exceptions=[];records=[];coverage={};classifications={}
edge_entry=F.address(bundle,'??0FeatureEdge@')
branch_labels={0x82BA8B40:'ccw_cross',0x82BA8BD8:'clockwise_cross',0x82BA8B9C:'wallV_ccw_estimate',0x82BA8C38:'wallV_clockwise_estimate',0x82BA8CBC:'wallU_estimate',0x82BA8BAC:'wallV_ccw_first_refine',0x82BA8BBC:'wallV_ccw_second_refine',0x82BA8C48:'wallV_clockwise_first_refine',0x82BA8C58:'wallV_clockwise_second_refine',0x82BA8CCC:'wallU_first_refine',0x82BA8CDC:'wallU_second_refine',0x82BA8DD0:'forward_ring_iteration',0x82BA8E74:'reverse_ring_iteration'}
for index in range(640):
    kind=index%12;axis=(index//12)%3
    pos=[f32(rng.uniform(-200,200))for _ in range(3)]+[f32(rng.uniform(-5,5))]
    normals=[[float(j==i)for j in range(3)]+[f32(rng.uniform(-4,4))]for i in range(3)]
    dims=[f32(rng.uniform(.1,7))for _ in range(4)]
    direction=[0.,0.,0.,f32(rng.uniform(-3,3))]
    if kind in [0,1,7,8,11]:direction[axis]=1.0 if kind!=1 else -1.0
    elif kind==2:direction[axis]=1.;direction[(axis+1)%3]=.75
    elif kind in [3,9]:direction[:3]=[.7,-1.1,1.3]
    elif kind==5:direction[:3]=[1.,1.,0.];dims[:3]=[0.,0.,0.]
    elif kind==6:direction[:3]=[struct.unpack('>f',struct.pack('>I',0x3E4CCCCD+(index//12)%3-1))[0],0.,1.]
    elif kind==10:direction[:3]=[-0.,0.,-0.];dims[axis]=-0.
    if kind==7:normals[(axis+1)%3][:3]=[0.,0.,0.]
    if kind==8:normals[(axis+1)%3][(axis+1)%3]=0.0001
    if kind==11:normals[(axis+1)%3][(axis+1)%3]=[.000345,.000346,.001][(index//12)%3]
    if kind==9:
        normals=[ [f32(rng.uniform(-1,1))for _ in range(4)]for _ in range(3)]
    if index>=512:
        # Separate bit-class cases; interpreter failures are recorded as model
        # limits rather than silently classed as a product pass.
        special=[0x7FC12345,0xFFC54321,0x7F800000,0xFF800000,0x00000001,0x80000001][(index//12)%6]
        value=struct.unpack('>f',struct.pack('>I',special))[0]
        group=(index//6)%4
        if group==0:pos[(index//4)%4]=value
        elif group==1:direction[(index//4)%4]=value
        elif group==2:dims[(index//4)%4]=value
        else:normals[axis][(index//4)%4]=value
    gp,feature=0x57000100,0x58000100
    image=bytearray(rng.randbytes(0x200));out_initial=rng.randbytes(0x500)
    for offset,v in [(0,pos),(0x10,normals[0]),(0x20,normals[1]),(0x30,normals[2]),(0x70,dims)]:struct.pack_into('>4f',image,0x100+offset,*v)
    ccw=[0,1,2,0xFFFFFFFF][(index//3)%4];outputs=[];case_errors=[];endpoints=[[],[]]
    for compiled in [False,True]:
        c=cpu(bundle);c.mem.add(gp-0x100,len(image),image);c.mem.add(feature-0x100,len(out_initial),out_initial)
        c.mem.wr(F.M.S.STACK,b'\xA5'*0x20000)
        c.mem.add(0x8327EEA0,16,struct.pack('>4I',*([0x34000000]*4)))
        c.mem.wr(epsilon,struct.pack('>4I',*([0x34000000]*4)))
        def copy(s):
            d,a,n=(s.g(r)&0xFFFFFFFF for r in [3,4,5]);s.mem.wr(d,s.mem.rd(a,n));s.setg(3,d)
        c.hooks[0x82C0A630]=copy
        for at,name in bundle.unresolved.items():
            if name=='memcpy':c.hooks[at]=copy
        c.setg(3,gp);c.setg(4,ccw);c.setg(5,feature);c.vr[1]=tuple(map(bits,direction))
        def observe(pc,word):
            if not compiled:
                if pc in branch_labels:
                    label=branch_labels[pc];coverage[label]=coverage.get(label,0)+1
                if pc==0x82BA89A4:
                    n=c.g(8)&0xFFFFFFFF;classifications[n]=classifications.get(n,0)+1
                if pc==0x82BA8980 and c.cr[6]&1:coverage['unordered_classifications']=coverage.get('unordered_classifications',0)+1
            if pc==(edge_entry if compiled else 0x82BA85B8):
                endpoints[int(compiled)].append((c.mem.rd(c.g(4)&0xFFFFFFFF,16).hex(),c.mem.rd(c.g(5)&0xFFFFFFFF,16).hex()))
        class ObserveCPU(type(c)):
            def fetch(s,pc):
                word=super().fetch(pc)
                observe(pc,word)
                return word
        c.__class__=ObserveCPU
        try:
            c.run(bundle.entry if compiled else 0x82BA8918)
            outputs.append((c.mem.rd(feature-0x100,len(out_initial)),c.mem.rd(gp-0x100,len(image))))
        except Exception as e:
            case_errors.append({'compiled':compiled,'error':str(e),'last_pc':hex(getattr(c,'last_pc',0))})
    if case_errors:
        exceptions.append({'case':index,'kind':kind,'errors':case_errors});continue
    assert endpoints[0]==endpoints[1],(index,'actual FeatureEdge endpoint argument mismatch',endpoints)
    count=int.from_bytes(outputs[0][0][0x330:0x334],'big');counts[count]=counts.get(count,0)+1
    assert len(endpoints[0])==len(endpoints[1])==int(count==1),(index,'unobserved endpoint boundary',count,endpoints)
    bad=[i-0x100 for i,(a,b)in enumerate(zip(outputs[0][0],outputs[1][0]))if a!=b]
    if outputs[0]!=outputs[1]:
        mismatches.append({'case':index,'kind':kind,'count':count,'ccw':hex(ccw),'offsets':list(map(hex,bad)),'gp':image.hex(),'direction_words':list(map(hex,map(bits,direction))),'out_initial':out_initial.hex(),'original':outputs[0][0].hex(),'compiled':outputs[1][0].hex()})
    records.append({'case':index,'kind':kind,'count':count,'ccw':hex(ccw),'original_sha256':hashlib.sha256(outputs[0][0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1][0]).hexdigest()})
report={'stage':stage,'requested':640,'compared':len(records),'feature_counts':counts,'mismatches':mismatches,'model_exceptions':exceptions,'cases':records,'native_branch_coverage':coverage,'native_axis_classifications':classifications,'actual_FeatureEdge_endpoint_arguments_equal':True,'featureedge_inputs_sha256':hashlib.sha256((OUT.parent/'volume-baseline/FeatureEdge-rebind-inputs.json').read_bytes()).hexdigest(),'objects':{str(o.path):hashlib.sha256(o.path.read_bytes()).hexdigest()for o in objects},'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Final file-level actual whole GPBox CPP and current-header World FeatureEdge rebind linked. Same retail r3/r4/r5/v1, no input rebinding. Full Feature/GP object bytes and surrounding guards compared, incl XYZW. Identical uniformlyA5-poisoned stack controls undefined temporary pn bytes in the edge branch; arbitrary prior stack state is not certified. Epsilon runtime row is controlled to attested34000000 in original/compiled provider; CRT lifecycle is separately owned. vrsqrtefp/vrefp/VMX floating math uses shared software estimate semantics, not hardware. Nonfinite interpreter exceptions are explicit exclusions. No GPU/frame/whole game or combined shared graph integration claim.'}
p=OUT/(stage+'-feature-comparison-v4.json');assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('compared',len(records),'counts',counts,'mismatches',len(mismatches),'exceptions',len(exceptions));print('first mismatches',[(x['case'],x['kind'],x['count'],x['offsets'][:16])for x in mismatches[:6]]);print('first exceptions',exceptions[:2])

print('coverage',coverage,'classifications',classifications);assert not mismatches and not exceptions
