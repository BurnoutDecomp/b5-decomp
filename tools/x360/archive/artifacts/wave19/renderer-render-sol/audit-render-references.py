import ast
import hashlib
import json
import os
from pathlib import Path
import struct
import sys

dest = Path(__file__).resolve().parent
root = dest.parents[1]
seal = json.loads((root / 'wave19/r23-link/seal.json').read_text())
parser = root / 'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
nodes = [
    node for node in ast.parse(parser.read_text()).body
    if isinstance(node, ast.FunctionDef) and node.name in ('parse', 'archive')
]
namespace = {'struct': struct}
exec(compile(ast.Module(body=nodes, type_ignores=[]), str(parser), 'exec'), namespace)
parse, archive = namespace['parse'], namespace['archive']
definition_kinds = {'strong', 'comdat', 'common', 'absolute', 'import'}

def symbols(path):
    parsed = parse(Path(path).read_bytes())
    if parsed is None:
        raise ValueError(path)
    return parsed

def external_uses(path):
    return {name for name, kind, *_ in symbols(path) if kind in ('undefined', 'weak')}

def get_object(report, source):
    rows = json.loads((dest / report / 'compile-results.json').read_text())['results']
    return next(row['object'] for row in rows if row['source'] == source)

renderer = 'src/GameSource/Graphics/BrnRendererModule.cpp'
before = get_object('monitor-colour-gate', renderer)
after = get_object(sys.argv[1], renderer)
new = external_uses(after) - external_uses(before)
providers = {name: [] for name in sorted(new)}
for row in seal['selected']:
    for name, kind, *_ in symbols(row['object']):
        if name in new and kind in definition_kinds:
            providers[name].append({'source': row['source'], 'kind': kind})

for library in seal['libraries']:
    for member, content in archive(Path(library['path'])):
        parsed = parse(content)
        if parsed is None and len(content) >= 20 and content[:4] == b'\x00\x00\xff\xff':
            name = content[20:].split(b'\x00', 1)[0].decode('latin1')
            parsed = [(name, 'import'), ('__imp_' + name, 'import')]
        for name, kind, *_ in parsed or ():
            if name in new and kind in definition_kinds:
                providers[name].append({'archive': library['path'], 'member': member, 'kind': kind})

for report_source in sys.argv[2:]:
    report, source = report_source.split(':', 1)
    for name, kind, *_ in symbols(get_object(report, source)):
        if name in new and kind in definition_kinds:
            providers[name].append({'candidate_source': source, 'gate': report, 'kind': kind})

result = {
    'baseline': before,
    'candidate': after,
    'candidate_sha256': hashlib.sha256(Path(after).read_bytes()).hexdigest(),
    'r23_head': seal['head'],
    'new_undefined_or_weak_names': providers,
    'no_parsed_provider': [name for name, definitions in providers.items() if not definitions],
    'note': 'Object/archive candidate providers only. No COMDAT liveness, archive extraction, or actual link established.',
}
(dest / os.environ.get('BPH_RENDER_REFERENCE_OUTPUT', 'render-reference-inventory.json')).write_text(json.dumps(result, indent=2) + '\n')
for name in result['no_parsed_provider']:
    print(name)
