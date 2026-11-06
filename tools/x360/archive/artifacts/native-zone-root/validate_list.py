from pathlib import Path
import ast, json, random, struct

HERE = Path(__file__).resolve().parent
source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes=[]
for node in ast.parse(source.read_text()).body:
    if isinstance(node,ast.Assign) and any(isinstance(t,ast.Name)and t.id=='report_path'for t in node.targets):break
    nodes.append(node)
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(source),'exec'),globals())
report=json.loads((HERE/'native-final/compile-results.json').read_text())
paths={Path(r['source']).name:Path(r['object'])for r in report['results']if r['passed']}
objects=[PackedCoff(paths['ZoneList_GetFirstZoneForPoint.cpp'],'?GetFirstZoneForPoint@'),
         PackedCoff(paths['Zone.cpp'],'?GetZone@Neighbour@')]
bundle=PackedBundle(objects);bundle.select('?GetFirstZoneForPoint@')
class ListCPU(TintCPU):
    def step(self,pc,word,nxt,depth):
        if word>>26==31 and ((word>>1)&1023)==278:
            return nxt # dcbt cache hint; no cache/hardware effects modeled.
        return super().step(pc,word,nxt,depth)
BASE,SELF,ZONES,POINTS,NEIGHBOURS=0x58000000,0x58000100,0x58000200,0x58001000,0x58002000
rng=random.Random(0x822BADA8);errors=[]
for index in range(128):
    data=bytearray(rng.randbytes(0x3000));total=0 if index%11==0 else 3
    struct.pack_into('>I',data,SELF-BASE+4,ZONES)
    struct.pack_into('>I',data,SELF-BASE+0x10,total)
    for i in range(3):
        zone=ZONES+i*0x30;points=POINTS+i*0x100;safe=NEIGHBOURS+i*0x40;unsafe=safe+0x10
        struct.pack_into('>III',data,zone-BASE,points,safe,unsafe)
        struct.pack_into('>hhh',data,zone-BASE+0x1A,4,1,1)
        struct.pack_into('>I',data,safe-BASE,ZONES+((i+1)%3)*0x30)
        struct.pack_into('>I',data,unsafe-BASE,ZONES+((i+2)%3)*0x30)
        for k,(x,y)in enumerate(((0,0),(4,0),(4,4),(0,4))):
            struct.pack_into('>4f',data,points-BASE+16*k,float(x+i*10),float(y),rng.uniform(-10,10),rng.uniform(-10,10))
    hint=-1 if not total or index%4==0 else index%3
    point=[float((index%4)*10+2),2.0,3.0,-4.0]
    results=[]
    for compiled in(False,True):
        c=cpu(bundle);c.__class__=ListCPU;c.mem.add(BASE,len(data),data);c.setg(3,SELF);c.setg(4,hint);c.setvf(1,point)
        c.run(bundle.entry if compiled else 0x822BADA8)
        results.append((c.g(3)&0xFFFFFFFF,c.mem.rd(BASE,len(data))))
    if results[0]!=results[1]:errors.append({'index':index,'hint':hint,'total':total,'point':point,'returns':[hex(r[0])for r in results]})
result={'cases':128,'mismatches':errors,'objects':{str(o.path):sha(o.path)for o in objects},
        'verifier_sha256':sha(__file__),'limits':'Actual full original ZoneList and compiled ZoneList→realZone point→realNeighbour getter code; empty/nohint/hint/safe/unsafe/fullscan/hit/miss over3 finite disjoint rectangles, nativev1/r3/r4 unchanged, complete mapped bytes and return pointers. SoftwareFP/VMX/CRT convention, private dcbt no-op cache hint, valid neighbour counts only. Initial unsupported dcbt fixture failure preserved in tool output. No fullserialization/concurrency/world/game/hardware claim.'}
with(HERE/'list-comparison.json').open('x',encoding='utf-8')as f:json.dump(result,f,indent=2)
print(json.dumps({'cases':128,'mismatches':len(errors)}));assert not errors
