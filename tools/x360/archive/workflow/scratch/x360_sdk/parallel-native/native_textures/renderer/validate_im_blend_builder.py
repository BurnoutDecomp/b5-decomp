from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
import native_atomic_fixture as F

out = parent / 'b5-decomp/build/x360/parallel/native_textures/state-callers-final1'
core = parent / 'b5-decomp/build/x360/parallel/native_textures/state-provider-core2'

def objects(folder):
    return {Path(r['source']).name: Path(r['object'])
            for r in json.loads((folder / 'compile-results.json').read_text())['results']
            if r['passed']}

paths = objects(out)
providers = objects(core)
used = [paths['CgsImRendererBlendState.cpp'], providers['blendstate.cpp'],
        providers['BaseResourceDescriptor.cpp']]
bundle = F.Bundle([F.Coff(used[0], '?ConstructBlendState@'),
                   F.Coff(used[1], '?Initialize@BlendState@'),
                   F.Coff(used[2], '??0BaseResourceDescriptor@')])
bundle.select('?ConstructBlendState@')
assert not bundle.unresolved, bundle.unresolved
descriptor = F.address(bundle, '?GetResourceDescriptor@BlendState@')
initialize = F.address(bundle, '?Initialize@BlendState@')
AUX = F.M.S.AUX
rng = random.Random(0x827ed118)
errors = []
canonical = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 14, 15, 16]

for i in range(384):
    inputs = ([canonical[i % len(canonical)],
               canonical[(i // len(canonical)) % len(canonical)], [0, 1, 4][i % 3]]
              if i < 192 else [rng.getrandbits(32) for _ in range(3)])
    initial = rng.randbytes(0x80)
    resource = struct.pack('>5I', AUX + 0x8000, rng.getrandbits(32),
                           rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32))
    both = []
    for compiled in [False, True]:
        c = F.cpu(bundle)
        c.mem.wr(AUX + 0x8000, initial)
        F.put(c, AUX + 0x1000, AUX + 0x2000)
        F.put(c, AUX + 0x2010, 0x84803000)
        c.setg(3, AUX + 0x1000)
        for reg, value in enumerate(inputs, 4):
            c.setg(reg, value)
        sp = c.g(1) & 0xffffffff
        c.mem.wr(sp - 0x2000, bytes([i & 255]) * 0x2000)
        trace = []

        def allocate(x):
            trace.append(('allocate', x.g(4) & 0xffffffff,
                          x.mem.rd(x.g(5) & 0xffffffff, 40).hex(),
                          x.g(6) & 0xffffffff))
            x.mem.wr(x.g(3) & 0xffffffff, resource)

        def observe(pc, word):
            if pc == (descriptor if compiled else 0x82b636b8):
                trace.append(('descriptor', c.mem.rd(c.g(4) & 0xffffffff, 0x38).hex()))
            if pc == (initialize if compiled else 0x82b627c8):
                trace.append(('initialize', c.mem.rd(c.g(3) & 0xffffffff, 20).hex(),
                              c.mem.rd(c.g(4) & 0xffffffff, 0x38).hex()))

        c.hooks[0x84803000] = allocate
        c.instruction_observer = observe
        c.run(bundle.entry if compiled else 0x827ed118)
        assert [t[0] for t in trace] == ['descriptor', 'allocate', 'initialize'], trace
        both.append((c.g(3) & 0xffffffff, c.mem.rd(AUX + 0x8000, 0x80), trace))
    if both[0] != both[1]:
        errors.append(dict(case=i, inputs=inputs, returns=[v[0] for v in both],
                           output_differences=[hex(n) for n, (a, b) in
                                               enumerate(zip(both[0][1], both[1][1])) if a != b],
                           traces=[v[2] for v in both]))

result = dict(cases=384, errors=errors,
              objects=[dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in used],
              scope='Whole actual four-argument ImRendererBase::ConstructBlendState versus ARTIST827ED118. '
                    'Actual BlendState descriptor/Initialize and BaseResourceDescriptor constructors execute. '
                    'All parameter/padding, descriptor/resource lanes, allocator receiver/flags, state bytes, return '
                    'and call order compare for SDK enum combinations and full-width noncanonical inputs.',
              limits='Virtual resource allocation is a controlled service. Other factories/callers have compile '
                     'evidence only in this checkpoint; unrelated legacy failures remain explicit.')
(out / 'im-blend-builder-comparison.json').write_text(json.dumps(result, indent=2) + '\n')
print('384 whole native blend-builder cases; errors', len(errors))
print(errors[:2])
raise SystemExit(bool(errors))
