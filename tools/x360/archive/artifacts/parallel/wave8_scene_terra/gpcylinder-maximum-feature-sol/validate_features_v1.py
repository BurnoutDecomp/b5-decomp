from pathlib import Path
import ast,hashlib,json,math,random,struct,sys
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent;stage=sys.argv[1];row=json.loads((OUT/stage/'compile-results.json').read_text())['results'][0]
obj=PackedCoff(Path(row['object']),'?GetMaximumFeature@GPCylinder@');bundle=PackedBundle([obj]);bundle.select('?GetMaximumFeature@GPCylinder@')
bits=lambda f:struct.unpack('>I',struct.pack('>f',f))[0]
f32=lambda f:struct.unpack('>f',struct.pack('>f',f))[0]
frombits=lambda n:struct.unpack('>f',struct.pack('>I',n))[0]
labels={0x82BAE4EC:'face',0x82BAE520:'negative_cap_flip',0x82BAE6D0:'face_descending_edge',0x82BAE6F4:'face_ascending_edge',0x82BAE740:'face_root_estimate',0x82BAE75C:'face_root_first_refine',0x82BAE774:'face_root_second_refine',0x82BAE780:'face_reciprocal_estimate',0x82BAE7AC:'face_dir_first_refine',0x82BAE7C8:'face_dir_second_refine',0x82BAE7B0:'face_plane_first_refine',0x82BAE7DC:'face_plane_second_refine',0x82BAE828:'point',0x82BAE860:'point_root_estimate',0x82BAE870:'point_root_first_refine',0x82BAE880:'point_root_second_refine',0x82BAE8B0:'negative_point_flip',0x82BAE8E4:'side',0x82BAE938:'side_direction_root',0x82BAE948:'side_direction_first_refine',0x82BAE95C:'side_direction_second_refine',0x82BAE98C:'side_length_root',0x82BAE9A0:'side_length_first_refine',0x82BAE9B4:'side_length_second_refine',0x82BAE9CC:'side_down_root',0x82BAE9F8:'side_down_first_refine',0x82BAEA08:'side_down_second_refine',0x82BAEA28:'common_tail',0x82BAEA80:'edge_world_transform'}
rng=random.Random(0x82BAE430);mismatches=[];exceptions=[];records=[];coverage={};counts={}
for index in range(640):
    kind=index%16
    position=[f32(rng.uniform(-100,100))for _ in range(4)]
    # Canonical localX/localY/axisZ frame; unrelated W lanes remain distinct.
    frame=[[1.,0.,0.,f32(rng.uniform(-3,3))],[0.,1.,0.,f32(rng.uniform(-3,3))],[0.,0.,1.,f32(rng.uniform(-3,3))]]
    dimensions=[f32(rng.uniform(.1,6)),f32(rng.uniform(.1,5)),f32(rng.uniform(-4,4)),f32(rng.uniform(-4,4))]
    direction=[.8,.3,.5,f32(rng.uniform(-4,4))]
    if kind==0:direction[:3]=[0.,0.,1.]
    elif kind==1:direction[:3]=[0.,0.,-1.]
    elif kind==2:direction[:3]=[1.,0.,0.]
    elif kind==3:direction[:3]=[-.7,.5,-0.01]
    elif kind==4:direction[:3]=[.8,-.3,-.5]
    elif kind==5:direction[:3]=[.7,.2,frombits(0x3F7FF2E5+(index//16)%3-1)]
    elif kind==6:direction[:3]=[.7,.2,frombits(0x3CA3D70A+(index//16)%3-1)]
    elif kind==7:direction[:3]=[0.,0.,0.]
    elif kind==8:direction[:3]=[0.,0.,1.];dimensions[1]=0.
    elif kind==9:direction[:3]=[.5,1.,0.];dimensions[0]=0.
    elif kind==10:direction[:3]=[0.,0.,-1.];dimensions[0]*=-1.;dimensions[1]*=-1.
    elif kind==11:direction[:3]=[0.,0.,-.5]
    elif kind==12:frame=[[-.8,.6,0.,1.2],[-.6,-.8,0.,-.7],[0.,0.,1.,.3]];direction[:3]=[.6,.8,0.]
    elif kind==13:frame=[[f32(rng.uniform(-1,1))for _ in range(4)]for _ in range(3)]
    elif kind==14:direction[:3]=[.8,-.3,.5];dimensions[1]=1e-20
    elif kind==15:direction[:3]=[-0.,0.,-0.];dimensions[0]=-0.
    if index>=512:
        value=frombits([0x7FC12345,0xFFC54321,0x7F800000,0xFF800000,1,0x80000001][(index//16)%6]);group=(index//6)%4
        if group==0:position[(index//4)%4]=value
        elif group==1:direction[(index//4)%4]=value
        elif group==2:dimensions[(index//4)%4]=value
        else:frame[(index//4)%3][index%4]=value
    gp,feature=0x57000100,0x58000100;image=bytearray(rng.randbytes(0x200));initial=rng.randbytes(0x500)
    for offset,v in [(0,position),(0x10,frame[2]),(0x20,frame[1]),(0x30,frame[0]),(0x40,frame[2]),(0x70,dimensions)]:struct.pack_into('>4f',image,0x100+offset,*v)
    ccw=[0,1,2,0xFFFFFFFF][(index//3)%4];outputs=[];errors=[]
    for compiled in [False,True]:
        c=cpu(bundle);c.mem.add(gp-0x100,len(image),image);c.mem.add(feature-0x100,len(initial),initial);c.mem.wr(F.M.S.STACK,b'\xA5'*0x20000)
        def copy(s):
            d,a,n=(s.g(r)&0xFFFFFFFF for r in [3,4,5]);s.mem.wr(d,s.mem.rd(a,n));s.setg(3,d)
        c.hooks[0x82C0A630]=copy
        for at,name in bundle.unresolved.items():
            if name=='memcpy':c.hooks[at]=copy
        class Observe(type(c)):
            def fetch(s,pc):
                word=super().fetch(pc)
                if not compiled:
                    if pc in labels:label=labels[pc];coverage[label]=coverage.get(label,0)+1
                    if pc==0x82BAE824 and s.cr[6]&1:coverage['unordered_side_selection']=coverage.get('unordered_side_selection',0)+1
                return word
        c.__class__=Observe;c.setg(3,gp);c.setg(4,ccw);c.setg(5,feature);c.vr[1]=tuple(map(bits,direction))
        try:
            c.run(bundle.entry if compiled else 0x82BAE430)
            outputs.append((c.mem.rd(feature-0x100,len(initial)),c.mem.rd(gp-0x100,len(image))))
        except Exception as e:errors.append({'compiled':compiled,'error':str(e),'pc':hex(getattr(c,'last_pc',0))})
    if errors:exceptions.append({'case':index,'kind':kind,'errors':errors});continue
    count=int.from_bytes(outputs[0][0][0x330:0x334],'big');counts[count]=counts.get(count,0)+1
    if outputs[0]!=outputs[1]:mismatches.append({'case':index,'kind':kind,'count':count,'ccw':hex(ccw),'offsets':[hex(i-0x100)for i,(a,b)in enumerate(zip(outputs[0][0],outputs[1][0]))if a!=b],'gp':image.hex(),'direction_words':list(map(hex,map(bits,direction))),'initial':initial.hex(),'original':outputs[0][0].hex(),'compiled':outputs[1][0].hex()})
    records.append({'case':index,'kind':kind,'count':count,'ccw':hex(ccw),'original_sha256':hashlib.sha256(outputs[0][0]).hexdigest(),'compiled_sha256':hashlib.sha256(outputs[1][0]).hexdigest()})
report={'stage':stage,'requested':640,'compared':len(records),'feature_counts':counts,'mismatches':mismatches,'model_exceptions':exceptions,'cases':records,'native_branch_coverage':coverage,'object':str(obj.path),'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Actual whole GPCylinder CPP, original440-instruction function and compiled body. Same retail r3/r4/r5/v1; complete Feature/GP bytes and guards including all four lanes. Standard memcpy is controlled byte service. UniformA5 stack poison controls the genuinely copied unstaged side pn row, then both real functions transform it; arbitrary prior stack payload is not certified. Shared software VMX estimates/rounding used, denormal/hardware fidelity not claimed. Nonfinite exceptions recorded as model exclusions, never product passes. No global collision/provider union, GPU/frame/gameplay or launch claim.'}
p=OUT/(stage+'-feature-diagnostic-v1.json');assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('compared',len(records),'counts',counts,'mismatches',len(mismatches),'exceptions',len(exceptions));print('first bad',[(x['case'],x['kind'],x['count'],x['offsets'][:20])for x in mismatches[:7]]);print('first exceptions',exceptions[:2]);print('coverage',coverage)
