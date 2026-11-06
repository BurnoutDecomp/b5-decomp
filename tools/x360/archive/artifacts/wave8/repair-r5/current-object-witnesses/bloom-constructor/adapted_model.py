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
assert F.M.V.x360rd.u32(2189514096) == 2089427832
assert F.M.V.x360rd.u32(2196923560) == 4

def private_loader(cls, declaration, assignment, replacement):
    code = inspect.getsource(cls)
    assert code.count(assignment) == 1
    code = code.replace(assignment, replacement)
    code = code.replace('assert s[3] < 0x10000', 'assert s[3] < 0x1000000')
    namespace = dict(vars(importlib.import_module(cls.__module__)))
    exec(compile(code, declaration, 'exec'), namespace)
    return namespace[cls.__name__]
PackedCoff = private_loader(F.Coff, 'private-packed-coff', 'bases = {sec: 0x83800000 + i*0x10000 for i, sec in enumerate(sorted(used))}', 'bases = {}\n        cursor = 0x83800000\n        for sec in sorted(used):\n            bases[sec] = cursor\n            cursor += max(16, (self.sections[sec][3] + 15) & ~15)')
PackedBundle = private_loader(F.Bundle, 'private-packed-bundle', 'bases = {key:0x83800000+i*0x10000 for i,key in enumerate(sorted(used))}', 'bases = {}\n        cursor = 0x83800000\n        for key in sorted(used):\n            bases[key] = cursor\n            cursor += max(16, (self.objects[key[0]].sections[key[1]][3] + 15) & ~15)')
report_path = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\bloom-constructor\\actual-row-selection.json')
report = json.loads(report_path.read_text(encoding='utf-8'))
paths = {Path(r['source']).name: Path(r['object']) for r in report['results']}
prereq_path = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r5\\current-object-witnesses\\bloom-constructor\\actual-row-selection.json')
prereq = json.loads(prereq_path.read_text(encoding='utf-8'))
extra = {Path(r['source']).name: Path(r['object']) for r in prereq['results']}
product = PackedCoff(paths['BrnPostFxBloom.cpp'], '?Construct@BrnPostFxBloom@')
objs = [product, PackedCoff(paths['vertexdescriptorparameters.cpp'], '??0Parameters@VertexDescriptor@renderengine@@'), PackedCoff(extra['BaseResourceDescriptor.cpp'], '??0BaseResourceDescriptor@rw@@'), PackedCoff(extra['rwgpfxhelper.cpp'], '?CreateProgram@PfxHelper@')]
bundle = PackedBundle(objs)
bundle.select('?Construct@BrnPostFxBloom@')
packages = [('gacBloomDSVertexProgram', 2181293816, 348, 0), ('gacBloomDSPixelProgram', 2181294168, 516, 1), ('gacBloomBlurVertexProgram', 2181294688, 368, 0), ('gacBloomBlurPixelProgram', 2181295056, 420, 1), ('gacBloomBlurOldVertexProgram', 2181295480, 404, 0), ('gacBloomBlurOldPixelProgram', 2181295888, 424, 1)]
package_addresses = {}
c = cpu(bundle)
images = []
for name, original, size, kind in packages:
    address = F.address(bundle, name)
    data = c.mem.rd(address, size)
    assert data == F.M.V.x360rd.rd(original, size), (name, address)
    package_addresses[original] = name
    package_addresses[address] = name
    images.append({'name': name, 'original': hex(original), 'compiled': hex(address), 'size': size, 'sha256': hashlib.sha256(data).hexdigest(), 'kind': kind})
