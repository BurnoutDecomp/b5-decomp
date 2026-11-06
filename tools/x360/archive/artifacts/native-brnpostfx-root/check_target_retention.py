from pathlib import Path
import ast, hashlib, json

HERE = Path(__file__).resolve().parent
script = HERE / 'validate_caller.py'
nodes = []
for node in ast.parse(script.read_text()).body:
    if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'report_path' for t in node.targets):
        break
    nodes.append(node)
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(script), 'exec'), globals())
peer_dir = Path('D:/Reverse/BP-X360-Experimental-Artifacts/rendertarget-terra-final2')
peer_report = json.loads((peer_dir / 'compile-results.json').read_text())
report = json.loads((HERE / 'native-combined/compile-results.json').read_text())
peer = {r['source']: Path(r['object']) for r in peer_report['results'] if r['passed']}
current = {r['source']: Path(r['object']) for r in report['results'] if r['passed']}
checks = []
for source, prefix in [
    ('src/SDKs/RenderEngineClub/MAIN/components/src/postfx/src/rwgpfxrendertarget.cpp', '?Begin@RenderTarget@'),
    ('src/GameShared/GameClasses/Graphics/CgsRenderTarget.cpp', '?Begin@CgsRenderTarget@@')]:
    old = PackedCoff(peer[source], prefix)
    new = PackedCoff(current[source], prefix)
    def raw_sections(obj):
        result = []
        for index, section in obj.sections.items():
            if section[0].startswith(b'.debug'):
                assert section[9] & 0x02000000 and not section[9] & 0x20000000
                continue
            data = obj.data[section[4]:section[4]+section[3]] if section[4] else bytes(section[3])
            relocations = [(offset, kind, obj.symbols[symbol] if kind != 0x12 else None)
                           for offset, symbol, kind in obj.relocations(index)]
            result.append((section[0].hex(), section[3], data.hex(), relocations))
        return result
    a, b = raw_sections(old), raw_sections(new)
    assert a == b, ('foreign provider object section/relocation changed', source)
    metadata = []
    for index, section in old.sections.items():
        if section[0].startswith(b'.debug'):
            other = new.sections[index]
            a_debug = old.data[section[4]:section[4]+section[3]]
            b_debug = new.data[other[4]:other[4]+other[3]]
            metadata.append({'section': section[0].decode().rstrip('\0'),
                'old_bytes': len(a_debug), 'new_bytes': len(b_debug),
                'old_sha256': hashlib.sha256(a_debug).hexdigest(),
                'new_sha256': hashlib.sha256(b_debug).hexdigest(),
                'discardable_nonexecuting_metadata': True})
    checks.append({'source': source, 'old_object': str(peer[source]), 'old_sha256': sha(peer[source]),
                   'new_object': str(current[source]), 'new_sha256': sha(current[source]),
                   'identical_runtime_sections': len(a),
                   'identical_runtime_bytes': sum(s[3] for s in new.sections.values() if not s[0].startswith(b'.debug')),
                   'excluded_debug_metadata': metadata})
result = {'checks': checks, 'peer_handoff': str(peer_dir / 'RENDERTARGET_PRIVATE_HANDOFF.json'),
          'peer_handoff_sha256': sha(peer_dir / 'RENDERTARGET_PRIVATE_HANDOFF.json'),
          'verifier_sha256': sha(__file__),
          'basis': 'Every actual COFF runtime code/data section and named relocation is identical to the peer final objects. Discardable nonexecuting .debug$S path metadata differs and is separately hashed, not counted as runtime identity; COFF file timestamp/header symbol auxiliary bookkeeping is not used. Initial all-section equality failed solely on .debug$S, preserved in tool output. Peer1344 scoped comparisons retain their original object/service bindings; this is identity retention, not a new1344-case runtime execution.'}
with (HERE / 'render-target-retention.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps(checks, indent=2))
