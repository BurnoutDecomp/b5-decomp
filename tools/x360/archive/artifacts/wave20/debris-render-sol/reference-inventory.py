import ast
import hashlib
import json
from pathlib import Path
import struct

dest = Path(__file__).resolve().parent
root = dest.parents[1]
r29 = root / 'wave20/r29-link'
seal = json.loads((r29 / 'seal.json').read_text())
assert seal['head'] == '145fce281539ce509c4dd1974f1e61d099650378'
parser = root / 'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
nodes = [n for n in ast.parse(parser.read_text()).body
         if isinstance(n, ast.FunctionDef) and n.name in ('parse', 'archive')]
namespace = {'struct': struct}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(parser), 'exec'), namespace)
parse, archive = namespace['parse'], namespace['archive']

defined_kinds = {'strong','comdat','common','absolute','import'}
def read_obj(path):
    rows = parse(Path(path).read_bytes())
    if rows is None:
        raise ValueError(path)
    return rows
def undefined(rows):
    return {name for name, kind, *_ in rows if kind in ('undefined', 'weak')}

target = '?RenderDebrisArray@BrnDebrisRenderer@Native@BrnParticle@@QAAXMPBVBrnDebrisArray@23@W4EDebrisArrayID@23@_N@Z'
actual = json.loads((r29 / 'diagnostic-parsed.json').read_text())
assert any(row['symbol'] == target for row in actual['unresolved'])
gate = json.loads((dest / 'native-final/compile-results.json').read_text())
r29_paths = {row['source']: row['object'] for row in seal['selected']}
candidate_paths = {row['source']: row['object'] for row in gate['results']}
api_gate = root / 'wave20/particle-native-states-root/native/compile-results.json'
api_paths = {row['source']: row['object'] for row in json.loads(api_gate.read_text())['results']}
new = {}
for source, candidate in candidate_paths.items():
    before = undefined(read_obj(r29_paths[source]))
    after = undefined(read_obj(candidate))
    for name in after - before:
        new.setdefault(name, []).append(source)
needed = set(new) | {target}
providers = {name: [] for name in needed}
for source, path in (r29_paths | api_paths | candidate_paths).items():
    for name, kind, *_ in read_obj(path):
        if name in needed and kind in defined_kinds:
            providers[name].append({'source': source, 'object': path, 'kind': kind})
for lib in seal['libraries']:
    for member, data in archive(Path(lib['path'])):
        rows = parse(data)
        if rows is None and len(data) >= 20 and data[:4] == b'\x00\x00\xff\xff':
            name = data[20:].split(b'\0', 1)[0].decode('latin1')
            rows = [(name,'import'), ('__imp_' + name,'import')]
        for name, kind, *_ in rows or ():
            if name in needed and kind in defined_kinds:
                providers[name].append({'library': lib['path'], 'member': member, 'kind': kind})
assert len(providers[target]) == 1
report = {'base': seal['head'], 'r29_missing_symbol': target,
          'candidate_provider': providers[target][0], 'api_candidate': 'ab01fd8bfb84c16a26dffbb86468dc727268b9ad',
          'new_references': {name:{'callers': new[name], 'providers':providers[name]} for name in sorted(new)},
          'unprovided': sorted(name for name in needed if not providers[name]),
          'note': 'Read-only COFF and SDK archive inventory; no full link, archive liveness or runtime proof.'}
(dest / 'reference-inventory.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps({'new':len(new), 'unprovided':report['unprovided'], 'provider':providers[target]}))
