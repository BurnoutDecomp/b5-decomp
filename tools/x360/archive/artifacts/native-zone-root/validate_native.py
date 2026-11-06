from pathlib import Path
import ast, hashlib, json, random, struct

HERE = Path(__file__).resolve().parent
source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-brnpostfx-root/validate_caller.py')
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
report_path = HERE / 'native-final/compile-results.json'
report = json.loads(report_path.read_text())
paths = {Path(r['source']).name: Path(r['object']) for r in report['results'] if r['passed']}
BASE, SELF, POINTS, OUT = 0x57000000, 0x57000100, 0x57000400, 0x57000800
rng = random.Random(0x822BAB60)
errors, details = [], []
for kind, path, prefix, original in [
    ('point', paths['ZoneList_GetFirstZoneForPoint.cpp'], '?IsPointInZone@', 0x822BAB60),
    ('centre', paths['Zone.cpp'], '?CalculateCentre@', 0x822BACE8),
    ('centre4', paths['Zone.cpp'], '?CalculateCentre4@', 0x822A3CA0)]:
    obj = PackedCoff(path, prefix)
    for index in range(256):
        count = 4 if kind == 'centre4' else index % 10 if kind == 'point' else index % 9 + 1
        image = bytearray(rng.randbytes(0x1000))
        struct.pack_into('>I', image, SELF-BASE, POINTS)
        struct.pack_into('>h', image, SELF-BASE+0x1A, count)
        for i in range(max(1,count)):
            values = [f32(rng.uniform(-100,100)) for _ in range(4)]
            if index % 7 == 0:
                values = [0.0, -0.0, -0.0, 0.0]
            struct.pack_into('>4f', image, POINTS-BASE+16*i, *values)
        point = [f32(rng.uniform(-150,150)) for _ in range(4)]
        if index % 8 == 0 and count:
            point = list(struct.unpack_from('>4f', image, POINTS-BASE))
        results = []
        for compiled in (False, True):
            c = cpu(obj)
            c.mem.add(BASE,len(image),image)
            if kind == 'point':
                c.setg(3,SELF)
                c.setg(4,0xDEAD0004) # reject the obsolete reference argument.
                c.setvf(1,point)
            else:
                c.setg(3,OUT)
                c.setg(4,SELF)
            c.run(obj.entry if compiled else original)
            results.append((c.g(3)&0xFFFFFFFF,c.mem.rd(BASE,len(image))))
        if results[0] != results[1]:
            errors.append({'kind':kind,'index':index,'count':count,'point':point,
                'original_return':hex(results[0][0]),'compiled_return':hex(results[1][0]),
                'original_output':results[0][1][OUT-BASE:OUT-BASE+16].hex(),
                'compiled_output':results[1][1][OUT-BASE:OUT-BASE+16].hex(),
                'different_bytes':[i for i,(a,b) in enumerate(zip(results[0][1],results[1][1]))if a!=b]})
    details.append({'kind':kind,'symbol':obj.fn[0],'original':hex(original),'cases':256,
                    'object':str(path),'object_sha256':sha(path)})
    print(kind,'256 checked',flush=True)
result={'cases':768,'mismatches':errors,'functions':details,'verifier_sha256':sha(__file__),
    'native_report_sha256':sha(report_path),
    'limits':'Actual full original and compiled member functions, unchanged native v1 point and hidden-sret centre registers, complete mapped source/result/guard bytes and returns. Finite random xyzw and signedzero, count0..9 point/count1..9 centre/4specialized; point-on-corner cases included. Same softwareVMX reciprocal estimate surrogate and CRT nonvolatile convention; does not prove actual hardware estimates, invalidCentre4 assert metadata, malformed allocations, wholeZoneList traversal or world/frame/game behavior.'}
with(HERE/'native-comparison.json').open('x',encoding='utf-8')as f:json.dump(result,f,indent=2)
print(json.dumps({'cases':768,'mismatches':len(errors)}),flush=True)
assert not errors
