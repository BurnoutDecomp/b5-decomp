import ast
import hashlib
import json
from pathlib import Path
import struct

dest = Path(__file__).resolve().parent
root = dest.parents[1]
seal = json.loads((root / 'wave20/r27-link/seal.json').read_text())
parser = root / 'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
functions = [n for n in ast.parse(parser.read_text()).body
             if isinstance(n, ast.FunctionDef) and n.name in ('parse', 'archive')]
namespace = {'struct': struct}
exec(compile(ast.Module(body=functions, type_ignores=[]), str(parser), 'exec'), namespace)
parse, archive = namespace['parse'], namespace['archive']
kind_defs = {'strong', 'comdat', 'common', 'absolute', 'import'}

def symbols(file):
    return parse(Path(file).read_bytes())

def undefined(rows):
    return {name for name, kind, *_ in rows if kind in ('undefined', 'weak')}

source = 'src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp'
baseline = next(row for row in seal['selected'] if row['source'] == source)
gate = json.loads((dest / 'native-final/compile-results.json').read_text())
candidate = next(row for row in gate['results'] if row['source'] == source)
before = symbols(baseline['object'])
after = symbols(candidate['object'])
target = '?Interpret@DrawRenderable@CgsGraphics@@SAXPAUDispatchCommand@2@PAVDispatchFrame@2@PAXM@Z'
assert target in undefined(before)
assert target not in undefined(after)
assert any(name == target and kind in kind_defs for name, kind, *_ in after)
new = undefined(after) - undefined(before)
providers = {name: [] for name in sorted(new)}
for row in seal['selected']:
    for name, kind, *_ in symbols(row['object']):
        if name in new and kind in kind_defs:
            providers[name].append({'source': row['source'], 'kind': kind})
for lib in seal['libraries']:
    for member, data in archive(Path(lib['path'])):
        parsed = parse(data)
        if parsed is None and len(data) >= 20 and data[:4] == b'\x00\x00\xff\xff':
            name = data[20:].split(b'\x00', 1)[0].decode('latin1')
            parsed = [(name, 'import'), ('__imp_' + name, 'import')]
        for name, kind, *_ in parsed or ():
            if name in new and kind in kind_defs:
                providers[name].append({'library': lib['path'], 'member': member, 'kind': kind})

report = {'r27_head': seal['head'], 'r27_object': baseline, 'candidate_object': candidate['object'],
          'candidate_sha256': hashlib.sha256(Path(candidate['object']).read_bytes()).hexdigest(),
          'resolved_in_candidate': target, 'new_external_references': providers,
          'unprovided_in_inspected_set': [name for name, records in providers.items() if not records],
          'note': 'Static COFF/SDK provider inventory only. No whole-image link or launch.'}
(dest / 'reference-inventory.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'new': len(new), 'unprovided': report['unprovided_in_inspected_set']}))
