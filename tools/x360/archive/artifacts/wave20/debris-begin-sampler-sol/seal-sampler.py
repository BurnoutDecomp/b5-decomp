from pathlib import Path
import ast, hashlib, json, struct, subprocess

d = Path(__file__).resolve().parent
root = d.parents[1]
source = 'src/GameSource/Effects/Particles/Native/BrnDebrisRenderer.cpp'
def git(*args):
    return subprocess.check_output(['git', *args])
def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
commit = git('rev-parse', 'HEAD').decode().strip()
base = git('rev-parse', 'HEAD^').decode().strip()
assert base == '54b2fa7752af7ab3ee423994a59df4de43cff28f'
changed = git('diff-tree','--no-commit-id','--name-only','-r',commit).decode().splitlines()
assert changed == [source]
marker = b'    // ARTIST 8228B078.'
assert git('show',f'{base}:{source}').split(marker,1)[1] == git('show',f'{commit}:{source}').split(marker,1)[1]
shader_files = git('diff-tree','--no-commit-id','--name-only','-r','ab01fd8bfb84').decode().splitlines()
shader_hashes = {}
for name in shader_files:
    assert git('show', f'{base}:{name}') == git('show', f'{commit}:{name}')
    shader_hashes[name] = hashlib.sha256(git('show', f'{commit}:{name}')).hexdigest()
assert len(shader_hashes) == 4
result = d / 'native-final/compile-results.json'
rows = json.loads(result.read_text())['results']
assert len(rows) == 3 and all(row['passed'] and row['machine'] == 498 for row in rows)
for row in rows:
    assert not any('\\src\\pc\\' in line.lower() for line in Path(row['log']).read_text(errors='replace').splitlines()
                   if 'including file:' in line.lower())

parser = root / 'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
defs = [n for n in ast.parse(parser.read_text()).body if isinstance(n, ast.FunctionDef) and n.name == 'parse']
namespace = {'struct': struct}
exec(compile(ast.Module(body=defs, type_ignores=[]), str(parser), 'exec'), namespace)
parse = namespace['parse']
old_gate = json.loads((d.parent / 'debris-render-sol/native-final/compile-results.json').read_text())
old_object = next(Path(row['object']) for row in old_gate['results'] if row['source'] == source)
new_object = next(Path(row['object']) for row in rows if row['source'] == source)
def references(path):
    return {name for name, kind, *_ in parse(path.read_bytes()) if kind in ('undefined', 'weak')}
new_refs = references(new_object) - references(old_object)
seal = json.loads((d.parent / 'r29-link/seal.json').read_text())
provider_rows = rows + [row for row in seal['selected'] if row['source'].endswith('/CgsImRenderer.cpp')]
providers = {}
for name in sorted(new_refs):
    providers[name] = [{'source':row['source'], 'object':row['object']} for row in provider_rows
                       if any(symbol == name and kind in ('comdat','strong','common')
                              for symbol, kind, *_ in parse(Path(row['object']).read_bytes()))]
assert providers and all(providers.values()), providers
patch = d / 'candidate.patch'
patch.write_bytes(git('format-patch','--stdout','--full-index','-1',commit))
art = d.parent / 'debris-render-sol/82281EC8-artist.asm'
report = {'commit':commit, 'base':base, 'changed_paths':changed,
          'render_debris_array_source_unchanged':True,
          'unchanged_root_shader_blob_sha256':shader_hashes,
          'new_external_refs_and_providers':providers, 'native_gate':{'passed':3,'jobs':1,'pc_includes':[]},
          'objects':[{'source':row['source'], **{key:{'path':row[key], 'sha256':digest(Path(row[key]))}
                       for key in ('object','assembly','log')}} for row in rows],
          'proof_sha256':{str(path):digest(path) for path in
                          (result, patch, art, d/'native-final-command.json')},
          'linked':False,'launched':False}
(d/'candidate.json').write_text(json.dumps(report, indent=2)+'\n')
print(commit)
print(digest(patch))
print(providers)
