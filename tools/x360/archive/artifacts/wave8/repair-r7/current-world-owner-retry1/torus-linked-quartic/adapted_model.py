from pathlib import Path
import ast, hashlib, json, random, struct
p = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/quartic-diagnosis-sol/validate_same64_correct_load.py')
nodes = []
_helper_nodes = ast.parse(p.read_text()).body
_class_end = next((i for i, n in enumerate(_helper_nodes) if isinstance(n, ast.ClassDef) and n.name == 'CorrectLoadCPU'))
nodes = _helper_nodes[:_class_end + 1]
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(p), 'exec'), globals())
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
obj = _r5_object(out / 'closure-selected-v1/LineSegIntersect.obj')
b = F.Bundle([F.Coff(obj, '?rwcTorusLineSegIntersect@')])
b.select('?rwcTorusLineSegIntersect@')
rng = random.Random(2193283760)
bad = []
errors = []
counts = {'hit': 0, 'miss': 0}
for i in range(192):
    initial = rng.randbytes(64)
    vs = [struct.unpack('>4I', struct.pack('>4f', *(rng.uniform(-3, 3) for _ in range(4)))) for _ in range(2)]
    R = rng.uniform(0.1, 3)
    r = rng.uniform(0.1, 1)
    results = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.__class__ = CorrectLoadCPU
        c.corrected_loads = 0
        c.site_seen = [0] * len(cylmodel.SITES)
        c.rest = set()
        c.max_steps = 1000000
        c.mem.add(1342177280, 64, initial)
        for addr, name in b.unresolved.items():
            if name.startswith('__save'):
                c.hooks[addr] = lambda cpu: None
            elif name.startswith('__rest'):
                c.rest.add(addr)
                c.hooks[addr] = lambda cpu: None
        c.setg(3, 1342177280)
        c.vr[1] = vs[0]
        c.vr[2] = vs[1]
        c.setf(1, R)
        c.setf(2, r)
        try:
            c.run(b.entry if compiled else 2193283760)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'error': repr(ex)})
            break
        results.append((c.g(3) & 4294967295, c.mem.rd(1342177280, 64)))
    if len(results) == 2:
        counts['hit' if results[0][0] else 'miss'] += 1
        if results[0] != results[1]:
            bad.append({'case': i, 'original_return': results[0][0], 'compiled_return': results[1][0], 'original_output': results[0][1].hex(), 'compiled_output': results[1][1].hex()})
report = {'cases': 192, 'counts': counts, 'mismatches': bad, 'errors': errors, 'object': str(obj), 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'scope': 'actual whole torus with linked quartic versus ARTIST 82BADAB0, finite random inputs, full output guards; corrected lvlx128 decoder and software estimate/log/exp model; no hardware claim'}
path = _r5_output(out / 'torus-quartic-composed-v1-comparison.json')
_r5_output(path).write_text(json.dumps(report, indent=2) + '\n')
print(counts, 'mismatches', len(bad), 'errors', len(errors), bad[:2], errors[:2])