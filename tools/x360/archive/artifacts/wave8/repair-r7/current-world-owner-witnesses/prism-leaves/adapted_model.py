import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F
out = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\volume-baseline')
paths = [_r5_object(out / 'FeaturePrism-nan-wip.obj'), _r5_object(out / 'FeatureEdge-rebind-wip.obj')]
b = F.Bundle([F.Coff(paths[0], '?FindIntervalOverlap@'), F.Coff(paths[1], '?constrain_point@')])
rng = random.Random(2193324072)
bad = []
coverage = {}
for name, original in [('?FindIntervalOverlap@', 2193324072), ('?FindEdgePointPrism@', 2193323968), ('?FindFacePointPrism@', 2193325848)]:
    b.select(name)
    coverage[name] = 0
    for i in range(128):
        regions = [bytearray(rng.randbytes(1024)) for _ in range(6)]
        addresses = [1342177280 + k * 4096 for k in range(6)]
        if 'Interval' in name:
            for j in range(3):
                for off in range(0, 32, 4):
                    if i % 2:
                        struct.pack_into('>I', regions[j], off, rng.choice([0, 2147483648, 2139095040, 4286578688, 2143363909, 4291117857, 1065353216, 3212836864, 1]))
                    else:
                        struct.pack_into('>f', regions[j], off, rng.uniform(-5, 5))
            alias = i % 3
            regs = [addresses[alias], addresses[1], addresses[2]]
        else:
            for j in [3, 4]:
                for off in range(0, 560, 4):
                    struct.pack_into('>f', regions[j], off, rng.uniform(-2, 2))
                struct.pack_into('>I', regions[j], 0, i * 7)
                struct.pack_into('>i', regions[j], 560, 1 + i % 4 if j == 3 and 'FacePoint' in name else 1)
                for edge in range(8):
                    size = rng.uniform(0, 3)
                    struct.pack_into('>4f', regions[j], 64 + edge * 64, *[size] * 4)
            for off in range(0, 16, 4):
                struct.pack_into('>f', regions[5], off, rng.uniform(-2, 2))
            regs = addresses
        results = []
        for compiled in (False, True):
            c = F.cpu(b)
            for a, data in zip(addresses, regions):
                c.mem.add(a, 1024, data)
            for n, val in enumerate(regs):
                c.setg(3 + n, val)
            c.run(b.entry if compiled else original)
            results.append(([c.mem.rd(a, 1024) for a in addresses], c.g(3) & 4294967295))
        coverage[name] += 1
        if results[0] != results[1]:
            bad.append({'function': name, 'case': i, 'original_return': results[0][1], 'compiled_return': results[1][1], 'differing_regions': [k for k in range(6) if results[0][0][k] != results[1][0][k]]})
    print(name, 'cases', coverage[name], 'mismatches', sum((x['function'] == name for x in bad)), flush=True)
report = {'cases': coverage, 'mismatches': bad, 'object_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, 'scope': 'Actual whole FeaturePrism and FeatureEdge objects vs original leaves, finite arbitrary XYZW, interval alias modes and entire guard regions; emulator model, no full prism family claim.'}
_r5_output(out / 'featureprism-leaves-v6-special-table-nan-wip-comparison.json').write_text(json.dumps(report, indent=2))
print(bad[:5])
assert not bad