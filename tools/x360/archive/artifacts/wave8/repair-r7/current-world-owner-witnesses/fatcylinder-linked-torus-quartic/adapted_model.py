from pathlib import Path
import ast, json, hashlib, random, struct
base = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/scripts/validate_quartic_native_v2_model.py')
nodes = []
for n in ast.parse(base.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'out' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(base), 'exec'), globals())

class CorrectCPU(CPU):

    def exec_vector(self, pc, w, mn):
        if w >> 26 == 4 and w & 2035 in (1027, 1091):
            return cylmodel.Cpu.exec_vector(self, pc, w, mn)
        return super().exec_vector(pc, w, mn)
out = Path('D:/Reverse/BP-X360-Experimental-Artifacts/parallel/wave8_scene_terra/volume-baseline')
d = out / 'closure-selected-v1'
paths = [_r5_object(d / 'LineSegIntersect.obj'), _r5_object(d / 'GPTriangle.obj'), _r5_object(d / 'AALineClipper.obj')]
b = F.Bundle([F.Coff(paths[0], '?FatLineSegIntersect@CylinderVolume@'), F.Coff(paths[1], '?TriangleNearestPointRegion@'), F.Coff(paths[2], '?Init@AALineClipper@')])
b.select('?FatLineSegIntersect@CylinderVolume@')
rng = random.Random(2193265784)
bad = []
errors = []
counts = {'hit': 0, 'miss': 0}
calls = {'original': 0, 'compiled': 0}
target = F.address(b, '?rwcTorusLineSegIntersect@')
quartic = F.address(b, '?SolveQuarticRoots@')
qcalls = {'original': 0, 'compiled': 0}
for i in range(32):
    vol = bytearray(rng.randbytes(256))
    tr = bytearray(rng.randbytes(128))
    initial = rng.randbytes(256)
    for m in (vol, tr):
        for k, row in enumerate(((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 0, 0, 1))):
            struct.pack_into('>4f', m, 16 * k, *row)
    struct.pack_into('>4f', vol, 68, 2.0, 1.25, 1.5, 0.2)
    p1 = struct.pack('>4f', *(rng.uniform(-5, 5) for _ in range(4)))
    p2 = struct.pack('>4f', *(rng.uniform(-5, 5) for _ in range(4)))
    res = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.__class__ = CorrectCPU
        c.site_seen = [0] * len(cylmodel.SITES)
        c.rest = set()
        c.max_steps = 1000000
        for addr, img in ((1342177280, vol), (1342181376, p1), (1342185472, p2), (1342189568, tr), (1342193664, initial)):
            c.mem.add(addr, len(img), img)
        for addr, name in b.unresolved.items():
            if name.startswith('__save'):
                c.hooks[addr] = lambda cpu: None
            elif name.startswith('__rest'):
                c.rest.add(addr)
                c.hooks[addr] = lambda cpu: None

        def observe(pc, w):
            if pc == (target if compiled else 2193283760):
                calls['compiled' if compiled else 'original'] += 1
            if pc == (quartic if compiled else 2193279536):
                qcalls['compiled' if compiled else 'original'] += 1
        c.instruction_observer = observe
        c.setg(3, 1342177280)
        c.setg(4, 1342181376)
        c.setg(5, 1342185472)
        c.setg(6, 1342189568 if i & 1 else 0)
        c.setg(7, 1342193664)
        c.setf(1, 0.15)
        try:
            c.run(b.entry if compiled else 2193287952)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'error': repr(ex)})
            break
        res.append((c.g(3) & 4294967295, c.mem.rd(1342193664, 256), c.mem.rd(1342177280, 256)))
    if len(res) == 2:
        counts['hit' if res[0][0] else 'miss'] += 1
        if res[0] != res[1]:
            bad.append({'case': i, 'original_return': res[0][0], 'compiled_return': res[1][0], 'original_result': res[0][1].hex(), 'compiled_result': res[1][1].hex()})
report = {'cases': 32, 'counts': counts, 'torus_calls': calls, 'quartic_calls': qcalls, 'mismatches': bad, 'errors': errors, 'objects': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'scope': 'whole actual fat cylinder with real linked torus and quartic vs original 82BAEB10, finite identity/null-transform inputs and full result/volume guards, corrected decoder plus software estimate/log/exp model; no hardware or broad parity claim'}
p = _r5_output(out / 'fatcylinder-torus-quartic-composed-v2-comparison.json')
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print(counts, calls, qcalls, 'mismatches', len(bad), 'errors', len(errors), bad[:1], errors[:1])