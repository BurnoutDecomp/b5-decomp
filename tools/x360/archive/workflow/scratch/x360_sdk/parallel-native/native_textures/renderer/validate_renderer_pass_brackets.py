from pathlib import Path
import hashlib
import json
import random
import struct
import sys

parent = Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow')
sys.path.insert(0, str(parent / 'scratch/x360_sdk'))
import native_atomic_fixture as F

out = parent / 'b5-decomp/build/x360/parallel/native_textures/renderer-pass-brackets2'
path = Path(json.loads((out / 'compile-results.json').read_text())['results'][0]['object'])
bundle = F.Bundle([F.Coff(path, '?EndRenderAntiAliased@')])
SELF, AUX = F.M.S.SELF, F.M.S.AUX
FACADES = [AUX + 0x1000 + n * 0x200 for n in range(3)]
BACKENDS = [AUX + 0x2000 + n * 0x200 for n in range(3)]
addresses = {'?saDepthStencilStates@': 0x8301090c, '?saRasterizerStates@': 0x83010a38,
             '?saBlendStates@': 0x83010f70}
specs = [('EndRenderPostFx', 0x823f65b0, [1, 2, 0]),
         ('EndRenderEnvironmentMapFace', 0x823fc5e8, None),
         ('EndRenderAntiAliased', 0x82408b00, [1, 2, 0]),
         ('BeginRenderEnvironmentMapFace', 0x823f63e0, [4, 1, 0]),
         ('BeginQuarterResBuffer', 0x82408c38, [3, 2, 7])]
rng = random.Random(0x82408b00)
errors = []
checks = 0

def rebase():
    for (oi, sec), base in bundle.bases.items():
        obj = bundle.objects[oi]
        body = bundle.images[base]
        for at, index, kind in obj.relocations(sec):
            name = obj.symbols[index][0]
            values = [v for p, v in addresses.items() if name.startswith(p)]
            if not values: continue
            value = values[0]
            word = struct.unpack_from('>I', body, at)[0]
            if kind in [0x10, 0x11]:
                word = (word & 0xffff0000) | (((value + 0x8000) >> 16 if kind == 0x10 else value) & 0xffff)
            elif kind == 2: word = value
            else: raise AssertionError((name, kind))
            struct.pack_into('>I', body, at, word)

def link_native(c, at, target):
    c.hooks.pop(at, None)
    # A linkage-only trampoline to the genuine original setter. No branch/body is substituted.
    c.mem.wr(at, struct.pack('>4I', 0x3d800000 | ((target + 0x8000) >> 16),
                            0x398c0000 | (target & 0xffff), 0x7d8903a6, 0x4e800420))