BASE, SELF, ALLOC, VT, DATA = (1459617792, 1459618048, 1459618560, 1459618816, 1459621888)
rng = random.Random(2185238056)
errors = []
cases = 128
for index in range(cases):
    initial = bytearray(rng.randbytes(49152))
    struct.pack_into('>I', initial, ALLOC - BASE, VT)
    struct.pack_into('>I', initial, VT - BASE + 16, BASE + 61440)
    results = []
    for compiled in [False, True]:
        c = cpu(bundle)
        c.mem.add(BASE, len(initial), initial)
        trace = []
        allocation = [0]
        lookup = [0]

        def words(state, address, count):
            return tuple(struct.unpack('>' + str(count) + 'I', state.mem.rd(address, count * 4)))

        def params(state, address):
            w = words(state, address, 10)
            assert w[0] in package_addresses, tuple((hex(t) for t in w))
            return (package_addresses[w[0]], w[1], w[2], w[4], w[6], w[8], w[9])

        def vdesc(state):
            raw = state.mem.rd(state.g(4) & 4294967295, 272)
            trace.append(('VertexDescriptor', raw.hex()))
            state.mem.wr(state.g(3) & 4294967295, struct.pack('>10I', 80, 16, 0, 1, 0, 1, 0, 1, 0, 1))

        def progdesc(state):
            p = params(state, state.g(4) & 4294967295)
            trace.append(('ProgramDescriptor', p))
            size = 256 + p[2]
            state.mem.wr(state.g(3) & 4294967295, struct.pack('>10I', size, 16, 0, 1, 128, 128, 0, 1, 0, 1))

        def allocate(state):
            a = allocation[0]
            allocation[0] += 1
            trace.append(('Allocate', state.g(4) & 4294967295, words(state, state.g(5) & 4294967295, 10), state.g(6) & 4294967295))
            state.mem.wr(state.g(3) & 4294967295, struct.pack('>5I', *(DATA + a * 4096 + j * 256 for j in range(5))))

        def vinit(state):
            trace.append(('VertexInitialize', words(state, state.g(3) & 4294967295, 5), state.mem.rd(state.g(4) & 4294967295, 272).hex()))
            state.setg(3, DATA)

        def pinit(state):
            resource = words(state, state.g(3) & 4294967295, 5)
            p = params(state, state.g(4) & 4294967295)
            trace.append(('ProgramInitialize', resource, p))
            state.setg(3, resource[0])

        def handle(state):
            n = lookup[0]
            lookup[0] += 1
            trace.append(('Handle', state.g(3) & 4294967295, F.cstring(state, state.g(4) & 4294967295)))
            F.put(state, state.g(5) & 4294967295, 270544960 + n)
            state.setg(3, 1)
        c.hooks[BASE + 61440] = allocate
        c.hooks[2192979920] = vdesc
        c.hooks[2192978016] = vinit
        c.hooks[2192979120] = progdesc
        c.hooks[2192974968] = pinit
        c.hooks[2192968112] = handle
        for address, n in bundle.unresolved.items():
            if n.startswith('?GetResourceDescriptor@VertexDescriptor@'):
                c.hooks[address] = vdesc
            elif n.startswith('?Initialize@VertexDescriptor@'):
                c.hooks[address] = vinit
            elif n.startswith('?GetResourceDescriptor@ProgramBuffer@'):
                c.hooks[address] = progdesc
            elif n.startswith('?Initialize@ProgramBuffer@'):
                c.hooks[address] = pinit
            elif n.startswith('?GetVariableHandleByName@ProgramBuffer@'):
                c.hooks[address] = handle
            elif n.startswith('?FireAssert@'):
                c.hooks[address] = lambda state: trace.append(('ASSERT',))
            elif n.startswith(('?BeginAssert@', '?EndAssert@')):
                c.hooks[address] = lambda state: None
        c.setg(3, SELF)
        c.setg(4, ALLOC)
        try:
            c.run(bundle.entry if compiled else 2185238056)
        except Exception:
            print('FAIL', index, compiled, hex(getattr(c, 'last_pc', 0)), flush=True)
            raise
        results.append((c.mem.rd(BASE, len(initial)), trace, allocation[0], lookup[0]))
    if results[0] != results[1]:
        errors.append({'index': index, 'different_bytes': [i for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b], 'original_trace': results[0][1], 'compiled_trace': results[1][1]})
result = {'cases': cases, 'mismatches': errors, 'packages': images, 'objects': {str(o.path): sha(o.path) for o in objs}, 'report_sha256': sha(report_path), 'prereq_report_sha256': sha(prereq_path), 'verifier_sha256': sha(__file__), 'limits': 'Actual whole original/native Bloom constructor, real linked PfxHelperCreateProgram, VertexParameters and BaseResourceDescriptor constructors; six actual emitted shader packages byte-identical to original. Allocator, vertex/program descriptor/init and handle lookup boundaries controlled, only package pointer relocations mapped to named identities. Entire poisoned object/resource/guard bytes and ordered parameter/allocation/lookup traces compared. Params compares seven defined words, three native-unused words excluded. Does not execute XG/D3D GPU allocation/program parsing, failure/assert paths, full startup/device/frame/game or global byte match.'}
p = _r5_output(HERE / 'constructor-final-comparison.json')
assert not p.exists()
_r5_output(p).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'cases': cases, 'mismatches': len(errors), 'shader_packages': len(images)}), flush=True)
assert not errors