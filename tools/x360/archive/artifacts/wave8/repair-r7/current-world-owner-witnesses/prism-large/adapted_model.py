import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F

class CarryCPU(F.AtomicCPU):

    def step(self, pc, w, nxt, depth):
        if w >> 26 == 8:
            dst, src = (w >> 21 & 31, w >> 16 & 31)
            imm = w & 65535
            if imm & 32768:
                imm -= 65536
            mask = 18446744073709551615
            total = (imm & mask) + (~self.g(src) & mask) + 1
            self.xer_ca = int(total > mask)
            self.setg(dst, total & mask)
            return nxt
        if w >> 26 == 31 and w >> 1 & 1023 == 136:
            dst, lhs, rhs = (w >> 21 & 31, w >> 16 & 31, w >> 11 & 31)
            mask = 18446744073709551615
            assert hasattr(self, 'xer_ca')
            total = (~self.g(lhs) & mask) + self.g(rhs) + self.xer_ca
            self.xer_ca = int(total > mask)
            self.setg(dst, total & mask)
            assert not w & 1
            return nxt
        return super().step(pc, w, nxt, depth)
out = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\volume-baseline')
paths = [_r5_object(out / 'FeaturePrism-nan-wip.obj'), _r5_object(out / 'FeatureEdge-rebind-wip.obj')]
b = F.Bundle([F.Coff(paths[0], '?FindIntervalOverlap@'), F.Coff(paths[1], '?constrain_point@')])
rng = random.Random(2193324072)
bad = []
coverage = {}
for name, original in [('?FindEdgeEdgePrism@', 2193324200), ('?FindFaceEdgePrism@', 2193327016), ('?FindFaceFacePrism@', 2193328024), ('?FindFaceFacePrism4x3@', 2193329312), ('?FindFaceFacePrism4x4@', 2193331200)]:
    b.select(name)
    coverage[name] = 0
    for i in range(128):
        regions = [bytearray(rng.randbytes(1024)) for _ in range(6)]
        addresses = [1342177280 + k * 4096 for k in range(6)]
        if 'Interval' in name:
            for j in range(3):
                for off in range(0, 32, 4):
                    struct.pack_into('>f', regions[j], off, rng.uniform(-5, 5))
            alias = i % 3
            regs = [addresses[alias], addresses[1], addresses[2]]
        else:
            for j in [3, 4]:
                for off in range(0, 560, 4):
                    struct.pack_into('>f', regions[j], off, rng.uniform(-2, 2))
                struct.pack_into('>I', regions[j], 0, i * 7)
                struct.pack_into('>i', regions[j], 560, 1 if 'EdgeEdge' in name else 3 if '4x3' in name and j == 4 else 4)
                for edge in range(8):
                    size = rng.uniform(0, 3)
                    struct.pack_into('>4f', regions[j], 64 + edge * 64, *[size] * 4)
            for off in range(0, 16, 4):
                struct.pack_into('>f', regions[5], off, rng.uniform(-2, 2))
            regs = addresses
        results = []
        for compiled in (False, True):
            c = F.cpu(b)
            c.__class__ = CarryCPU
            for a, data in zip(addresses, regions):
                c.mem.add(a, 1024, data)
            for n, val in enumerate(regs):
                c.setg(3 + n, val)
            c.run(b.entry if compiled else original)
            results.append(([c.mem.rd(a, 1024) for a in addresses], c.g(3) & 4294967295))
        coverage[name] += 1
        if results[0] != results[1]:
            bad.append({'function': name, 'case': i, 'original_return': results[0][1], 'compiled_return': results[1][1], 'differing_regions': [k for k in range(6) if results[0][0][k] != results[1][0][k]], 'diffs': [{'region': k, 'offset': q, 'original': results[0][0][k][q:q + 4].hex(), 'compiled': results[1][0][k][q:q + 4].hex()} for k in range(6) for q in range(0, 1024, 4) if results[0][0][k][q:q + 4] != results[1][0][k][q:q + 4]][:12]})
    print(name, 'cases', coverage[name], 'mismatches', sum((x['function'] == name for x in bad)), flush=True)
report = {'cases': coverage, 'mismatches': bad, 'object_sha256': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, 'scope': 'Actual whole FeaturePrism and FeatureEdge objects vs original leaves, finite arbitrary XYZW, interval alias modes and entire guard regions; emulator model, no full prism family claim.'}
_r5_output(out / 'featureprism-large-v6-128-table-nan-wip-comparison.json').write_text(json.dumps(report, indent=2))
print(bad[:5])