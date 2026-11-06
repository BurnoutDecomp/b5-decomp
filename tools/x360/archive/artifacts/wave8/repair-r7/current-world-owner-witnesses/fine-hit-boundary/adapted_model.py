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
obj = _r5_object(out / 'CgsFine-hit-v2.obj')
b = F.Bundle([F.Coff(obj, '?ComputeLineTestFine@FineIntersectionTestModule@')])
b.select('?ComputeLineTestFine@FineIntersectionTestModule@')
rng = random.Random(2190245232)
bad = []
errors = []
traceCounts = {}
M, Q, O, ARR, IDX, EM, VM, VI, V, D, LQ, ID, TV, HIT = [1342177280, 1343225856, 1343229952, 1358954496, 1343238144, 1343242240, 1343246336, 1343250432, 1343254528, 1343258624, 1343262720, 1343266816, 1343270912, 1343275008]
for i in range(64):
    module = bytearray(rng.randbytes(366848))
    query = bytearray(rng.randbytes(256))
    output = rng.randbytes(256)
    array = bytearray(rng.randbytes(16400))
    struct.pack_into('>i', array, 16384, 0)
    indices = struct.pack('>H', [0, 1, 3, 255, 256, 511, 1023, 4095][i % 8]) + rng.randbytes(30)
    em = rng.randbytes(256)
    vm = rng.randbytes(256)
    vi = bytearray(rng.randbytes(256))
    vol = bytearray(rng.randbytes(256))
    desc = bytearray(rng.randbytes(64))
    lq = bytearray(rng.randbytes(512))
    identity = bytearray(rng.randbytes(64))
    tv = bytearray(rng.randbytes(64))
    for off, ptr in [(366608, VM), (366612, EM), (366620, LQ)]:
        struct.pack_into('>I', module, off, ptr)
    for off in range(0, 32, 4):
        struct.pack_into('>f', query, off, rng.uniform(-2, 2))
    struct.pack_into('>I', query, 36, IDX)
    struct.pack_into('>H', query, 40, 1)
    struct.pack_into('>H', query, 42, 65535)
    query[44] = 1
    struct.pack_into('>i', vi, 92, 7)
    struct.pack_into('>i', vi, 96, -1)
    struct.pack_into('>I', vol, 64, D)
    struct.pack_into('>I', desc, 0, 1)
    struct.pack_into('>I', identity, 0, 305419896)
    tv[8] = 1
    hit = bytearray(rng.randbytes(208))
    struct.pack_into('>I', hit, 80, V)
    struct.pack_into('>I', lq, 16, HIT)
    images = [(HIT, hit), (M, module), (Q, query), (O, output), (ARR, array), (IDX, indices), (EM, em), (VM, vm), (VI, vi), (V, vol), (D, desc), (LQ, lq), (ID, identity), (TV, tv)]
    results = []
    for compiled in (False, True):
        c = F.cpu(b)
        c.__class__ = CPU
        c.rest = set()
        trace = []
        for addr, data in images:
            c.mem.add(addr, len(data), data)

        def entity(cpu):
            trace.append('entity')
            if compiled:
                ptr = cpu.g(3) & 4294967295
                cpu.mem.wr(ptr, struct.pack('>I', 305419896))
                cpu.setg(3, ptr)
            else:
                cpu.setg(3, ID)

        def first(cpu):
            trace.append('first')
            cpu.mem.wr(cpu.g(5) & 4294967295, struct.pack('>i', 7))
            cpu.setg(3, VI)

        def alloc(cpu):
            trace.append('alloc')
            cpu.setg(3, 1)

        def typevol(cpu):
            trace.append('type')
            cpu.setg(3, 1 if compiled else TV)

        def volume(cpu):
            trace.append('volume')
            cpu.setg(3, V)

        def nextinst(cpu):
            trace.append('next')
            cpu.setg(3, 0)

        def getall(cpu):
            trace.append('getall')
            ptr = cpu.g(3) & 4294967295
            cpu.mem.wr(ptr + 12, struct.pack('>I', 1))
            cpu.setg(3, 1)
        hooks = {'GetEntityIdByIndex': entity, 'GetFirstEntityVolumeInstance': first, 'GetVolumeTypeFlags': typevol, 'GetRwVolume': volume, 'GetVolumeInstance': nextinst, 'GetAllIntersections': getall}
        if compiled:
            for addr, name in b.unresolved.items():
                if name.startswith('__save'):
                    c.hooks[addr] = lambda cpu: None
                elif name.startswith('__rest'):
                    c.rest.add(addr)
                    c.hooks[addr] = lambda cpu: None
                else:
                    for part, hook in hooks.items():
                        if part in name:
                            c.hooks[addr] = hook
        else:
            for addr, hook in [(2190175240, entity), (2190237120, first), (2190178928, alloc), (2190178224, typevol), (2190237288, volume), (2190188328, nextinst), (2193307680, getall)]:
                c.hooks[addr] = hook
        c.setg(3, M)
        c.setg(4, Q)
        c.setg(5, O)
        c.setg(6, ARR)
        try:
            c.run(b.entry if compiled else 2190245232)
        except Exception as ex:
            errors.append({'case': i, 'compiled': compiled, 'trace': trace, 'error': repr(ex)})
            break
        state = bytearray(c.mem.rd(LQ, 512))
        state[0:8] = bytes(8)
        trace = [x for x in trace if x != 'alloc']
        results.append((c.mem.rd(O, 256), c.mem.rd(ARR, 16400), bytes(state), trace))
    if len(results) == 2:
        traceCounts[str(results[0][3])] = traceCounts.get(str(results[0][3]), 0) + 1
        if results[0] != results[1] and i == 0:
            print('ARRAY DIFF', [(j, results[0][1][j], results[1][1][j]) for j in range(len(results[0][1])) if results[0][1][j] != results[1][1][j]][:80])
        if results[0] != results[1]:
            bad.append({'case': i, 'original_trace': results[0][3], 'compiled_trace': results[1][3], 'regions': [k for k in range(3) if results[0][k] != results[1][k]]})
print('cases', 64, 'mismatches', len(bad), 'errors', len(errors), bad[:2], errors[:2], traceCounts)
_r5_output(out / 'fine-primitive-hit-boundary-v4-comparison.json').write_text(json.dumps({'requested': 64, 'mismatches': bad, 'errors': errors, 'traces': traceCounts, 'object_sha256': hashlib.sha256(obj.read_bytes()).hexdigest(), 'scope': 'one entity/primitive one-hit path; controlled entity, manager and volume-query services; stack input-array pointers normalized; complete result/array/query guards, controlled hit service; no cluster claim'}, indent=2))