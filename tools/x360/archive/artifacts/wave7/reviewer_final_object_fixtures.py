"""Read-only Wave7 final-object rerun of the inspected original/COFF fixtures.

Run with Python -B; optional argv[1] is a complete compile-results.json.
Prints evidence only. No source, report, directory or bytecode writes.
"""
import ast
import contextlib
import hashlib
import io
import json
import sys
from pathlib import Path

sys.dont_write_bytecode = True
ROOT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave7')
REPORT = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'full-successor1/compile-results.json'
data = json.loads(REPORT.read_text())
assert data['state'] == 'complete' and len(data['results']) == 3615
paths = {Path(r['source']).name: Path(r['object']) for r in data['results'] if r['passed']}
required = ['CgsLooseOctree.cpp', 'CgsSpatialPartition.cpp', 'CgsJobCoarseResultBuffer.cpp',
            'BaseResourceDescriptor.cpp', 'BrnCrashNavEnterOnline_wI_07.cpp',
            'BrnOnlineGameRoomPlayerInfo_wH_12.cpp', 'Iir2.cpp']
assert all(name in paths for name in required)
sha = lambda p: hashlib.sha256(Path(p).read_bytes()).hexdigest()

class Rebind(ast.NodeTransformer):
    def visit_Expr(self, node):
        if (isinstance(node.value, ast.Call) and isinstance(node.value.func, ast.Attribute)
                and node.value.func.attr == 'write_text'):
            return None
        return self.generic_visit(node)

    def visit_Assign(self, node):
        if any(isinstance(t, ast.Name) and t.id == 'D' for t in node.targets):
            node.value = ast.Call(func=ast.Name(id='Path', ctx=ast.Load()),
                                  args=[ast.Constant(str(REPORT.parent))], keywords=[])
        return self.generic_visit(node)

    def visit_Constant(self, node):
        if isinstance(node.value, str):
            if node.value.endswith('CgsLooseOctree-b3d7e078.obj'):
                node.value = str(paths['CgsLooseOctree.cpp'])
            elif node.value.endswith('CgsSpatialPartition-65a81eb0.obj'):
                node.value = str(paths['CgsSpatialPartition.cpp'])
            elif node.value.endswith('octree-layout-terra-build\\buffer-ctor'):
                node.value = str(REPORT.parent)
        return node

specs = [
    ('gui_login', 'D:/Reverse/BP-X360-Experimental-Artifacts/wave7-gui-caller-repair-1/validate-hide.py', 32),
    ('gui_splash', 'D:/Reverse/BP-X360-Experimental-Artifacts/wave7-gui-caller-repair-1/validate-splash.py', 40),
    ('gui_clear', 'D:/Reverse/BP-X360-Experimental-Artifacts/wave7-gui-caller-repair-1/validate-splash-clear.py', 32),
    ('scene_nonempty_wait', 'D:/Reverse/octree-layout-terra-build/final-exact/wait-query-witness.py', 8),
    ('scene_ctor', 'D:/Reverse/octree-layout-terra-build/final-exact/ctor-witness.py', 128),
    ('scene_add', 'D:/Reverse/octree-layout-terra-build/final-exact/entity-add-witness.py', 16),
    ('scene_result_ctor', 'D:/Reverse/octree-layout-terra-build/final-exact/buffer-ctor-witness.py', 128),
    ('scene_wait_receiver', str(ROOT / 'validate_loose_wait_receiver.py'), 4),
    ('iir_rounding', str(ROOT / 'validate_iir2_rounding.py'), 65),
]
assert sha(ROOT / 'validate_loose_wait_receiver.py') == '6d9784d4276b0ec4bb5dcbdaebf420e336b017e33ca489f760582466f5bf8d03'
assert sha(ROOT / 'validate_iir2_rounding.py') == 'd421aa6c89c5eb6a0db32a1f61838b718d5463afbb81b58ba80a1069bf36881b'
old_mkdir, old_open, old_argv = Path.mkdir, Path.open, sys.argv

def readonly(self, mode='r', *args, **kwargs):
    assert not any(c in mode for c in 'wax+'), ('unexpected write', str(self), mode)
    return old_open(self, mode, *args, **kwargs)

Path.mkdir = lambda *args, **kwargs: None
Path.open = readonly
records = []
try:
    for name, script, cases in specs:
        script = Path(script)
        original = name in ('scene_wait_receiver', 'iir_rounding')
        tree = ast.parse(script.read_text())
        if not original:
            tree = Rebind().visit(tree)
        ast.fix_missing_locations(tree)
        namespace = {'__file__': str(script), '__name__': 'bounded_reviewer_fixture'}
        sys.argv = [str(script)]
        if original:
            sys.argv.append(str(paths['CgsLooseOctree.cpp' if name == 'scene_wait_receiver' else 'Iir2.cpp']))
        capture = io.StringIO()
        with contextlib.redirect_stdout(capture):
            exec(compile(tree, str(script), 'exec'), namespace)
        result = namespace.get('report', namespace.get('result'))
        if name == 'scene_wait_receiver':
            assert all(r['receiver_and_arguments_equal'] for r in result['results'])
        elif name == 'iir_rounding':
            assert result['first_sample_rounding_witness']['equal']
            assert result['mismatch_count'] == 0
        elif isinstance(result, dict):
            assert result.get('passed', result.get('result') == 'pass')
        if result is None:
            result = {'result': 'pass', 'cases': cases}
        records.append({'test': name, 'script': str(script), 'script_sha256': sha(script),
                        'cases': cases, 'result': result})
finally:
    Path.mkdir, Path.open, sys.argv = old_mkdir, old_open, old_argv

print(json.dumps({'report': str(REPORT), 'report_sha256': sha(REPORT),
                  'head': '3534f7f16103a58ff904bbd1604e18e9934868f4',
                  'objects': {name: {'path': str(paths[name]), 'sha256': sha(paths[name])}
                              for name in required},
                  'cases': sum(r['cases'] for r in records), 'records': records,
                  'transform': 'Inspected worker AST: rebind object/report paths only and remove report-write expressions; Wait/IIR models unchanged. Block Path writes/mkdir and bytecode.',
                  'limits': 'Controlled hooks and software PPC rules remain those of the inspected fixtures. No asynchronous worker, allocator alignment, hardware, exceptional FP, performance or full-runtime proof.'}, indent=2))
