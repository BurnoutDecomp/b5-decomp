from pathlib import Path
import ast, json, random, struct, sys

HERE = Path(__file__).resolve().parent
source = HERE / 'validate_caller.py'
nodes = []
for node in ast.parse(source.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
stage = sys.argv[3] if len(sys.argv) > 3 else 'native-c0d-vmx'
report_path = HERE / stage / 'compile-results.json'
report = json.loads(report_path.read_text())
path = Path(next(r['object'] for r in report['results'] if r['source'].endswith('BrnPfxHookBlender.cpp')))
BASE, SELF, SRC = 0x54000000, 0x54000040, 0x54000200
rng = random.Random(0x824F6810)
errors, details = [], []
count = int(sys.argv[1]) if len(sys.argv) > 1 else 128
output_name = sys.argv[2] if len(sys.argv) > 2 else 'accumulator-smoke.json'
for name, original, weight_offset, count_offset, payload in [
    ('BloomBlend', 0x824F6810, 0x20, 0x24, 0x20),
    ('TintData2dBlend', 0x824F6D60, 0x10, 0x14, 0x10)]:
    obj = PackedCoff(path, '?Add@' + name + '@')
    print(name, obj.unresolved, flush=True)
    for index in range(count):
        initial = bytearray(rng.randbytes(0x400))
        for offset in range(0, payload, 4):
            for at in (SELF + offset, SRC + offset):
                struct.pack_into('>f', initial, at - BASE, rng.uniform(-1e5, 1e5))
        n = [0.0, 1.0, 3.0, 0.0][index % 4]
        weight = f32([0.0, 1.0, -0.5, 0.25, rng.uniform(-1, 1)][index % 5])
        struct.pack_into('>f', initial, SELF - BASE + weight_offset, rng.uniform(-1, 1))
        struct.pack_into('>f', initial, SELF - BASE + count_offset, n)
        results = []
        for compiled in (False, True):
            c = cpu(obj)
            c.mem.add(BASE, len(initial), initial)
            c.setg(3, SELF)
            c.setg(4, 0 if index % 8 == 0 else SRC)
            c.setf(1, weight)
            c.run(obj.entry if compiled else original)
            results.append((c.g(3) & 0xFFFFFFFF, c.mem.rd(BASE, len(initial))))
        if results[0] != results[1]:
            errors.append({'name': name, 'index': index, 'weight': weight, 'initial_count': n,
                'original_return': results[0][0], 'compiled_return': results[1][0],
                'different_bytes': [i for i, (a,b) in enumerate(zip(results[0][1], results[1][1])) if a != b],
                'original_payload': results[0][1][SELF-BASE:SELF-BASE+0x30].hex(),
                'compiled_payload': results[1][1][SELF-BASE:SELF-BASE+0x30].hex()})
    details.append({'name': name, 'original': hex(original), 'symbol': obj.fn[0], 'cases': count})

result = {'cases': count * 2, 'mismatches': errors, 'functions': details,
    'object': str(path), 'object_sha256': sha(path), 'report_sha256': sha(report_path),
    'verifier_sha256': sha(__file__),
    'limits': 'Actual complete original and native Bloom/Tint Add code; finite payloads and weights[-1,1], null/first-copy/later-blend and max-weight-update paths, full mapped source/destination/guard bytes and returned pointer. Native scalar FPU operations and native VMX source multiply then fused old*inverse+source execute. Does not cover exceptional payloads, invalid over-one assert path, GPU/hardware or other Add/SetToBlend providers.'}
with (HERE / output_name).open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}))
assert not errors