for method, original, slots in specs:
    bundle.select('?' + method + '@')
    rebase()
    for i in range(256):
        module = rng.randbytes(0xca80)
        globals_ = rng.randbytes(0x1200)
        face = [0, 1, 5, 6, 0xffffffff, 0x80000000][i % 6]
        white_bits = [0, 0x80000000, 0x3f800000, 0x40000000, 0xbf800000,
                      0x00800000, 0x7f7fffff, 0x7fc12345][i % 8]
        both = []
        for compiled in [False, True]:
            c = F.cpu(bundle)
            c.mem.add(0x83010000, 0x2000)
            c.mem.wr(SELF, module)
            c.mem.wr(0x83010000, globals_)
            F.put(c, SELF + 0x244, FACADES[2])
            F.put(c, SELF + 0x248, FACADES[0])
            F.put(c, SELF + 0x25c, FACADES[1])
            for facade, backend in zip(FACADES, BACKENDS): F.put(c, facade + 0x108, backend)
            slot_offset = 0x25c if method == 'BeginQuarterResBuffer' else 0x244
            assert_capable = method in ['EndRenderEnvironmentMapFace', 'BeginRenderEnvironmentMapFace', 'BeginQuarterResBuffer']
            if assert_capable and i % 5 == 1: F.put(c, SELF + slot_offset, 0)
            if assert_capable and i % 5 == 2:
                index = 1 if method == 'BeginQuarterResBuffer' else 2
                F.put(c, FACADES[index] + 0x108, 0)
            for kind, table, count in [('depth', 0x8301090c, 5), ('raster', 0x83010a38, 3), ('blend', 0x83010f70, 9)]:
                for n in range(count): F.put(c, table + n * 4, AUX + 0x5000 + table % 0x1000 + n * 0x100)
            if slots:
                for n, (table, cache, lock) in enumerate([(0x8301090c, 0x83010a28, 0x83010904),
                                                         (0x83010a38, 0x83010a2c, 0x83010905),
                                                         (0x83010f70, 0x83010964, 0x83010907)]):
                    wanted = int.from_bytes(c.mem.rd(table + slots[n] * 4, 4), 'big')
                    mode = (i // (8 * 3 ** n)) % 3
                    F.put(c, cache, [0, wanted, AUX + 0x7000 + n * 0x100][mode])
                    c.mem.wr(lock, bytes([0 if not (i & (1 << n)) else [1, 2, 255][(i // 8) % 3]]))
            sp = c.g(1) & 0xffffffff
            c.mem.wr(sp - 0x2000, bytes([i]) * 0x2000)
            c.setg(3, SELF)
            c.setg(4, AUX + 0x3800 if method == 'EndRenderPostFx' else face)
            c.setf(1, struct.unpack('>f', struct.pack('>I', white_bits))[0])
            trace = []
            assertion = [None]

            def context():
                return (hashlib.sha256(c.mem.rd(SELF, 0xca80)).hexdigest(),
                        c.mem.rd(0x83010900, 0x180).hex(), c.mem.rd(0x83010f70, 36).hex())

            def begin_assert(x): trace.append(('assert_begin', context()))
            def fire(x):
                args = [F.cstring(x, x.g(n) & 0xffffffff) for n in [3, 4]]
                assertion[0] = x.g(5) & 0xffffffff
                trace.append(('assert_fire', *args, assertion[0], context()))
            def end_assert(x):
                trace.append(('assert_end', context()))
                if assertion[0] in [4163, 4204, 4302]:
                    index = 1 if method == 'BeginQuarterResBuffer' else 2
                    F.put(x, SELF + slot_offset, FACADES[index])
                else:
                    F.put(x, SELF + slot_offset, FACADES[0])
                    F.put(x, FACADES[0] + 0x108, BACKENDS[0])

            def start(x):
                trace.append(('start_monitor', x.g(3) & 0xffffffff, context()))
                if i % 7 == 0: F.put(x, SELF + 0x248, FACADES[2])
            def stop(x): trace.append(('stop_monitor', x.g(3) & 0xffffffff, context()))
            def target(x, inverted=False):
                trace.append(('target_inverted' if inverted else 'target', x.g(3) & 0xffffffff,
                              x.g(4) & 0xffffffff, context()))
                x.setf(1, 12345.0)
                if i % 7 == 1:
                    F.put(x, 0x8301090c + (slots[0] if slots else 0) * 4, AUX + 0x7100)
            def apply(x, name):
                trace.append(('apply_' + name, x.g(3) & 0xffffffff, x.g(4) & 0xffffffff, context()))
                if i % 7 == 2 and name == 'depth':
                    F.put(x, 0x83010a38 + slots[1] * 4, AUX + 0x7200)
                if i % 7 == 3 and name == 'raster':
                    F.put(x, 0x83010f70 + slots[2] * 4, AUX + 0x7300)
                if i % 7 == 4 and name == 'blend':
                    F.put(x, SELF + 0x248, FACADES[1])
                    F.put(x, SELF + 0x25c, FACADES[2])
            def clear(x, depth=False):
                trace.append(('clear_depth' if depth else 'clear_color',
                              x.mem.rd(x.g(3) & 0xffffffff, 16).hex(),
                              x.mem.rd(x.g(4) & 0xffffffff, 12).hex() if depth else None,
                              x.g(5 if depth else 4) & 0xffffffff, context()))
                if i % 7 == 6: F.put(x, 0x8301090c + slots[0] * 4, AUX + 0x7400)
            def blit(x, composite=False):
                trace.append(('blit_composite' if composite else 'blit_depth',
                              [x.g(n) & 0xffffffff for n in range(3, 9 if composite else 6)], context()))
                if i % 7 == 5:
                    if composite:
                        saved = x.g(5) & 0xffffffff
                        F.put(x, saved + 0x108, BACKENDS[1])
                        F.put(x, SELF + 0x248, FACADES[0])
                    else: F.put(x, SELF + 0x25c, FACADES[0])
            def resolve(x, face_only=False):
                trace.append(('resolve_face' if face_only else 'resolve_target',
                              [x.g(n) & 0xffffffff for n in range(3, 5 if face_only else 6)], context()))
                F.put(x, SELF + 0xc9e8, 0x76540000 + i)

            c.hooks.update({0x82817548: begin_assert, 0x82820810: fire, 0x82817558: end_assert,
                            0x82825e78: start, 0x82826098: stop,
                            0x827e7588: target, 0x827e7668: lambda x: target(x, True),
                            0x827e8150: lambda x: apply(x, 'depth'),
                            0x827e8690: lambda x: apply(x, 'raster'),
                            0x827e7d10: lambda x: apply(x, 'blend'),
                            0x82b61c88: clear, 0x82b61e18: lambda x: clear(x, True),
                            0x82406a68: lambda x: blit(x, True), 0x82406ea8: blit,
                            0x823f9338: resolve, 0x823f9170: lambda x: resolve(x, True)})
            if compiled:
                for at, name in bundle.unresolved.items():
                    if name.startswith('?SetState@Device@shadow@@'):
                        link_native(c, at, 0x82276ad0 if 'DepthStencilState@' in name else
                                    0x82276b38 if 'RasterizerState@' in name else 0x82276a68)
                    elif name.startswith('?BeginAssert@'): c.hooks[at] = begin_assert
                    elif name.startswith('?FireAssert@'): c.hooks[at] = fire
                    elif name.startswith('?EndAssert@'): c.hooks[at] = end_assert
                    elif name.startswith('?StartMonitor@'): c.hooks[at] = start
                    elif name.startswith('?StopMonitor@'): c.hooks[at] = stop
                    elif name.startswith('?SetRenderTargetStateInvertDepth@'): c.hooks[at] = lambda x: target(x, True)
                    elif name.startswith('?SetRenderTargetState@'): c.hooks[at] = target
                    elif name.startswith('?Clear@Device@'): c.hooks[at] = (lambda x: clear(x, True)) if 'ClearDepthStencilParameters@' in name else clear
                    elif name.startswith('?BlitComposite@'): c.hooks[at] = lambda x: blit(x, True)
                    elif name.startswith('?BlitDepth@'): c.hooks[at] = blit
                    elif name.startswith('?Resolve@Target@'): c.hooks[at] = lambda x: resolve(x, True)
                    elif name.startswith('?Resolve@RenderTarget@'): c.hooks[at] = resolve
            c.run(bundle.entry if compiled else original)
            both.append((c.mem.rd(SELF, 0xca80), c.mem.rd(0x83010000, 0x1200),
                         [c.mem.rd(a, 0x110) for a in FACADES], trace))
        checks += 1
        if both[0] != both[1]:
            errors.append(dict(method=method, case=i,
                               module_differences=[hex(n) for n, (a, b) in enumerate(zip(both[0][0], both[1][0])) if a != b],
                               global_differences=[hex(0x83010000 + n) for n, (a, b) in enumerate(zip(both[0][1], both[1][1])) if a != b],
                               traces=[v[3] for v in both]))

result = dict(cases=checks, errors=errors, object=str(path), object_sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
              scope='256 whole cases for each of five genuine native Renderer pass brackets. Full Renderer/global/facade bytes, '
                    'assert text/file/line and continuation reloads, lock/cache branches, state application, clear data, '
                    'saved/live target and monitor inputs, Memory blit arguments and service order compare.',
              limits='Compiled calls to shadow state setters link to their genuine original native bodies by linkage-only thunks; '
                     'this does not claim a compiled Shadow TU. Their low-level state appliers, assertions, GPU monitors, '
                     'target binding/resolve, Device clears and separately verified Memory blits are controlled services. '
                     'No real GPU, whole frame, link or boot claim.')
report = out / 'renderer-pass-brackets-comparison.json'
assert not report.exists(), 'Refusing to overwrite completed comparison evidence'
report.write_text(json.dumps(result, indent=2) + '\n')
print(checks, 'whole Renderer pass bracket cases; errors', len(errors))
print(errors[:1])
raise SystemExit(bool(errors))
