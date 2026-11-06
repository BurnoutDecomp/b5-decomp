from pathlib import Path
import ast, hashlib, json, random, struct, sys
OUT = Path(__file__).resolve().parent
source = OUT / 'validate_construct.py'
nodes = []
for n in ast.parse(source.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'stage' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
stage = sys.argv[1]
rows = {Path(r['source']).name: r for r in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\replay-providers\\actual-row-selection.json').read_text())['results'] if r['passed']}
providers = [('DirectorBridgeSerialiser', 2187642008), ('RaceCarEntitySerialiser', 2187642048), ('GameModuleSerialiser', 2187642528), ('PropEntitySerialiser', 2187642560), ('TrafficEntitySerialiser', 2187643256), ('EffectsSerialiser', 2187643352), ('SoundSerialiser', 2187643384), ('GuiModuleSerialiser', 2187644840)]
objects = [PackedCoff(Path(rows['BrnReplayBaseSerialiser.cpp']['object']), '?Construct@BaseSerialiser@'), PackedCoff(Path(rows['CgsStrStream.cpp']['object']), '??0StrStream@')]
objects += [PackedCoff(Path(rows['BrnReplay' + n + '.cpp']['object']), '?Construct@' + n + '@') for n, _ in providers]
bundle = PackedBundle(objects)
rng = random.Random(2187641472)
groups = []
for kind, original in [('BaseSerialiser', 2187641472)] + providers:
    bundle.select('?Construct@' + kind + '@')
    mismatches = []
    cases = 256 if kind == 'BaseSerialiser' else 128
    for index in range(cases):
        base, self, name = (1459617792, 1459618048, 1476395008)
        initial = rng.randbytes(1280)
        text = bytes((rng.choice(range(1, 256)) for _ in range(index % 32))) + b'\x00'
        namebytes = b'\xa7' * 16 + text + b'k' * 48
        results = []
        for compiled in [False, True]:
            c = cpu(bundle)
            c.mem.add(base, len(initial), initial)
            c.mem.add(name, len(namebytes), namebytes)
            c.setg(3, self)
            if kind == 'BaseSerialiser':
                values = [index % 11, index % 2, [0, 1, 110592, 2147483647][index % 4], [0, 1, 110592, 2147483647][index // 4 % 4], 0 if index % 9 == 0 else name + 16, index % 2]
                for reg, value in zip(range(4, 10), values):
                    c.setg(reg, value)
            c.run(bundle.entry if compiled else original)
            results.append((c.mem.rd(base, len(initial)), c.mem.rd(name, len(namebytes))))
        if results[0] != results[1]:
            mismatches.append({'case': index, 'offsets': [hex(i - (self - base)) for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b]})
    groups.append({'kind': kind, 'original': hex(original), 'cases': cases, 'mismatches': mismatches})
report = {'stage': stage, 'groups': groups, 'objects': {str(o.path): hashlib.sha256(o.path.read_bytes()).hexdigest() for o in objects}, 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'limits': 'Complete original and real compiled Base plus all eight emitted leaf Construct methods, linked to actual reconstructed Base and StrStream. Full poisoned object/guard and source bytes, ordinary enum/context/sizes/bool values and valid0..31/null names. Void incidental return excluded on both sides. The ninth original direct Base caller is nested DirectorModule and is separately bound as an unnormalized containment negative, not counted as matching. No allocator/frame/hardware or whole replay claim.'}
p = _r5_output(OUT / (stage + '-provider-closure-v1.json'))
assert not p.exists()
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print([(g['kind'], g['cases'], len(g['mismatches'])) for g in groups])
assert not any((g['mismatches'] for g in groups))