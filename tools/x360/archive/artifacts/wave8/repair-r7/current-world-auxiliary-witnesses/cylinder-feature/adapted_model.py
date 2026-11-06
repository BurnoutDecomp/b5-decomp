from pathlib import Path
import ast, hashlib, json, math, random, struct, sys
source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/replay_base_sol_r3/validate_construct.py')
nodes = []
for n in ast.parse(source.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'stage' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
OUT = Path(__file__).resolve().parent
stage = sys.argv[1]
row = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r7\\current-world-auxiliary-witnesses\\cylinder-feature\\actual-row-selection.json').read_text())['results'][0]
obj = PackedCoff(Path(row['object']), '?GetMaximumFeature@GPCylinder@')
bundle = PackedBundle([obj])
bundle.select('?GetMaximumFeature@GPCylinder@')
bits = lambda f: struct.unpack('>I', struct.pack('>f', f))[0]
f32 = lambda f: struct.unpack('>f', struct.pack('>f', f))[0]
frombits = lambda n: struct.unpack('>f', struct.pack('>I', n))[0]
labels = {2193286380: 'face', 2193286432: 'negative_cap_flip', 2193286864: 'face_descending_edge', 2193286900: 'face_ascending_edge', 2193286976: 'face_root_estimate', 2193287004: 'face_root_first_refine', 2193287028: 'face_root_second_refine', 2193287040: 'face_reciprocal_estimate', 2193287084: 'face_dir_first_refine', 2193287112: 'face_dir_second_refine', 2193287088: 'face_plane_first_refine', 2193287132: 'face_plane_second_refine', 2193287208: 'point', 2193287264: 'point_root_estimate', 2193287280: 'point_root_first_refine', 2193287296: 'point_root_second_refine', 2193287344: 'negative_point_flip', 2193287396: 'side', 2193287480: 'side_direction_root', 2193287496: 'side_direction_first_refine', 2193287516: 'side_direction_second_refine', 2193287564: 'side_length_root', 2193287584: 'side_length_first_refine', 2193287604: 'side_length_second_refine', 2193287628: 'side_down_root', 2193287672: 'side_down_first_refine', 2193287688: 'side_down_second_refine', 2193287720: 'common_tail', 2193287808: 'edge_world_transform'}
rng = random.Random(2193286192)
mismatches = []
exceptions = []
records = []
coverage = {}
counts = {}
for index in range(640):
    kind = index % 16
    position = [f32(rng.uniform(-100, 100)) for _ in range(4)]
    frame = [[1.0, 0.0, 0.0, f32(rng.uniform(-3, 3))], [0.0, 1.0, 0.0, f32(rng.uniform(-3, 3))], [0.0, 0.0, 1.0, f32(rng.uniform(-3, 3))]]
    dimensions = [f32(rng.uniform(0.1, 6)), f32(rng.uniform(0.1, 5)), f32(rng.uniform(-4, 4)), f32(rng.uniform(-4, 4))]
    direction = [0.8, 0.3, 0.5, f32(rng.uniform(-4, 4))]
    if kind == 0:
        direction[:3] = [0.0, 0.0, 1.0]
    elif kind == 1:
        direction[:3] = [0.0, 0.0, -1.0]
    elif kind == 2:
        direction[:3] = [1.0, 0.0, 0.0]
    elif kind == 3:
        direction[:3] = [-0.7, 0.5, -0.01]
    elif kind == 4:
        direction[:3] = [0.8, -0.3, -0.5]
    elif kind == 5:
        direction[:3] = [0.7, 0.2, frombits(1065349861 + index // 16 % 3 - 1)]
    elif kind == 6:
        direction[:3] = [0.7, 0.2, frombits(1017370378 + index // 16 % 3 - 1)]
    elif kind == 7:
        direction[:3] = [0.0, 0.0, 0.0]
    elif kind == 8:
        direction[:3] = [0.0, 0.0, 1.0]
        dimensions[1] = 0.0
    elif kind == 9:
        direction[:3] = [0.5, 1.0, 0.0]
        dimensions[0] = 0.0
    elif kind == 10:
        direction[:3] = [0.0, 0.0, -1.0]
        dimensions[0] *= -1.0
        dimensions[1] *= -1.0
    elif kind == 11:
        direction[:3] = [0.0, 0.0, -0.5]
    elif kind == 12:
        frame = [[-0.8, 0.6, 0.0, 1.2], [-0.6, -0.8, 0.0, -0.7], [0.0, 0.0, 1.0, 0.3]]
        direction[:3] = [0.6, 0.8, 0.0]
    elif kind == 13:
        frame = [[f32(rng.uniform(-1, 1)) for _ in range(4)] for _ in range(3)]
    elif kind == 14:
        direction[:3] = [0.8, -0.3, 0.5]
        dimensions[1] = 1e-20
    elif kind == 15:
        direction[:3] = [-0.0, 0.0, -0.0]
        dimensions[0] = -0.0
    if index >= 512:
        value = frombits([2143363909, 4291117857, 2139095040, 4286578688, 1, 2147483649][index // 16 % 6])
        group = index // 6 % 4
        if group == 0:
            position[index // 4 % 4] = value
        elif group == 1:
            direction[index // 4 % 4] = value
        elif group == 2:
            dimensions[index // 4 % 4] = value
        else:
            frame[index // 4 % 3][index % 4] = value
    gp, feature = (1459618048, 1476395264)
    image = bytearray(rng.randbytes(512))
    initial = rng.randbytes(1280)
    for offset, v in [(0, position), (16, frame[2]), (32, frame[1]), (48, frame[0]), (64, frame[2]), (112, dimensions)]:
        struct.pack_into('>4f', image, 256 + offset, *v)
    ccw = [0, 1, 2, 4294967295][index // 3 % 4]
    outputs = []
    errors = []
    for compiled in [False, True]:
        c = cpu(bundle)
        c.mem.add(gp - 256, len(image), image)
        c.mem.add(feature - 256, len(initial), initial)
        c.mem.wr(F.M.S.STACK, b'\xa5' * 131072)

        def copy(s):
            d, a, n = (s.g(r) & 4294967295 for r in [3, 4, 5])
            s.mem.wr(d, s.mem.rd(a, n))
            s.setg(3, d)
        c.hooks[2193663536] = copy
        for at, name in bundle.unresolved.items():
            if name == 'memcpy':
                c.hooks[at] = copy

        class Observe(type(c)):

            def fetch(s, pc):
                word = super().fetch(pc)
                if not compiled:
                    if pc in labels:
                        label = labels[pc]
                        coverage[label] = coverage.get(label, 0) + 1
                    if pc == 2193287204 and s.cr[6] & 1:
                        coverage['unordered_side_selection'] = coverage.get('unordered_side_selection', 0) + 1
                return word
        c.__class__ = Observe
        c.setg(3, gp)
        c.setg(4, ccw)
        c.setg(5, feature)
        c.vr[1] = tuple(map(bits, direction))
        try:
            c.run(bundle.entry if compiled else 2193286192)
            outputs.append((c.mem.rd(feature - 256, len(initial)), c.mem.rd(gp - 256, len(image))))
        except Exception as e:
            errors.append({'compiled': compiled, 'error': str(e), 'pc': hex(getattr(c, 'last_pc', 0))})
    if errors:
        exceptions.append({'case': index, 'kind': kind, 'errors': errors})
        continue
    count = int.from_bytes(outputs[0][0][816:820], 'big')
    counts[count] = counts.get(count, 0) + 1
    if outputs[0] != outputs[1]:
        mismatches.append({'case': index, 'kind': kind, 'count': count, 'ccw': hex(ccw), 'offsets': [hex(i - 256) for i, (a, b) in enumerate(zip(outputs[0][0], outputs[1][0])) if a != b], 'gp': image.hex(), 'direction_words': list(map(hex, map(bits, direction))), 'initial': initial.hex(), 'original': outputs[0][0].hex(), 'compiled': outputs[1][0].hex()})
    records.append({'case': index, 'kind': kind, 'count': count, 'ccw': hex(ccw), 'original_sha256': hashlib.sha256(outputs[0][0]).hexdigest(), 'compiled_sha256': hashlib.sha256(outputs[1][0]).hexdigest()})
report = {'stage': stage, 'requested': 640, 'compared': len(records), 'feature_counts': counts, 'mismatches': mismatches, 'model_exceptions': exceptions, 'cases': records, 'native_branch_coverage': coverage, 'object': str(obj.path), 'object_sha256': hashlib.sha256(obj.path.read_bytes()).hexdigest(), 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'limits': 'Actual whole GPCylinder CPP, original440-instruction function and compiled body. Same retail r3/r4/r5/v1; complete Feature/GP bytes and guards including all four lanes. Standard memcpy is controlled byte service. UniformA5 stack poison controls the genuinely copied unstaged side pn row, then both real functions transform it; arbitrary prior stack payload is not certified. Shared software VMX estimates/rounding used, denormal/hardware fidelity not claimed. Nonfinite exceptions recorded as model exclusions, never product passes. No global collision/provider union, GPU/frame/gameplay or launch claim.'}
p = _r5_output(OUT / (stage + '-feature-diagnostic-v1.json'))
assert not p.exists()
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print('compared', len(records), 'counts', counts, 'mismatches', len(mismatches), 'exceptions', len(exceptions))
print('first bad', [(x['case'], x['kind'], x['count'], x['offsets'][:20]) for x in mismatches[:7]])
print('first exceptions', exceptions[:2])
print('coverage', coverage)