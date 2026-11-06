"""Read-only complete256-sample IIR rounding comparison; optional argv[1]=actual obj.
PPC FP arithmetic/state/output execute. Compiled nonvolatile FP save/restore CRT
helpers are controlled. No report, mkdir, bytecode, hardware/audio claims.
"""
import sys, struct, random, json, hashlib, re
from pathlib import Path
sys.dont_write_bytecode = True
old_mkdir, old_open = Path.mkdir, Path.open
Path.mkdir = lambda *a, **kw: None
def readonly(self, mode='r', *a, **kw):
    assert not any(c in mode for c in 'wax+'), ('fixture write blocked', str(self), mode)
    return old_open(self, mode, *a, **kw)
Path.open = readonly
try:
    sys.path.insert(0, 'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk')
    import native_atomic_fixture as F
    root = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7')
    rows = json.loads((root/'full-stage1/compile-results.json').read_text())['results']
    default = Path(next(x['object'] for x in rows if x['source'].endswith('/Iir2.cpp')))
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else default
    obj = F.Coff(path, '?Filter@Iir2@core@audio@rw@@')
    BASE = 0x45000000
    def execute(state, coeff, samples, alias):
        pair = []
        for compiled in [False, True]:
            c = F.cpu(obj)
            c.mem.add(BASE, 0x5000)
            c.mem.wr(BASE, state)
            c.mem.wr(BASE + 0x100, coeff)
            c.mem.wr(BASE + 0x1000, samples)
            destination = BASE + (0x1000 if alias else 0x2000)
            c.setg(3, BASE)
            c.setg(4, destination)
            c.setg(5, BASE + 0x1000)
            c.setg(6, BASE + 0x100)
            saved = {}
            def save(cpu, start):
                saved.update({k: cpu.f(k) for k in range(start, 32)})
            def restore(cpu, start):
                for k in range(start, 32):
                    cpu.setf(k, saved[k])
            for address, name in obj.unresolved.items():
                match = re.fullmatch(r'__savefpr_(\d+)', name)
                if match:
                    c.hooks[address] = lambda cpu, start=int(match.group(1)): save(cpu, start)
                match = re.fullmatch(r'__restfpr_(\d+)', name)
                if match:
                    c.hooks[address] = lambda cpu, start=int(match.group(1)): restore(cpu, start)
            c.run(obj.entry if compiled else 0x82b69dd0)
            pair.append((c.mem.rd(BASE, 0x5000), c.mem.rd(destination, 4).hex()))
        return pair
    val = lambda text: struct.unpack('>f', bytes.fromhex(text))[0]
    bits = lambda x: struct.pack('>f', x).hex()
    f32 = lambda x: struct.unpack('>f', struct.pack('>f', x))[0]
    x0, x2, c2, c4 = [val(x) for x in ['bf51fbe4', '3e120c28', '3ee8f562', 'bf47c4a9']]
    special = execute(struct.pack('>4f', 0, x2, 0, 0),
                      struct.pack('>5f', 0, 0, c2, 0, c4),
                      struct.pack('>256f', x0, *([0] * 255)), False)
    reference = bits(f32(x2 * c4 + f32(x0 * c2)))
    reversed_reference = bits(f32(x0 * c2 + f32(x2 * c4)))
    assert special[0][1] == reference == 'bef8115f'
    assert reversed_reference == 'bef81160'
    rng = random.Random(0x82b69dd0)
    mismatches = []
    for index in range(64):
        state = struct.pack('>4f', *(rng.uniform(-.5, .5) for _ in range(4)))
        coeff = struct.pack('>5f', *(rng.uniform(-.2, .2) for _ in range(5)))
        samples = struct.pack('>256f', *(rng.uniform(-1, 1) for _ in range(256)))
        pair = execute(state, coeff, samples, bool(index % 2))
        if pair[0][0] != pair[1][0]:
            mismatches.append({'case': index, 'inplace': bool(index % 2),
                               'first_differences': [hex(n) for n, (a, b) in enumerate(zip(pair[0][0], pair[1][0])) if a != b][:12]})
    result = {'object': str(path), 'object_sha256': hashlib.sha256(path.read_bytes()).hexdigest(),
              'first_sample_rounding_witness': {
                  'input_bits': {'x0': bits(x0), 'history_x2': bits(x2), 'B0': bits(c2), 'B2': bits(c4),
                                 'x1_y1_y2_A1_A2_B1': '00000000'},
                  'remaining_samples': 255, 'remaining_samples_value': 0,
                  'original_first_output': special[0][1], 'compiled_first_output': special[1][1],
                  'original_reference': reference, 'reversed_rounding_reference': reversed_reference,
                  'equal': special[0][0] == special[1][0]},
              'random_blocks': 64, 'inplace_blocks': 32, 'samples_per_block': 256,
              'rng_seed': '0x82b69dd0', 'mismatch_count': len(mismatches), 'mismatches': mismatches,
              'scope': 'Complete original82B69DD0 and actual compiled fixed256 Filter; finite stable coefficients/history/input, mapped state/source/destination bytes; compiled nonvolatile FP save/restore controlled. First witness independently matches the exact normal two-product rounding references. No exceptional-FP, hardware or audio lifecycle claim.'}
finally:
    Path.mkdir, Path.open = old_mkdir, old_open
print(json.dumps(result, indent=2))
