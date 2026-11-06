from pathlib import Path
import ast, hashlib, importlib, inspect, json, random, struct, sys
OUT = Path(__file__).resolve().parent
source = OUT / 'validate_construct.py'
nodes = []
for n in ast.parse(source.read_text()).body:
    if isinstance(n, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'stage' for t in n.targets)):
        break
    nodes.append(n)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(source), 'exec'), globals())
stage = sys.argv[1]
negative = stage == 'name-void-v2'
rows = {Path(r['source']).name: r for r in json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\replay-module\\actual-row-selection.json').read_text())['results'] if r['passed']}
stream = json.loads(Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\replay-module\\actual-row-selection.json').read_text())['results']
stream = next((r for r in stream if r['source'].endswith('CgsStrStream.cpp')))
spec = [('BrnRaceCarEntityModule.cpp', '?Construct@RaceCarEntityModule@'), ('BrnReplayBaseSerialiser.cpp', '?Construct@BaseSerialiser@'), ('BrnReplayRaceCarEntitySerialiser.cpp', '?Construct@RaceCarEntitySerialiser@')]
objects = [PackedCoff(Path(rows[n]['object']), prefix) for n, prefix in spec] + [PackedCoff(Path(stream['object']), '??0StrStream@')]
prefixes = ['?Construct@RaceCarEntityModule@', '?Construct@RaceCarEntitySerialiser@', '?CheckStaticLayoutCleared@RaceCarEntitySerialiser@', '?Read@RaceCarEntitySerialiser@', '?Write@RaceCarEntitySerialiser@', '?GetStaticLayout@RaceCarEntitySerialiser@', '?Construct@BaseSerialiser@']
loader = inspect.getsource(F.Bundle)
loader = loader.replace('used, pending = set(), [(oi,sec)]', 'used, pending = set(), [(v[0],v[2])for n,v in self.definitions.items()if n.startswith(ROOT_PREFIXES)]')
loader = loader.replace('bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}', 'bases = {}\n        cursor = 0x83800000\n        for key in sorted(used):\n            bases[key] = cursor\n            cursor += max(16,(self.objects[key[0]].sections[key[1]][3]+15)&~15)')
ns = dict(vars(importlib.import_module(F.Bundle.__module__)))
ns['ROOT_PREFIXES'] = tuple(prefixes)
exec(compile(loader, 'module-stable-multi-root-bundle', 'exec'), ns)
bundle = ns['Bundle'](objects)
entries = {}
for p in prefixes:
    bundle.select(p)
    entries[p] = bundle.entry
artist = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX/0x822FD898.json')
export = json.loads(artist.read_text())

def target(pc):
    w = F.M.V.x360rd.u32(pc)
    assert w >> 26 == 18
    d = w & 67108860
    if d & 33554432:
        d -= 67108864
    return (d if w & 2 else pc + d) & 4294967295

def z(c, p):
    a = bytearray()
    while (v := c.mem.rd(p + len(a), 1)[0]):
        a.append(v)
    return bytes(a)
rng = random.Random(2184175796)
cases = []
mismatches = []
outside = []
for index in range(128):
    base, self, static, dynamic = (1459617792, 1459618048, 1476395024, 1493172240)
    initial = bytearray(rng.randbytes(100864))
    initial[self - base + 99828] = 165
    static_initial = rng.randbytes(110624)
    dynamic_initial = rng.randbytes(110624)
    results = []
    for compiled in [False, True]:
        c = cpu(bundle)
        c.mem.add(base, len(initial), initial)
        c.mem.add(static - 16, len(static_initial), static_initial)
        c.mem.add(dynamic - 16, len(dynamic_initial), dynamic_initial)
        trace = []
        foreign = []
        boundaries = []

        def noop(s):
            s.setg(3, 0)

        def reject(s):
            raise AssertionError('unexpected assert in module replay lifecycle')
        for row in export['xrefs_from']:
            if row['name'].startswith('__') or int(row['address'], 16) == 2187642048:
                continue
            c.hooks[int(row['address'], 16)] = reject if 'Assert::' in row['name'] else noop
        for at, name in bundle.unresolved.items():
            c.hooks[at] = reject if 'Assert' in name else noop

        def copy(s):
            d, a, n = (s.g(r) & 4294967295 for r in [3, 4, 5])
            trace.append(('memcpy', d, a, n))
            s.mem.wr(d, s.mem.rd(a, n))
            s.setg(3, d)

        def clear(s):
            d = s.g(3) & 4294967295
            trace.append(('clear', d))
            s.mem.wr(d, bytes([49 + index % 32]) * 16)
        for at, name in bundle.unresolved.items():
            if name == 'memcpy':
                c.hooks[at] = copy
            elif name.startswith('?Clear@RaceCarSerialiserFrame@'):
                c.hooks[at] = clear
        for pc in [2187641444, 2187641180]:
            c.hooks[target(pc)] = copy
        c.hooks[target(2187688580)] = clear

        class Observe(type(c)):

            def step(s, pc, w, nxt, depth):
                if pc == (entries[prefixes[1]] if compiled else 2187642048):
                    boundaries.append(('derived', s.g(3) & 4294967295))
                if pc == (entries[prefixes[6]] if compiled else 2187641472):
                    boundaries.append(('base', *[s.g(r) & 4294967295 for r in range(3, 8)], z(s, s.g(8) & 4294967295).decode(), s.g(9) & 255))
                return super().step(pc, w, nxt, depth)
        c.__class__ = Observe
        c.setg(3, self)
        c.run(entries[prefixes[0]] if compiled else 2184173720)
        post_ctor = c.mem.rd(base, len(initial))
        serial = self + 99840
        ctor_window = c.mem.rd(self + 99808, 192)
        returns = []
        if not negative:

            def run(p, original, ret=False):
                c.setg(3, serial)
                c.run(entries[p] if compiled else original)
                if ret:
                    returns.append(c.g(3) & 4294967295)
            run(prefixes[2], 2187688520)
            F.put(c, serial + 32, static)
            F.put(c, serial + 8, dynamic)
            run(prefixes[2], 2187688520)
            run(prefixes[2], 2187688520)
            mode = index % 8
            F.put(c, serial, mode)
            F.put(c, serial + 16, 55264 if mode == 5 else 0)
            F.put(c, serial + 20, 0)
            run(prefixes[3], 2187642136, True)
            run(prefixes[4], 2187642392, True)
            run(prefixes[5], 2183803904, True)
        results.append((ctor_window, c.mem.rd(self + 99808, 192), c.mem.rd(static - 16, len(static_initial)), c.mem.rd(dynamic - 16, len(dynamic_initial)), trace, returns, boundaries, post_ctor))
    a, b = results
    failed = a[:-1] != b[:-1]
    assert failed == negative, (index, 'unexpected replay result', a[4:7], b[4:7])
    if not negative:
        assert a[6] == b[6] == [('derived', self + 99840), ('base', self + 99840, 0, 0, 110592, 110592, 'RaceCarEntity', 1)]
        assert a[0][20] == 0
        assert a[0][21] == initial[self - base + 99829]
    if failed:
        mismatches.append({'case': index, 'original_boundaries': a[6], 'compiled_boundaries': b[6]})
    all_bad = [i - (self - base) for i, (x, y) in enumerate(zip(a[-1], b[-1])) if x != y]
    outside.append({'case': index, 'differing_module_bytes_outside_replay_window': len([i for i in all_bad if not 99808 <= i < 100000]), 'outside_offsets': list(map(hex, [i for i in all_bad if not 99808 <= i < 100000]))})
    cases.append({'case': index, 'mode': index % 8, 'boundaries': a[6]})
report = {'stage': stage, 'negative_control': negative, 'cases': 128, 'replay_mismatches': mismatches, 'case_boundaries': cases, 'preserved_whole_module_outside_debt': outside, 'objects': {str(o.path): hashlib.sha256(o.path.read_bytes()).hexdigest() for o in objects}, 'artist': str(artist), 'artist_sha256': hashlib.sha256(artist.read_bytes()).hexdigest(), 'verifier_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(), 'limits': 'Execute complete real original and compiled module Construct, composing actual derived Construct and actual Base Construct rather than substituting original Base. Identical physical module address and real+18600 pointer, no field or register rebinding. Owned replay interval185E0..1869F (including full80-byte serialiser and surrounding poisoned guard/flag bytes), subsequent lazy-clear/read/write/getter state, full static/dynamic buffers and16-byte guards, true return registers and ordered traces compare. Other module construction/monitor/queue services are controlled noops on each side; standard memcpy and FrameClear have explicit byte/marker controls. Full module outside-window differences are retained as known unclaimed source debt rather than normalized or asserted equivalent. No whole module constructor parity, allocator, FrameClear internals, synchronization, hardware or launch/game replay claim.'}
p = _r5_output(OUT / (stage + '-module-replay-v1.json'))
assert not p.exists()
_r5_output(p).write_text(json.dumps(report, indent=2) + '\n')
print('module replay', 128, 'mismatches', len(mismatches), 'negative', negative, 'outside debt', sorted(set((x['differing_module_bytes_outside_replay_window'] for x in outside))))