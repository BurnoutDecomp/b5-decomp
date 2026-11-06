import sys, random, json, struct, hashlib
from pathlib import Path
sys.path.insert(0, 'C:\\Users\\adri1\\.codex\\worktrees\\fbaf\\BP-Decomp_Workflow\\scratch\\x360_sdk')
import native_atomic_fixture as F

class CPU(F.AtomicCPU):

    def step(self, pc, w, nxt, depth):
        if pc in getattr(self, 'rest', set()):
            raise F.CPU.run.__globals__['Return']()
        if w >> 26 == 8:
            d, a = (w >> 21 & 31, w >> 16 & 31)
            im = w & 65535
            if im & 32768:
                im -= 65536
            mask = 18446744073709551615
            v = (im & mask) + (~self.g(a) & mask) + 1
            self.xer_ca = int(v > mask)
            self.setg(d, v & mask)
            return nxt
        if w >> 26 == 31 and w >> 1 & 1023 == 136:
            d, a, r = (w >> 21 & 31, w >> 16 & 31, w >> 11 & 31)
            mask = 18446744073709551615
            v = (~self.g(a) & mask) + self.g(r) + self.xer_ca
            self.xer_ca = int(v > mask)
            self.setg(d, v & mask)
            return nxt
        if w >> 26 == 31 and w >> 1 & 1023 == 444 and w & 1:
            x = w >> 21 & 31
            d = w >> 16 & 31
            y = w >> 11 & 31
            v = self.g(x) | self.g(y)
            self.setg(d, v)
            self.setcr(0, bool(v >> 63), bool(v and (not v >> 63)), v == 0)
            return nxt
        return super().step(pc, w, nxt, depth)
out = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\parallel\\wave8_scene_terra\\volume-baseline')
obj = _r5_object(out / 'closure-selected-v1/CgsFine_wSQ1.obj')
b = F.Bundle([F.Coff(obj, '?ComputeLineTestFine@FineIntersectionTestModule@')])
b.select('?ComputeLineTestFine@FineIntersectionTestModule@')
rng = random.Random(2190245232)
bad = []
errors = []
for i in range(64):
    images = [bytearray(rng.randbytes(256)) for k in range(4)]
    struct.pack_into('>H', images[1], 40, 0)
    struct.pack_into('>H', images[1], 42, 65535)
    addresses = [1342177280 + k * 4096 for k in range(4)]
    res = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.__class__ = CPU
        c.rest = set()
        for addr, data in zip(addresses, images):
            c.mem.add(addr, 256, data)
        for addr, name in b.unresolved.items():
            if name.startswith('__save'):
                c.hooks[addr] = lambda cpu: None
            elif name.startswith('__rest'):
                c.rest.add(addr)
                c.hooks[addr] = lambda cpu: None
        for k, addr in enumerate(addresses):
            c.setg(3 + k, addr)
        try:
            c.run(b.entry if compiled else 2190245232)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'error': repr(ex)})
            break
        res.append([c.mem.rd(addr, 256) for addr in addresses])
    if len(res) == 2 and res[0] != res[1]:
        bad.append({'case': i, 'regions': [k for k in range(4) if res[0][k] != res[1][k]], 'original_out': res[0][2].hex(), 'compiled_out': res[1][2].hex()})
print('cases', 64, 'mismatches', len(bad), 'errors', len(errors), bad[:2], errors[:2])
_r5_output(out / 'fine-empty-v1-final-selected-comparison.json').write_text(json.dumps({'cases': 64 - len(errors), 'mismatches': bad, 'errors': errors, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'actual whole ComputeLineTestFine vs ARTIST 828C7D70, zero candidates/no excluded entity, entire module/query/output/array guards; no populated scene or cluster behavior claim'}, indent=2))