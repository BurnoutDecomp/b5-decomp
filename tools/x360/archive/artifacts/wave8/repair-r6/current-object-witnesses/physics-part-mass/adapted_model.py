from pathlib import Path
import ast, json, random, struct
HERE = Path(__file__).resolve().parent
fixture = Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-bloom-root/validate_constructor.py')
nodes = []
for node in ast.parse(fixture.read_text()).body:
    if isinstance(node, ast.Assign) and any((isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets)):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(fixture), 'exec'), globals())
caller_report = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\physics-part-mass\\actual-row-selection.json')
provider_report = Path('D:\\Reverse\\BP-X360-Experimental-Artifacts\\wave8\\repair-r6\\current-object-witnesses\\physics-part-mass\\actual-row-selection.json')
caller = json.loads(caller_report.read_text())
provider = json.loads(provider_report.read_text())
cp = next((Path(x['object']) for x in caller['results'] if x['source'].endswith('BrnPhysicalBodyPart_Construct.cpp')))
ep = next((Path(x['object']) for x in provider['results'] if x['source'].endswith('ExternalPhysicsBody.cpp')))
vf = next((Path(x['object']) for x in provider['results'] if x['source'].endswith('vecfloat.cpp')))
objects = [PackedCoff(cp, '?Construct@PhysicalBodyPart@'), PackedCoff(ep, '?SetMass@ExternalPhysicsBody@'), PackedCoff(vf, '??0VecFloat@')]
bundle = PackedBundle(objects)
bundle.select('?Construct@PhysicalBodyPart@')
setmass = F.address(bundle, '?SetMass@ExternalPhysicsBody@')
construct = F.address(bundle, '?Construct@ExternalPhysicsBody@')
prepare = F.address(bundle, '?Prepare@ExternalPhysicsBody@')

class MassCPU(TintCPU):

    def exec_vector(self, pc, word, mnemonic):
        if pc == self.mass_pc:
            body = self.g(3) if self.product else self.g(31)
            value = self.vr[1 if self.product else 0]
            self.mass_trace.append(('SetMass', body & 4294967295, tuple(value)))
        return super().exec_vector(pc, word, mnemonic)
BASE, SELF = (1509949440, 1509949696)
rng = random.Random(2187018616)
errors = []
for index in range(64):
    initial = rng.randbytes(8192)
    results = []
    for compiled in (False, True):
        c = cpu(bundle)
        c.__class__ = MassCPU
        c.product = compiled
        c.mass_trace = []
        c.mem.add(BASE, len(initial), initial)
        for address, data in bundle.images.items():
            if len(data) == 4:
                c.mem.add(address, 16, bytes(data) + rng.randbytes(12))
        c.mass_pc = setmass if compiled else 2187018724

        def begin(state):
            state.mass_trace.append(('Construct', state.g(3) & 4294967295))

        def finish(state):
            state.mass_trace.append(('Prepare', state.g(3) & 4294967295))
            state.setg(3, 1)
        c.hooks[2186941848] = begin
        c.hooks[2186967248] = finish
        c.hooks[construct] = begin
        c.hooks[prepare] = finish
        c.setg(3, SELF)
        c.run(bundle.entry if compiled else 2187018616)
        results.append((c.mem.rd(BASE, len(initial)), c.mass_trace))
    if results[0] != results[1]:
        errors.append({'index': index, 'different_bytes': [i for i, (a, b) in enumerate(zip(results[0][0], results[1][0])) if a != b], 'original_trace': results[0][1], 'compiled_trace': results[1][1]})
result = {'cases': 64, 'mismatches': errors, 'objects': {str(o.path): sha(o.path) for o in objects}, 'caller_report_sha256': sha(caller_report), 'provider_report_sha256': sha(provider_report), 'verifier_sha256': sha(__file__), 'limits': 'Actual whole PhysicalBodyPart constructor and real reconstructed SetMass with genuine SDK VecFloat constructor. Original inlined mass store and product member entry are observed without replacing instructions; exact physical receiver, full raw v1 mass and ordered Construct/SetMass/Prepare boundaries compared. ExternalBody Construct and Prepare controlled no-op/true boundaries. Every poisoned part/buffer/guard byte compared. Four-byte COFF float literal has varied mapped adjacent linker bytes for actual lvlx; following splat discards those bytes, literal unchanged. Does not prove controlled providers, whole part lifecycle, general physics, hardware, link or launch. Candidate-v3 provider has separate owner tests for other methods; SetMass composition alone is tested.'}
p = _r5_output(HERE / 'part-mass-final-comparison.json')
assert not p.exists()
_r5_output(p).write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'cases': 64, 'mismatches': len(errors)}), flush=True)
assert not errors