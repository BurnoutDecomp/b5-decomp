from pathlib import Path
import ast, hashlib, importlib, inspect, json, random, struct, sys

HERE = Path(__file__).resolve().parent
fixture_source = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-tintblend-root/validate_native.py')
nodes = []
for node in ast.parse(fixture_source.read_text()).body:
    if isinstance(node, ast.Assert):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(fixture_source), 'exec'), globals())
# The imported prefix sets HERE from __file__, which remains this script.
assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4

# Actual hardware-init BSS includes its 400-KiB backing buffer. Pack sections by
# their real COFF extents; the shared fixture's fixed 64-KiB spacing cannot load
# that object. This changes addresses only, not any product instructions/data.
def private_loader(cls, declaration, assignment, replacement):
    code = inspect.getsource(cls)
    assert code.count(assignment) == 1
    code = code.replace(assignment, replacement)
    code = code.replace('assert s[3] < 0x10000', 'assert s[3] < 0x1000000')
    namespace = dict(vars(importlib.import_module(cls.__module__)))
    exec(compile(code, declaration, 'exec'), namespace)
    return namespace[cls.__name__]

PackedCoff = private_loader(F.Coff, 'private-packed-coff',
    'bases = {sec: 0x83800000 + i*0x10000 for i, sec in enumerate(sorted(used))}',
    '''bases = {}
        cursor = 0x83800000
        for sec in sorted(used):
            bases[sec] = cursor
            cursor += max(16, (self.sections[sec][3] + 15) & ~15)''')
PackedBundle = private_loader(F.Bundle, 'private-packed-bundle',
    'bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}',
    '''bases = {}
        cursor = 0x83800000
        for key in sorted(used):
            bases[key] = cursor
            cursor += max(16, (self.objects[key[0]].sections[key[1]][3] + 15) & ~15)''')

