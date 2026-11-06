from pathlib import Path
import ast, hashlib, json, random, struct, sys

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
stage = sys.argv[1] if len(sys.argv) > 1 else 'native'
output_name = sys.argv[2] if len(sys.argv) > 2 else 'transform-comparison.json'
report = json.loads((HERE / stage / 'compile-results.json').read_text())
path = Path(report['results'][0]['object'])
obj = PackedCoff(path, '?SetScreenTransform@?')
Return = F.CPU.run.__globals__['Return']
BASE, SELF, RENDERER = 0x56000000, 0x56000100, 0x56000300
rng = random.Random(0x823E8750)
errors, outputs = [], []
for index in range(16):
    data = bytearray(rng.randbytes(0x1000))
    data[SELF-BASE+0x16] = 1
    result = []
    for compiled in (False, True):
        c = cpu(obj)
        c.mem.add(BASE, len(data), data)
        def capture(state):
            result.append(state.mem.rd(state.g(3) & 0xFFFFFFFF, 0x40))
            raise Return()
        c.hooks[0x823ABED8] = lambda state: None
        c.hooks[0x823DB448] = capture
        for address, name in obj.unresolved.items():
            if name.startswith('?TransformByAspectRatio@Im2dTransform@'):
                c.hooks[address] = capture
        c.setg(3, RENDERER if compiled else SELF)
        c.setg(4, RENDERER)
        c.run(obj.entry if compiled else 0x823E8750)
        assert c.mem.rd(BASE, len(data)) == bytes(data), 'transform preparation modified mapped objects'
    assert len(result) == 2
    outputs.append(result[1].hex())
    if result[0] != result[1]:
        errors.append({'index': index, 'original': result[0].hex(), 'compiled': result[1].hex()})
result = {'cases': 16, 'mismatches': errors, 'object': str(path), 'object_sha256': sha(path),
    'compiled_actual_helper': obj.fn[0], 'original': '0x823E8750 prefix through native Im2dTransform::TransformByAspectRatio call',
    'prepared_transform_bytes': outputs[0], 'verifier_sha256': sha(__file__),
    'limits': 'Actual original overlay prefix and actual emitted product helper prepare all four native16-byte transform fields, captured at same genuine aspect-ratio method boundary. Sixteen randomized mapped object/stack guard states; native Im2d BeginRendering controlled, aspect-ratio/body/GPU/draw not executed. Does not claim complete loading-screen behavior, native image or launch.'}
with (HERE / output_name).open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': 16, 'mismatches': len(errors)}))
assert not errors
