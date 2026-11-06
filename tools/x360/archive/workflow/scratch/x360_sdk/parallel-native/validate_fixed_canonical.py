"""Run inspected arithmetic fixtures against the actual canonical rebuilt CPPs.

Only artifact selection and report writes are redirected. Expected values,
instruction emulation, test cases, assertions and the negative control stay intact.
"""
import ast
import hashlib
import json
from pathlib import Path
import sys

sys.dont_write_bytecode = True
HERE = Path(__file__).resolve().parent
PARENT = HERE.parents[2]
ROOT = PARENT / 'b5-decomp'
REPORT = ROOT / 'build/x360/parallel-wave2-fixed/compile-results.json'
rows = json.loads(REPORT.read_text())
assert rows['state'] == 'complete' and rows['passed'] == 4 and not rows['failed']
spark = next(r for r in rows['results'] if r['source'].endswith('ParticleModule_SparkEvents.cpp'))
cap = next(r for r in rows['results'] if r['source'].endswith('LineSegIntersect.cpp'))


def assignment(name, expression):
    return ast.parse(name + ' = ' + expression).body[0]


def run_inspected(path, replacements):
    tree = ast.parse(path.read_text(encoding='utf-8'))
    kept = []
    changes = []
    for node in tree.body:
        if (isinstance(node, ast.Expr) and isinstance(node.value, ast.Call)
                and isinstance(node.value.func, ast.Attribute)
                and node.value.func.attr == 'write_text'):
            changes.append({'line': node.lineno, 'change': 'omit original report write'})
            continue
        if (isinstance(node, ast.Assign) and len(node.targets) == 1
                and isinstance(node.targets[0], ast.Name)
                and node.targets[0].id in replacements):
            name = node.targets[0].id
            kept.append(assignment(name, replacements[name]))
            changes.append({'line': node.lineno, 'change': 'redirect artifact selection', 'name': name})
        else:
            kept.append(node)
    namespace = {'__file__': str(path), '__name__': 'canonical_arithmetic_validation'}
    ast.fix_missing_locations(tree)
    module = ast.fix_missing_locations(ast.Module(body=kept, type_ignores=[]))
    exec(compile(module, str(path), 'exec'), namespace)
    return namespace, changes


particle_path = PARENT / 'scratch/parallel-native/particles_native/validate_fma_boundary.py'
particle, changes1 = run_inspected(particle_path, {
    'fixed_path': 'Path(' + repr(spark['object']) + ')',
    'asm': 'Path(' + repr(spark['assembly']) + ').read_text(errors="replace")',
})
cap_path = PARENT / 'scratch/parallel-native/native-replay-collision/validate_cap_vmx.py'
caps, changes2 = run_inspected(cap_path, {
    'r': repr(cap),
})
result = {
    'report': str(REPORT),
    'particle': particle['result'],
    'caps': caps['report'],
    'fixture_selection_changes': {'particle': changes1, 'caps': changes2},
    'fixture_sha256': {
        str(particle_path): hashlib.sha256(particle_path.read_bytes()).hexdigest(),
        str(cap_path): hashlib.sha256(cap_path.read_bytes()).hexdigest(),
    },
}
(HERE / 'wave2-fixed-canonical-behavior.json').write_text(json.dumps(result, indent=2) + '\n')
print('Canonical objects: particle256 + cap1700 original-instruction cases passed.')
