from pathlib import Path
import ast, hashlib, json, random, sys
OUT = Path(__file__).resolve().parent
source = OUT / 'validate_construct.py'
nodes = []
for n in ast.parse(source.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'stage' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
stage = sys.argv[1]
rows = {Path(r['source']).name: r for r in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\replay-director-post-ice-negative\\actual-row-selection.json').read_text())['results'] if r['passed']}
spec = [('BrnDirectorModule.cpp', '?Construct@DirectorModule@BrnDirector@@'), ('BrnReplayBaseSerialiser.cpp', '?Construct@BaseSerialiser@'), ('CgsStrStream.cpp', '??0StrStream@')]
objects = [PackedCoff(Path(rows[n]['object']), prefix) for n, prefix in spec]
bundle = PackedBundle(objects)
bundle.select(spec[0][1])
entry = bundle.entry

class Stop(Exception):
    pass

def z(c, p):
    a = bytearray()
    while (v := c.mem.rd(p + len(a), 1)[0]):
        a.append(v)
    return bytes(a)
rng = random.Random(2183513764)
cases = []
for index in range(128):
    self = 1459617792
    initial = rng.randbytes(233472)
    results = []
    for compiled in [False, True]:
        c = cpu(bundle)
        c.mem.add(self, len(initial), initial)
        c.setg(31, self)
        events = []
        call = entry + 228 if compiled else 2183513764
        end = entry + 232 if compiled else 2183513768

        class WindowCPU(type(c)):

            def step(s, pc, word, nxt, depth):
                if pc == call:
                    events.append({'this': s.g(3) & 4294967295, 'id': s.g(4) & 4294967295, 'context': s.g(5) & 4294967295, 'dynamic': s.g(6) & 4294967295, 'static': s.g(7) & 4294967295, 'name': z(s, s.g(8) & 4294967295).decode(), 'skip': s.g(9) & 255})
                if pc == end:
                    raise Stop()
                return super().step(pc, word, nxt, depth)
        c.__class__ = WindowCPU
        try:
            c.run(entry + 192 if compiled else 2183513724)
        except Stop:
            pass
        assert len(events) == 1
        results.append((events[0], c.mem.rd(self, len(initial))))
    a, b = (results[0][0], results[1][0])
    assert a['this'] == self + 231728 and b['this'] == self + 232000
    assert {k: v for k, v in a.items() if k != 'this'} == {k: v for k, v in b.items() if k != 'this'}
    assert results[0][1] != results[1][1]
    cases.append({'case': index, 'original': a, 'compiled': b, 'differing_bytes': sum((a != b for a, b in zip(results[0][1], results[1][1])))})
report = {'stage': stage, 'negative_cases': 128, 'cases': cases, 'objects': {str(o.path): hashlib.sha256(o.path.read_bytes()).hexdigest() for o in objects}, 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'limits': 'Actual original call-setup window8225C67C..6A4 and actual compiled callerC0..E4, composed with original BaseCtor and real reconstructed BaseCtor respectively; no input/this offset is rebound or normalized. All outgoing constructor value arguments/name agree, but real contained this pointer differs by+100 and resulting object stores differ. This is a live Director containment negative. Other Director constructor stores/services, header layout reconstruction, allocator or full-game behavior are not certified.'}
p = _r5_output(OUT / (stage + '-director-negative-v1.json'))
assert not p.exists()
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print('Director negative', len(cases), 'original38930 candidate38A30')