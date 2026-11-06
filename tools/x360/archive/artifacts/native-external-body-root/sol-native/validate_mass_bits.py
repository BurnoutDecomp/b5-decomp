from pathlib import Path
import ast,hashlib,json,random,struct
source=Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py');nodes=[]
for n in ast.parse(source.read_text()).body:
    if isinstance(n,ast.Assign)and any(isinstance(t,ast.Name)and t.id=='stage'for t in n.targets):break
    nodes.append(n)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
OUT=Path(__file__).resolve().parent;row=json.loads((OUT/'candidate-v3/compile-results.json').read_text())['results'][0]
obj=PackedCoff(Path(row['object']),'?SetMass@ExternalPhysicsBody@');bundle=PackedBundle([obj]);bundle.select('?SetMass@ExternalPhysicsBody@')
word=F.M.V.x360rd.u32(0x825B41E4);assert word==0x101F59C3 # stvx128 v0,r31,r11
class Stop(Exception):pass
rng=random.Random(0x825B41E4);errors=[]
for index in range(32):
    base,self=0x57000000,0x57000040;initial=rng.randbytes(0x1A0);value=tuple(rng.getrandbits(32)for _ in range(4));outputs=[]
    for compiled in [False,True]:
        c=cpu(bundle);c.mem.add(base,len(initial),initial);c.setg(3,self);c.setg(31,self);c.setg(11,0xD0);c.vr[0]=value;c.vr[1]=value
        class Window(type(c)):
            def fetch(s,pc):
                if not compiled and pc==0x825B41E8:raise Stop()
                return super().fetch(pc)
        c.__class__=Window
        try:c.run(bundle.entry if compiled else 0x825B41E4)
        except Stop:pass
        outputs.append(c.mem.rd(base,len(initial)))
    expected=bytearray(initial);expected[0x40+0xD0:0x40+0xE0]=struct.pack('>4I',*value)
    if outputs[0]!=outputs[1]or outputs[0]!=bytes(expected):errors.append(index)
report={'cases':32,'mismatches':errors,'object':str(obj.path),'object_sha256':hashlib.sha256(obj.path.read_bytes()).hexdigest(),'original_store_pc':'825B41E4','original_word':hex(word),'verifier_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),'limits':'Isolated actual original inlined store versus entire compiled genuine SetMass(VecFloat) member. Original inline vector register and body base/index operands are supplied as an instruction-width/value diagnostic; this is not an invented original out-of-line setter ABI. Arbitrary all4 raw input lanes, entire120-byte body and guards match. Root separately binds the actual whole PhysicalBodyPart constructor/caller with unchanged retail receiver/v1; no normalization in that composed proof. No floating arithmetic or runtime/hardware/game claim.'}
p=OUT/'candidate-v3-setmass-bits.json';assert not p.exists();p.write_text(json.dumps(report,indent=2)+'\n');print('SetMass raw lanes',32,'mismatches',len(errors));assert not errors
