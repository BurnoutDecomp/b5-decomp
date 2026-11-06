"""Run the read-only reviewer witness, suppress import writes, save new evidence only."""
import builtins
import contextlib
import hashlib
import io
import json
from pathlib import Path
import runpy
import sys
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
fixture = HERE / 'validate_splice_constructor_snapshot.py'
assert hashlib.sha256(fixture.read_bytes()).hexdigest() == 'ad581616404b9c09ccacb19e108f8d7bb3f9c5117c094424e92c36ccd467f6e4'
meta_path = HERE / 'native-parallel-wave6-final2-patch.json'
meta = json.loads(meta_path.read_text())
report_path = HERE / 'final2-repair/compile-results.json'
report = json.loads(report_path.read_text())
assert report['state'] == 'complete' and report['passed'] == report['requested'] == 3
row = next(r for r in report['results'] if Path(r['source']).name == 'SpliceObjects.cpp')
sys.argv = [str(fixture), row['object']]
sys.dont_write_bytecode = True
output = io.StringIO()
suppressed = []
original_open, original_io_open = builtins.open, io.open


def guard_open(original):
    def guarded(file, mode='r', *args, **kwargs):
        assert not any(flag in mode for flag in 'wax+'), ('Unexpected fixture write', file, mode)
        return original(file, mode, *args, **kwargs)
    return guarded


def no_mkdir(path, *args, **kwargs):
    suppressed.append(str(path))


def no_write(*args, **kwargs):
    raise AssertionError('Unexpected fixture write')


with contextlib.redirect_stdout(output), patch.object(Path, 'mkdir', no_mkdir), \
        patch.object(Path, 'write_text', no_write), patch.object(Path, 'write_bytes', no_write), \
        patch('builtins.open', guard_open(original_open)), patch('io.open', guard_open(original_io_open)):
    runpy.run_path(str(fixture), run_name='__main__')
result = json.loads(output.getvalue())
evidence = {
    'head': meta['head'], 'patch_sha256': meta['sha256'],
    'metadata_sha256': hashlib.sha256(meta_path.read_bytes()).hexdigest(),
    'compile_report_sha256': hashlib.sha256(report_path.read_bytes()).hexdigest(),
    'fixture': str(fixture), 'fixture_sha256': hashlib.sha256(fixture.read_bytes()).hexdigest(),
    'suppressed_import_mkdir': suppressed, 'fixture_file_writes_blocked': True,
    'result': result,
    'scope': 'Three controlled source-equivalence modes against actual final2 compiled constructors; not an independently located ARTIST constructor or audio lifecycle.'}
with (HERE / 'final2-splice-mutation-comparison.json').open('x', encoding='utf-8') as stream:
    json.dump(evidence, stream, indent=2)
assert all(case['equivalent_ignoring_FindSplice'] for case in result['results'].values())
print('Final2 mutation witness: 3/3 modes equivalent; FindSplice once; object', result['object_sha256'])
