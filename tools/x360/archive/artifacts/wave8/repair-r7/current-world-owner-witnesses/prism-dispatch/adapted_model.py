import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F

class CarryCPU(F.AtomicCPU):

    def step(self, pc, w, nxt, depth):
        if pc in getattr(self, 'target_map', {}):
            self.worker_trace.append((self.target_map[pc], [self.g(r) & 4294967295 for r in range(3, 9)]))
            self.setg(3, 291)
            raise F.CPU.run.__globals__['Return']()
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
b = F.Bundle([F.Coff(paths[0], '?FindFeatureIntersectionPrism@'), F.Coff(paths[1], '?constrain_point@')])
b.select('?FindFeatureIntersectionPrism@')
rng = random.Random(2193333208)
bad = []
coverage = {}
skipped = []
worker_names = [('FindEdgePointPrism', 2193323968), ('FindFacePointPrism', 2193325848), ('FindEdgeEdgePrism', 2193324200), ('FindFaceEdgePrism', 2193327016), ('FindFaceFacePrism', 2193328024), ('FindFaceFacePrism4x3', 2193329312), ('FindFaceFacePrism4x4', 2193331200)]
for count1 in (0, 1, 2, 3, 4):
    for count2 in (0, 1, 2, 3, 4):
        key = f'{count1},{count2}'
        coverage[key] = 0
        for i in range(8):
            regions = [bytearray(rng.randbytes(1024)) for _ in range(4)]
            addresses = [1342177280 + k * 4096 for k in range(4)]
            for j, count in [(1, count1), (2, count2)]:
                for off in range(0, 560, 4):
                    struct.pack_into('>f', regions[j], off, rng.uniform(-2, 2))
                struct.pack_into('>I', regions[j], 0, i * 7)
                struct.pack_into('>i', regions[j], 560, count)
                for edge in range(8):
                    length = rng.uniform(0, 3)
                    struct.pack_into('>4f', regions[j], 64 + edge * 64, *[length] * 4)
            for off in range(0, 16, 4):
                struct.pack_into('>f', regions[3], off, rng.uniform(-2, 2))
            results = []
            for compiled in (False, True):
                c = F.cpu(b)
                c.__class__ = CarryCPU
                c.worker_trace = []
                c.target_map = {F.address(b, '?' + n + '@') if compiled else address: n for n, address in worker_names}
                for addr, data in zip(addresses, regions):
                    c.mem.add(addr, 1024, data)
                c.mem.add(2200432576, 16, bytes.fromhex('34000000') * 4)
                for r, addr in enumerate(addresses):
                    c.setg(3 + r, addr)
                try:
                    c.run(b.entry if compiled else 2193333208)
                except AssertionError as ex:
                    skipped.append({'counts': key, 'case': i, 'compiled': compiled, 'error': str(ex)})
                    break
                results.append(([c.mem.rd(addr, 1024) for addr in addresses], c.g(3) & 4294967295, c.worker_trace))
            if len(results) < 2:
                continue
            coverage[key] += 1
            if results[0] != results[1]:
                bad.append({'counts': key, 'case': i, 'original_return': results[0][1], 'compiled_return': results[1][1], 'regions': [k for k in range(4) if results[0][0][k] != results[1][0][k]], 'original_trace': results[0][2], 'compiled_trace': results[1][2]})
print('coverage', coverage, 'mismatches', len(bad), bad[:8])
report = {'cases': coverage, 'mismatches': bad, 'skipped': skipped, 'objects': {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in paths}, 'scope': 'whole native dispatcher vs ARTIST 82BB9BD8, controlled workers capture names and all r3-r8; finite arbitrary inputs, point/edge/3/4 counts, poisoned guards, initialized epsilon; emulator estimate/carry limits'}
_r5_output(out / 'featureprism-dispatch-controlled-v6-table-nan-wip-comparison.json').write_text(json.dumps(report, indent=2))