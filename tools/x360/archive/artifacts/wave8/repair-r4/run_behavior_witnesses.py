"""Rebind inspected unchanged parking/State models to the actual selected R4 objects."""
import ast
from contextlib import redirect_stdout
import hashlib
import io
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True

HERE = Path(__file__).resolve().parent
RAW = HERE / 'selected-stage1/compile-results.json'
report = json.loads(RAW.read_text(encoding='utf-8'))
assert report['state'] == 'complete'
objects = {Path(r['source']).name: r['object'] for r in report['results'] if r['passed']}
OUT = HERE / 'behavior-witnesses'
OUT.mkdir(exist_ok=True)


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def path_expr(path):
    return ast.Call(func=ast.Name(id='Path', ctx=ast.Load()), args=[ast.Constant(str(path))], keywords=[])


for name, source, original_hash, selection, outfile in [
    ('parking128', Path('D:/Reverse/BP-X360-Experimental-Artifacts/racecar_containment_sol_r1/validate_pre_replay_callers.py'),
     'e6fd4d690e64d54fbaf086f64c7044947e2262189d21803324eb6ffc6b39f9ba', 'parking', 'parking128-result.json'),
    ('state18_query_history', Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-static-sound-map-root/validate_consumers_state_final.py'),
     '8d1f36fdf570bdc6f3add064bad5822d9e93bd2c4db077383e4fbba144770e35', 'all', 'state18-query-history-result.json'),
]:
    assert sha(source) == original_hash, source
    output = OUT / outfile
    assert not output.exists()
    tree = ast.parse(source.read_text(encoding='utf-8'))
    changed = []
    for node in tree.body:
        if not isinstance(node, ast.Assign):
            continue
        names = {t.id for t in node.targets if isinstance(t, ast.Name)}
        if 'report_path' in names:
            node.value = path_expr(RAW)
            changed.append('report_path: actual complete selected84 raw report')
        elif 'paths' in names:
            node.value = ast.Dict(keys=[ast.Constant(k) for k in objects], values=[path_expr(v) for v in objects.values()])
            changed.append('paths: actual successful object lookup; raw failed rows remain unchanged')
        elif names & {'out', 'p'}:
            # These are the final result destinations in the inspected original scripts.
            node.value = path_expr(output)
            changed.append('result destination: new owned D artifact')
    ast.fix_missing_locations(tree)
    namespace = {'__name__': '__main__', '__file__': str(source)}
    sys.argv = [str(source), selection]
    log = io.StringIO()
    with redirect_stdout(log):
        exec(compile(tree, str(source), 'exec'), namespace)
    result = json.loads(output.read_text(encoding='utf-8'))
    assert result['mismatches'] == []
    expected = 128 if selection == 'parking' else 384
    assert result['cases'] == expected
    envelope = {'name': name, 'head': json.loads((HERE / 'repair-r4-receipts.json').read_text())['head'],
                'original_model': str(source), 'original_model_sha256': original_hash,
                'adaptation': changed, 'adapted_ast_sha256': hashlib.sha256(ast.dump(tree).encode()).hexdigest(),
                'selected_raw_report': str(RAW), 'selected_raw_sha256': sha(RAW),
                'cases': result['cases'], 'mismatches': 0, 'groups': result['groups'],
                'result': str(output), 'result_sha256': sha(output),
                'stdout': log.getvalue(), 'runner_sha256': sha(__file__),
                'limits': result['limits']}
    with (OUT / (name + '-binding.json')).open('x', encoding='utf-8') as stream:
        json.dump(envelope, stream, indent=2)
    print(json.dumps({'name': name, 'cases': expected, 'mismatches': 0, 'result_sha256': sha(output)}), flush=True)
