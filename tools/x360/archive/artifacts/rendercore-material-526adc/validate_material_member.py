from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
import native_atomic_fixture as F

out = Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-material-526adc/material-member1')
paths = {Path(r['source']).name: Path(r['object']) for r in json.loads((out / 'compile-results.json').read_text())['results']}
caller = F.Coff(paths['CgsMaterialTechniqueResourceType.cpp'], '?PostFixUp@MaterialTechniqueResourceType@')
getter = F.Coff(paths['blendstate.cpp'], '?GetParameters@BlendState@')
bundle = F.Bundle([caller, getter])
bundle.select('?PostFixUp@MaterialTechniqueResourceType@')
get_entry = F.address(bundle, '?GetParameters@BlendState@')
returns = [bundle.bases[(0, caller.fn[2])] + at + 4
           for at, ix, kind in caller.relocations(caller.fn[2])
           if caller.symbols[ix][0].startswith('?GetParameters@BlendState@')]
assert len(returns) == 1
get_return = returns[0]
SELF, AUX = F.M.S.SELF, F.M.S.AUX
TECH, MATERIAL, BLEND, SHADER, NAME, RESOURCE = SELF + 0x1000, AUX + 0x1000, AUX + 0x2000, AUX + 0x3000, AUX + 0x4000, AUX + 0x5000
rng = random.Random(0x828a7ec8)
errors = []

for i in range(512):
    technique = bytearray(rng.randbytes(0x80))
    state = bytearray(rng.randbytes(0x4c))
    shader = bytearray(rng.randbytes(0xa0))
    material = bytearray(rng.randbytes(0x40))
    resource = rng.randbytes(20)
    struct.pack_into('>II', technique, 0, SHADER, MATERIAL)
    flags = struct.unpack_from('>H', technique, 8)[0]
    if i % 2 == 0: struct.pack_into('>H', technique, 8, flags & ~0x19)
    struct.pack_into('>I', material, 0, BLEND)
    for channel in range(4):
        value = 0x10001 if i % 5 == 0 else [0x10001, 0x7060706, 0, 0xffffffff][(i + channel) % 4]
        struct.pack_into('>I', state, channel * 4, value)
    for word in [10, 11, 12, 13, 14, 16]:
        struct.pack_into('>I', state, word * 4, [0, 1, 2, 0x80, 0xffffffff][(i + word) % 5])
    struct.pack_into('>II', shader, 0, 0 if i % 4 == 0 else 0x81000000 + i, 0x82000000 + i * 4)
    struct.pack_into('>I', shader, 0x94, NAME)
    char = [ord('0'), ord('3'), ord('9'), ord('A'), 0x80, 0xff][i % 6]
    both = []
    for compiled in [False, True]:
        c = F.cpu(bundle)
        for address, data in [(TECH, technique), (MATERIAL, material), (BLEND, state), (SHADER, shader), (RESOURCE, resource)]:
            c.mem.wr(address, data)
        c.mem.wr(NAME, bytes([char, 0]))
        sp = c.g(1) & 0xffffffff
        c.mem.wr(sp - 0x2000, bytes([i & 255]) * 0x2000)
        c.setg(3, SELF)
        c.setg(4, TECH)
        c.setg(5, RESOURCE)
        trace = []
        destination = [None]

        def context(): return c.mem.rd(TECH, 0x80).hex()
        def observe(pc, word):
            if pc == (get_entry if compiled else 0x82b60a50):
                destination[0] = c.g(4) & 0xffffffff
                trace.append(('get_enter', c.g(3) & 0xffffffff,
                              c.mem.rd(c.g(3) & 0xffffffff, 0x4c).hex(),
                              c.mem.rd(destination[0], 0x38).hex(), context()))
            if pc == (get_return if compiled else 0x828a7fb0):
                trace.append(('get_return', c.mem.rd(destination[0], 0x38).hex(), context()))

        def has_constant(x):
            block = x.g(3) & 0xffffffff
            trace.append(('has_constant', block, F.cstring(x, x.g(4) & 0xffffffff), context()))
            assert block in [SHADER + 0x1c, SHADER + 0x2c]
            x.setg(3, bool(i & (1 if block == SHADER + 0x1c else 2)))
        def hash_value(x, bits):
            count = x.g(4) & 0xffffffff
            data = x.mem.rd(x.g(3) & 0xffffffff, count)
            trace.append(('hash' + str(bits), data.hex(), count, context()))
            value = int.from_bytes(data, 'big') ^ (0xabcd1234 if bits == 16 else 0x9876fedc) ^ (i * 0x10201)
            x.setg(3, value & 0xffffffff)

        c.hooks.update({0x827e8de0: has_constant,
                        0x828155e0: lambda x: hash_value(x, 16),
                        0x82815570: lambda x: hash_value(x, 12)})
        if compiled:
            for at, name in bundle.unresolved.items():
                if name.startswith('?HasShaderConstant@'): c.hooks[at] = has_constant
                elif name.startswith('?CalculateHash@CgsHash16@'): c.hooks[at] = lambda x: hash_value(x, 16)
                elif name.startswith('?CalculateHash@CgsHash12@'): c.hooks[at] = lambda x: hash_value(x, 12)
        c.instruction_observer = observe
        c.run(bundle.entry if compiled else 0x828a7ec8)
        assert destination[0] is not None
        assert trace[0][0] == 'get_enter' and trace[1][0] == 'get_return'
        assert trace[0][1] == BLEND, trace[0]
        both.append((c.mem.rd(TECH, 0x80), c.mem.rd(MATERIAL, 0x40), c.mem.rd(BLEND, 0x4c),
                     c.mem.rd(SHADER, 0xa0), c.mem.rd(RESOURCE, 20), trace))
    if both[0] != both[1]:
        errors.append(dict(case=i, technique_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                           traces=[v[-1] for v in both]))

result = dict(cases=512, errors=errors,
              objects={n: dict(path=str(p), sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for n, p in paths.items()},
              scope='Whole valid-resource MaterialTechniqueResourceType::PostFixUp828A7EC8 with actual compiled BlendState::GetParameters82B60A50 linked. '
                    'The real receiver is materialState[0]; every seeded/copied56byte Parameters byte including padding, all custom blend factors, '
                    'alpha-test normalization, technique flags/hashes/profile and all untouched input buffers compare against original.',
              limits='Shader constant query and pure hash services are controlled. Invalid/null resource and assertion paths are existing unrelated code, '
                     'unchanged by this two-line API spelling correction and outside the claim. No strict link, boot or GPU claim.')
report = out / 'material-member-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print('512 whole valid material caller cases; errors', len(errors))
print(errors[:1])
raise SystemExit(bool(errors))
