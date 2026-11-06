from pathlib import Path
import ast, hashlib, inspect, json, random, re, struct, sys, time

HERE = Path(__file__).resolve().parent
leaf_script = HERE / 'validate_native.py'
tree = ast.parse(leaf_script.read_text())
nodes = []
for node in tree.body:
    if isinstance(node, ast.Assert):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(leaf_script), 'exec'), globals())

assert F.M.V.x360rd.u32(0x82815570) == 0x7C8A2378
assert F.M.V.x360rd.u32(0x82F264A8) == 4
report = json.loads((HERE / 'native/compile-results.json').read_text())
object_path = Path(report['results'][0]['object'])
SPECS = [(1, 0x82AD4078, '?SetColour@'), (2, 0x82AD4170, '?Blend2Cubes@'),
         (3, 0x82AD4280, '?Blend3Cubes@'), (4, 0x82AD43D0, '?Blend4Cubes@'),
         (5, 0x82AD4528, '?Blend5Cubes@'), (6, 0x82AD46B0, '?Blend6Cubes@')]
BASE, PARAM, DEST = 0x45000000, 0x45000080, 0x45001000
SOURCE_BASES = [0x46000000 + i * 0x100000 for i in range(6)]
rng = random.Random(0x82AD4078)
errors, groups, details = [], {}, []
mode = sys.argv[1] if len(sys.argv) > 1 else 'smoke'

def compare(count, original, prefix, edge, entry_kind, case):
    obj = F.Coff(object_path, prefix if entry_kind == 'direct' else
                 '?TintBlend@' if entry_kind == 'dispatch' else '?TintBlendEntry@@')
    stride = (edge >> 4) * 64 + 32
    slice_stride = stride * edge + 64
    dest_bytes = max(0x200, slice_stride * max(1, edge) + 0x80)
    source_bytes = max(0x200, (edge >> 4) * 48 * edge * edge + 0x80)
    initial = bytearray(rng.randbytes(0x1000 + dest_bytes))
    weights = [f32(((-1 if case & 1 else 1) * (i + 1)) / (count + 1)) for i in range(6)]
    if case % 4 == 0:
        weights = [1.0] * 6
    elif case % 4 == 1:
        weights = [-1.0, 0.5, 1.0, 0.0, 2.0, -0.5]
    struct.pack_into('>5I', initial, PARAM - BASE, edge, count, stride, slice_stride, DEST)
    for i, address in enumerate(SOURCE_BASES):
        struct.pack_into('>I', initial, PARAM - BASE + 0x14 + i * 4, address + 0x40)
        struct.pack_into('>f', initial, PARAM - BASE + 0x2C + i * 4, weights[i])
    source_data = [rng.randbytes(source_bytes) for _ in range(count)]
    results = []
    step_counts = []
    for compiled in (False, True):
        c = cpu(obj)
        # Extend the existing fixture's CRT nonvolatile-save convention to
        # matching compiler externals; these do not supply product behavior.
        for address, symbol in obj.unresolved.items():
            if re.fullmatch(r'__(save|rest)vmx_\d+', symbol):
                c.hooks[address] = lambda state: None
        c.max_steps = 20000000
        c.mem.add(BASE, len(initial), initial)
        for address, data in zip(SOURCE_BASES, source_data):
            c.mem.add(address, len(data), data)
        c.setg(3, 0x5A5A5A5A if entry_kind == 'entry' else PARAM)
        c.setg(4, PARAM if entry_kind == 'entry' else 0x4A4A4A4A)
        c.setg(5, 0x3A3A3A3A)
        c.setg(6, 0x2A2A2A2A)
        c.run(obj.entry if compiled else
              0x82AD4860 if entry_kind == 'dispatch' else
              0x82AD2CE8 if entry_kind == 'entry' else original)
        for address, data in zip(SOURCE_BASES, source_data):
            assert c.mem.rd(address, len(data)) == data, ('source written', count, case, compiled)
        results.append((c.mem.rd(BASE, len(initial)), c.tint_sat, c.convert_counts))
        step_counts.append(c.steps)
    if results[0] != results[1]:
        errors.append({'count': count, 'edge': edge, 'entry': entry_kind, 'case': case,
                       'different_bytes': [i for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b],
                       'original_sat_conversions': results[0][1:], 'compiled_sat_conversions': results[1][1:]})
    return step_counts

for count, original, prefix in SPECS:
    # A full sixteen-cubed surface executes the real variant AND real leaf.
    # Partial edges exercise the native discarded tail and continuous source cursor.
    cases = [(0, 'direct'), (1, 'direct'), (15, 'direct')]
    if mode == 'full':
        cases += [(16, 'direct'), (17, 'direct'), (16, 'dispatch'), (16, 'entry')]
        if count in (1, 2):
            cases.append((32, 'direct'))
    else:
        cases = [(16, 'direct')]
    steps = []
    for case, (edge, kind) in enumerate(cases):
        t = time.monotonic()
        s = compare(count, original, prefix, edge, kind, case)
        steps.append({'edge': edge, 'entry': kind, 'steps': s})
        print('surface', count, edge, kind, s, 'seconds', round(time.monotonic()-t, 2), flush=True)
    groups[str(count)] = len(cases)
    details.append({'count': count, 'original': hex(original), 'compiled_prefix': prefix, 'steps': steps})

result = {'object': str(object_path), 'object_sha256': sha(object_path), 'cases': sum(groups.values()),
          'groups': groups, 'mismatches': errors, 'functions': details,
          'verifier_sha256': sha(__file__), 'conversion_model_sha256': sha(leaf_script),
          'limits': 'Actual whole original and compiled surface routines, dispatch table and EA Jobs entry; real leaves execute. Finite source weights, bounded non-aliasing mapped buffers; complete destination/source/parameter/guard bytes and unsigned conversion saturation/counts compared. Ignores non-contract volatile register values, uses existing software VMX/FP model and controlled CRT nonvolatile save/restore convention, also for compiler __savevmx/__restvmx externals. Does not verify CRT helper bodies or full register preservation. No exceptional weights, full VSCR, GPU, physical runtime or game launch claim.'}
target = HERE / ('surface-' + mode + '.json')
with target.open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'cases': result['cases'], 'mismatches': len(errors)}), flush=True)
assert not errors
