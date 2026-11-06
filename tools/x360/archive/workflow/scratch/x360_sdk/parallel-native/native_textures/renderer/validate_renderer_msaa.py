from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
sys.path.insert(0, str(parent / 'tools/re'))
import native_atomic_fixture as F
import x360rd as R

out = parent / 'b5-decomp/build/x360/parallel/native_textures/renderer-msaa2'
obj = Path(json.loads((out / 'compile-results.json').read_text())['results'][0]['object'])
bundle = F.Bundle([F.Coff(obj, '?ResolveMSAA@')])
SELF, AUX = F.M.S.SELF, F.M.S.AUX
FACADES = [AUX + 0x1000 + n * 0x200 for n in range(3)]
BACKENDS = [AUX + 0x2000 + n * 0x200 for n in range(3)]
DEVICE, CACHE = 0x83271608, 0x83010a30
addresses = {'?gpD3DDevice@@': DEVICE, '?mpRenderTargetState@Device@shadow@@': CACHE,
             'KMSAA_TILING_PLAN': 0x8203e080, 'KNO_MSAA_TILING_PLAN': 0x8203e0c8}
rng = random.Random(0x823ffa18)
errors = []
checks = 0

for begin, original in [(True, 0x823ffa18), (False, 0x823ffbe0)]:
    bundle.select('?BeginRenderAntiAliased@' if begin else '?ResolveMSAA@')
    for (oi, sec), base in bundle.bases.items():
        source = bundle.objects[oi]
        body = bundle.images[base]
        for at, index, kind in source.relocations(sec):
            name = source.symbols[index][0]
            matches = [v for p, v in addresses.items() if name.startswith(p)]
            if not matches: continue
            value = matches[0]
            raw = struct.unpack_from('>I', source.data, source.sections[sec][4] + at)[0]
            word = struct.unpack_from('>I', body, at)[0]
            if kind == 0x10: word = (word & 0xffff0000) | (((value + 0x8000) >> 16) & 0xffff)
            elif kind == 0x11:
                addend = (raw & 0x7fff) - (raw & 0x8000)
                word = (word & 0xffff0000) | ((value + addend) & 0xffff)
            elif kind == 2: word = (value + raw) & 0xffffffff
            else: raise AssertionError((name, kind))
            struct.pack_into('>I', body, at, word)
    for i in range(256):
        module = rng.randbytes(0xca80)
        globals_ = rng.randbytes(0x200)
        white = struct.unpack('>f', struct.pack('>I', [0, 0x80000000, 0x3f800000, 0xbf800000,
                                                     0x00800000, 0x7f7fffff, 0x7f800000, 0x7fc12345][i % 8]))[0]
        both = []
        for compiled in [False, True]:
            c = F.cpu(bundle)
            c.mem.add(0x83010000, 0x2000)
            c.mem.add(0x83271000, 0x1000)
            c.mem.add(0x8203e000, 0x1000)
            c.mem.wr(0x8203e080, R.rd(0x8203e080, 0x90))
            c.mem.wr(SELF, module)
            c.mem.wr(0x83010900, globals_)
            F.put(c, DEVICE, AUX + 0x8000)
            F.put(c, SELF + 0x238, FACADES[0])
            F.put(c, SELF + 0x248, FACADES[1])
            for n, (facade, backend) in enumerate(zip(FACADES, BACKENDS)):
                F.put(c, facade + 0x108, backend)
                F.put(c, backend + 0x1c, AUX + 0x4000 + n * 0x100)
            F.put(c, CACHE, [0, AUX + 0x4000, AUX + 0x5000][(i // 4) % 3])
            c.mem.wr(SELF + 0xc400, bytes([[0, 1, 2, 255][i % 4]]))
            c.mem.wr(SELF + 0xc434, bytes([[0, 1, 2, 255][(i // 4) % 4]]))
            c.setg(3, SELF)
            c.setg(4, 0x91234567)
            c.setg(5, (i // 16) % 2 if begin else i)
            c.setg(6, i)
            c.setf(1, white)
            sp = c.g(1) & 0xffffffff
            c.mem.wr(sp - 0x2000, bytes([i]) * 0x2000)
            trace = []

            def context():
                return (hashlib.sha256(c.mem.rd(SELF, 0xca80)).hexdigest(),
                        c.mem.rd(CACHE, 4).hex(), c.mem.rd(DEVICE, 4).hex())
            def start(x):
                trace.append(('start', x.g(3) & 0xffffffff, context()))
                if i % 6 == 0:
                    x.mem.wr(SELF + 0xc400, bytes([0 if i % 4 else 1]))
                    F.put(x, SELF + 0x238, FACADES[2])
                if i % 6 == 1:
                    x.mem.wr(SELF + 0xc4d0, struct.pack('>4f', 0.25, 0.5, 0.75, 1.0))
                    F.put(x, DEVICE, AUX + 0x8100)
            def stop(x):
                trace.append(('stop', x.g(3) & 0xffffffff, context()))
                if i % 6 == 2: F.put(x, DEVICE, AUX + 0x8200)
            def state(x):
                trace.append(('state', x.g(3) & 0xffffffff, context()))
                if i % 6 == 3:
                    F.put(x, CACHE, 0x12345678)
                    F.put(x, DEVICE, AUX + 0x8300)
            def begin_tiling(x):
                count = x.g(5) & 0xffffffff
                trace.append(('begin_tiling', [x.g(n) & 0xffffffff for n in range(3, 7)],
                              x.mem.rd(x.g(6) & 0xffffffff, count * 16).hex(),
                              x.mem.rd(x.g(7) & 0xffffffff, 16).hex(),
                              struct.pack('>f', x.f(1)).hex(), x.g(9) & 0xffffffff, context()))
                if i % 6 == 4: F.put(x, SELF + 0xc9c4, 0x70000000 + i)
            def predication(x):
                trace.append(('predication', x.g(3) & 0xffffffff, x.g(4) & 0xffffffff, context()))
                if i % 6 == 5:
                    F.put(x, SELF + 0x248, FACADES[2])
                    F.put(x, DEVICE, AUX + 0x8400)
            def get_texture(x, depth=False):
                facade = x.g(3) & 0xffffffff
                trace.append(('get_depth' if depth else 'get_color', facade,
                              None if depth else x.g(4) & 0xffffffff, context()))
                x.setg(3, 0 if i % 17 == 0 else AUX + 0x6000 + (facade - FACADES[0]) + (0 if depth else 0x100))
                if i % 6 == 2 and depth: F.put(x, SELF + 0x248, FACADES[0])
            def resolve(x):
                stack = x.g(1) & 0xffffffff
                clear = x.g(10) & 0xffffffff
                assert (x.g(7) & 3) == 0
                args = [x.g(n) & 0xffffffff for n in range(3, 10)]
                args[4] = 'stack-point'
                trace.append(('resolve', args,
                              x.mem.rd(x.g(5) & 0xffffffff, 16).hex(),
                              x.mem.rd(x.g(7) & 0xffffffff, 8).hex(),
                              x.mem.rd(clear, 16).hex() if clear else None,
                              struct.pack('>f', x.f(1)).hex(), x.mem.rd(stack + 0x5c, 4).hex(),
                              x.mem.rd(stack + 0x64, 4).hex(), context()))
                if i % 6 == 3:
                    F.put(x, SELF + 0x248, FACADES[2])
                    F.put(x, SELF + 0xc9e4, 0x71000000 + i)
                    x.mem.wr(SELF + 0xc4d0, bytes([0x42]) * 16)
            def end_tiling(x):
                trace.append(('end_tiling', [x.g(n) & 0xffffffff for n in [3, 4, 5, 6, 7, 9, 10]],
                              struct.pack('>f', x.f(1)).hex(), context()))

            original_hooks = {0x82825e78: start, 0x82826098: stop, 0x82b61b38: state,
                              }
            # Bind SDK identities from each exported caller rather than assuming database labels.
            export = json.loads((Path(r'D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX') /
                                 (f'0x{original:08X}.json')).read_text())
            named = {v['name']: int(v['address'], 16) for v in export['xrefs_from']}
            for name, handler in [('D3DDevice_BeginTiling', begin_tiling), ('D3DDevice_SetPredication', predication),
                                  ('D3DDevice_EndTiling', end_tiling), ('D3DDevice_Resolve', resolve),
                                  ('CgsRenderTarget::GetDepthTexture', lambda x: get_texture(x, True)),
                                  ('CgsRenderTarget::GetTexture', get_texture)]:
                if name in named: original_hooks[named[name]] = handler
            c.hooks.update(original_hooks)
            if compiled:
                for at, name in bundle.unresolved.items():
                    if name.startswith('?StartMonitor@'): c.hooks[at] = start
                    elif name.startswith('?StopMonitor@'): c.hooks[at] = stop
                    elif name.startswith('?SetState@Device@renderengine@@'): c.hooks[at] = state
                    elif name.startswith('?GetDepthTexture@'): c.hooks[at] = lambda x: get_texture(x, True)
                    elif name.startswith('?GetTexture@'): c.hooks[at] = get_texture
                    elif name == 'D3DDevice_BeginTiling': c.hooks[at] = begin_tiling
                    elif name == 'D3DDevice_SetPredication': c.hooks[at] = predication
                    elif name == 'D3DDevice_Resolve': c.hooks[at] = resolve
                    elif name == 'D3DDevice_EndTiling': c.hooks[at] = end_tiling
            c.run(bundle.entry if compiled else original)
            both.append((c.mem.rd(SELF, 0xca80), c.mem.rd(0x83010900, 0x200), c.mem.rd(DEVICE, 4), trace))
        checks += 1
        if both[0] != both[1]:
            errors.append(dict(method='Begin' if begin else 'Resolve', case=i,
                               module_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                               global_differences=[hex(0x83010900 + n) for n, (a, b) in enumerate(zip(both[0][1], both[1][1])) if a != b],
                               traces=[v[3] for v in both]))

result = dict(cases=checks, errors=errors, object=str(obj), object_sha256=hashlib.sha256(obj.read_bytes()).hexdigest(),
              scope='256 whole BeginRenderAntiAliased and256 whole ResolveMSAA cases, complete Renderer/cache/device outputs, '
                    'both MSAA/grey branches, target-cache branches, SDK plan/point/color/flags/stencil argument ABI, '
                    'predication and monitor order, saved device/background and late producer mutations.',
              limits='SDK GPU calls, monitor APIs, Device render-target application and CgsRenderTarget texture getters are controlled. '
                     'The true compiled inline render-target cache logic executes. No real GPU/frame, strict link or boot claim.')
report = out / 'renderer-msaa-corrected-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed comparison evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print(checks, 'whole Renderer MSAA cases; errors', len(errors))
print(errors[:1])
raise SystemExit(bool(errors))
