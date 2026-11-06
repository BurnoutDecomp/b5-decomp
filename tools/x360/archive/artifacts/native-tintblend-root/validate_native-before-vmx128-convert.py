from pathlib import Path
import hashlib, inspect, json, math, random, struct, sys

HERE = Path(__file__).resolve().parent
PARENT = Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(PARENT / 'scratch/x360_sdk'))
mkdir = Path.mkdir
Path.mkdir = lambda *a, **k: None
try:
    import native_atomic_fixture as F
finally:
    Path.mkdir = mkdir

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def f32(v):
    return struct.unpack('>f', struct.pack('>f', v))[0]

def fbits(v):
    return struct.unpack('>I', struct.pack('>f', v))[0]

class TintCPU(F.AtomicCPU):
    def exec_vector(self, pc, word, mnemonic):
        # Classic VMX unsigned fixed-point conversions. These inputs originate
        # in byte gathers. Exceptional FP conversion/status is not claimed.
        if word >> 26 == 4 and (word & 0x7FF) in (778, 906):
            dest, src, shift = (word >> 21) & 31, (word >> 11) & 31, (word >> 16) & 31
            if (word & 0x7FF) == 778:
                self.vr[dest] = tuple(fbits(f32(value / (1 << shift))) for value in self.vr[src])
                self.convert_counts[0] += 1
            else:
                result = []
                for value in self.vf(src):
                    assert math.isfinite(float(value)), ('unclaimed exceptional conversion', pc, value)
                    scaled = math.ldexp(float(value), shift)
                    self.tint_sat |= scaled < 0 or scaled > 0xFFFFFFFF
                    result.append(max(0, min(0xFFFFFFFF, math.trunc(scaled))))
                self.vr[dest] = tuple(result)
                self.convert_counts[1] += 1
            return True
        return super().exec_vector(pc, word, mnemonic)

def cpu(obj):
    c = F.cpu(obj)
    c.__class__ = TintCPU
    c.tint_sat = False
    c.convert_counts = [0, 0]
    return c

assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
r = json.loads((HERE / 'native/compile-results.json').read_text())
path = Path(r['results'][0]['object'])
SPECS = [(2, 0x82AD2CF0, '?VMXBlend@'), (3, 0x82AD2F38, '?sub_82AD2F38@'),
         (4, 0x82AD3250, '?sub_82AD3250@'), (5, 0x82AD3668, '?sub_82AD3668@'),
         (6, 0x82AD3AF8, '?sub_82AD3AF8@')]
rng = random.Random(0x82AD4860)
BASE, DEST = 0x45000000, 0x45000100
errors, groups, details = [], {}, []
per_leaf = int(sys.argv[1]) if len(sys.argv) > 1 else 8
output_name = sys.argv[2] if len(sys.argv) > 2 else 'leaf-smoke.json'
for count, original, prefix in SPECS:
    obj = F.Coff(path, prefix)
    for index in range(per_leaf):
        initial = rng.randbytes(0x2000)
        weights = []
        for source in range(count):
            if index % 8 == 0:
                w = [1.0 if source == 0 else 0.0] * 4
            elif index % 8 == 1:
                w = [1.0] * 4
            elif index % 8 == 2:
                w = [0.0] * 4
            elif index % 8 == 3:
                w = [-1.0 if source == 0 else 0.0] * 4
            elif index % 8 == 4:
                w = [f32(rng.uniform(-2.0, 4.0))] * 4
            else:
                w = [f32(rng.uniform(-2.0, 4.0)) for _ in range(4)]
            weights.append(w)
        result = []
        for compiled in (False, True):
            c = cpu(obj)
            c.mem.add(BASE, len(initial), initial)
            c.setg(3, DEST + index % 16)
            for source in range(count):
                c.setg(4 + source, BASE + 0x400 + source * 0x100 + index % 16)
                c.setvf(source + 1, weights[source])
            c.run(obj.entry if compiled else original)
            result.append((c.mem.rd(BASE, len(initial)), c.tint_sat, c.convert_counts))
        if result[0] != result[1]:
            errors.append({'source_count': count, 'index': index, 'weights': weights,
                           'original_destination': result[0][0][0xF0:0x160].hex(),
                           'compiled_destination': result[1][0][0xF0:0x160].hex(),
                           'different_bytes': [i for i, (a, b) in enumerate(zip(result[0][0], result[1][0])) if a != b],
                           'original_sat_counts': result[0][1:], 'compiled_sat_counts': result[1][1:]})
    groups[str(count)] = per_leaf
    details.append({'count': count, 'original': hex(original), 'compiled_symbol': obj.fn[0],
                    'original_raw_sha256': hashlib.sha256(F.M.V.x360rd.rd(original,
                        {2: 0x248, 3: 0x318, 4: 0x418, 5: 0x490, 6: 0x580}[count])).hexdigest()})
    print('leaf', count, per_leaf, 'checked', flush=True)
result = {'object': str(path), 'object_sha256': sha(path), 'cases': sum(groups.values()),
          'groups': groups, 'mismatches': errors, 'functions': details,
          'manual': 'https://www.nxp.com/docs/en/reference-manual/ALTIVECPOWERISAPEM.pdf',
          'private_model': 'vcfux: nearest f32 unsigned-word /2^shift; vctuxs: scale2^shift then truncation and unsigned32 saturation. Finite values only; private SAT observation is not complete VSCR/hardware modeling. Shared fixture unchanged.',
          'verifier_sha256': sha(__file__),
          'limits': 'Actual original and compiled PPC helper instructions, unchanged retail GPR/VR inputs. Includes lane-varying finite weights, negative/zero/overflow-byte sums and unaligned vector-load/store addresses. Every destination/source/guard byte and conversion count compared; no source/destination alias case, physical FP status, exceptional weights, full job or GPU/game claim.'}
with (HERE / output_name).open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}, indent=2))
assert not errors