stage = sys.argv[3] if len(sys.argv) > 3 else 'native'
report_path = HERE / stage / 'compile-results.json'
report = json.loads(report_path.read_text())
paths = {r['source']: Path(r['object']) for r in report['results'] if r['passed']}
hardware_report_path = Path('D:/Reverse/BP-X360-Experimental-Artifacts/jobmanager-provider-luna-3534/native/compile-results.json')
hardware_report = json.loads(hardware_report_path.read_text())
hardware_path = Path(hardware_report['results'][0]['object'])
objects = [PackedCoff(paths['src/GameSource/Graphics/PostFx/BrnPostFx.cpp'], '?BeginTintBlend@BrnPostFx@@'),
    PackedCoff(paths['src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxtint.cpp'], '?BeginBlendJob@Tint@'),
    PackedCoff(Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7/full-successor1/job-0ccb82fc.obj'), '?SetData@Job@Jobs@EA@@'),
    PackedCoff(hardware_path, '?JobManager@CgsSystem@@')]
bundle = PackedBundle(objects)
bundle.select('?BeginTintBlend@BrnPostFx@@')
scheduler = F.address(bundle, '?mJobManager@HardwareInit@CgsSystem@@')
print('Actual caller/provider sections', len(bundle.images), 'scheduler', hex(scheduler), flush=True)
print('Controlled/rejected external symbols', bundle.unresolved, flush=True)

BASE, SELF, TINT0, TINT1, CUBES, PIXELS = 0x45000000, 0x45000100, 0x45002000, 0x45002200, 0x45003000, 0x45004000
rng = random.Random(0x823F8380)
errors, cases = [], int(sys.argv[1]) if len(sys.argv) > 1 else 128
output_name = sys.argv[2] if len(sys.argv) > 2 else 'caller-smoke.json'
traces = []
for index in range(cases):
    initial = bytearray(rng.randbytes(0x5000))
    active = 0 if index % 8 == 0 else rng.randrange(1, 5)
    enabled = rng.getrandbits(32)
    enabled = enabled & ~0x20 if index % 8 == 1 else enabled | 0x20
    count = index % 6
    current = rng.getrandbits(32)
    width = [16, 32, 64][index % 3]
    row_pitch = width * 4 + 32
    slice_pitch = row_pitch * width + 64
    image = initial
    def put(address, value):
        struct.pack_into('>I', image, address - BASE, value & 0xFFFFFFFF)
    put(SELF + 0x380, active)
    put(SELF + 0x384, enabled)
    put(SELF + 0x5C0, TINT0)
    put(SELF + 0x5C4, TINT1)
    put(SELF + 0x5F8, count)
    put(SELF + 0x5FC, current)
    for i in range(5):
        put(SELF + 0x5CC + i * 4, CUBES + i * 0x20)
        put(CUBES + i * 0x20, rng.randrange(1, 65))
        put(CUBES + i * 0x20 + 4, PIXELS + i * 0x100 + index % 16)
        struct.pack_into('>f', image, SELF - BASE + 0x5E0 + i * 4, rng.uniform(-1.0, 2.0))
    for i, tint in enumerate((TINT0, TINT1)):
        put(tint + 0xC0, 0x48000000 + i * 0x100)
    results = []
    for compiled in (False, True):
        c = cpu(bundle)
        c.mem.add(BASE, len(initial), initial)
        c.max_steps = 2000000
        trace = []
        def lock(state):
            address = state.g(7) & 0xFFFFFFFF
            trace.append(('Lock', *(state.g(r) & 0xFFFFFFFF for r in range(3, 8))))
            data = bytearray(b'\x6B' * 0x1C)
            struct.pack_into('>I', data, 4, PIXELS + 0x800)
            struct.pack_into('>I', data, 8, row_pitch)
            struct.pack_into('>I', data, 0x14, slice_pitch)
            state.mem.wr(address, data)
        def get_width(state):
            trace.append(('GetWidth', state.g(3) & 0xFFFFFFFF))
            state.setg(3, width)
        def add_jobs(state):
            actual = state.g(3) & 0xFFFFFFFF
            assert actual == (scheduler if compiled else 0x830EA650), (compiled, hex(actual))
            trace.append(('AddJobs', 'shared_native_scheduler', state.g(4) & 0xFFFFFFFF,
                          state.g(5) & 0xFFFFFFFF,
                          state.mem.rd(SELF + 0x5F4, 1).hex(),
                          state.mem.rd(SELF + 0x634, 8).hex()))
        c.hooks[0x82B62B20] = lock
        c.hooks[0x82B60EC8] = get_width
        c.hooks[0x82BCB498] = add_jobs
        for address, name in bundle.unresolved.items():
            if name.startswith('?Lock@Texture@'):
                c.hooks[address] = lock
            elif name.startswith('?GetWidth@Texture@'):
                c.hooks[address] = get_width
            elif name.startswith('?AddJobs@JobScheduler@'):
                c.hooks[address] = add_jobs
        c.setg(3, SELF)
        c.run(bundle.entry if compiled else 0x823F8380)
        results.append((c.mem.rd(BASE, len(initial)), trace))
    if results[0] != results[1]:
        errors.append({'index': index, 'active': active, 'enabled': hex(enabled), 'count': count,
            'different_bytes': [i for i, (a,b) in enumerate(zip(results[0][0], results[1][0])) if a != b],
            'original_trace': results[0][1], 'compiled_trace': results[1][1]})
    if index < 8:
        traces.append({'index': index, 'trace': results[1][1]})

result = {'cases': cases, 'mismatches': errors, 'objects': {str(o.path): sha(o.path) for o in objects},
    'linked_functions': ['BrnPostFx::BeginTintBlend', 'Tint::BeginBlendJob', 'Job::SetData', 'CgsSystem::JobManager'],
    'actual_scheduler_address_in_fixture': hex(scheduler), 'sample_traces': traces,
    'native_report_sha256': sha(report_path), 'hardware_report_sha256': sha(hardware_report_path),
    'verifier_sha256': sha(__file__),
    'limits': 'Actual original/native caller and real Tint/SetData/JobManager providers, software PPC model and actual relocated native singleton BSS. GPU Lock/GetWidth and scheduler AddJobs are controlled; workers and pixel blending do not execute. Valid source counts0..5, varying gates/indices/weights, non-contiguous fixed-up source pointers; complete mapped object/tint/resource/source/guard bytes and ordered traces compared. CRT nonvolatile convention modeled; no actual scheduler concurrency, GPU/hardware/full Render/game or strict-link claim. Private loader packs real COFF extents to accommodate actual400KiB hardware BSS; no product data or instructions altered.'}
with (HERE / output_name).open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': cases, 'mismatches': len(errors)}), flush=True)
assert not errors
