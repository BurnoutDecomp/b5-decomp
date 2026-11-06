"""Bind the already completed native DriveThru compile checkpoint."""
from pathlib import Path
import hashlib, json, subprocess, struct

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
SOURCE = 'src/GameSource/GameState/Offences/BrnDriveThruManager.cpp'
ARTIST = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def read(path):
    return json.loads(Path(path).read_text(encoding='utf-8'))

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

before = read(HERE / 'body-final1/input-before.json')
after = read(HERE / 'body-final1/input-after.json')
assert before['hashes'] == after['hashes'] and len(before['hashes']) == 6277
assert before['changed'] == [SOURCE]
assert sha(ROOT / SOURCE) == before['hashes'][SOURCE]
headers = read(HERE / 'body-final1/actual-headers.json')
assert headers['count'] == 393 and len(headers['hashes']) == 393
assert all(sha(path) == value for path, value in headers['hashes'].items())
assert not any('wave4-intake' in path.lower() for path in headers['hashes'])
audit = read(HERE.parent / 'wave6/full-stage1-terminal-audit.json')
tools = audit['external_inputs']['compiler_and_runtime']
assert len(tools) == 7 and all(sha(path) == value for path, value in tools.items())
report_path = HERE / 'body-final1/native/compile-results.json'
report = read(report_path)
assert report['state'] == 'complete' and report['passed'] == 3 and report['failed'] == 0
artifacts = {}
for row in report['results']:
    assert row['passed'] and row['returncode'] == 0 and row['machine'] == 0x1f2
    assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1f2
    for field in ('object', 'assembly', 'log'):
        artifacts[row[field]] = sha(row[field])
layout = read(HERE / 'body-final1/layout.json')
baseline = read(HERE / 'layout-baseline/layout-extracted.json')
assert layout['values'] == baseline['values']
assert layout['values']['size'] == 0x960 and layout['values']['progression'] == 0x950
assert layout['values']['trigger_size'] == 0x30
originals = {}
for address in ('823BD4E0', '8236E2E8', '8239B6E8', '823867A0', '82386840'):
    path = ARTIST / ('0x' + address + '.json')
    target = HERE / ('original-' + address + '.json')
    target.write_bytes(path.read_bytes())
    originals[str(target)] = {'source': str(path), 'sha256': sha(path)}
sizes = read(HERE / 'original-823BD4E0.json')['assembly'].splitlines()
construct = read(HERE / 'original-8236E2E8.json')['assembly'].splitlines()
assert any('0x823BF4EC' in line and '0x960' in line for line in sizes)
assert any('0x8236E324' in line and '0x950' in line for line in construct)
assert any('0x8236E334' in line and '0x30' in line for line in construct)
source = (ROOT / SOURCE).read_text(encoding='utf-8')
assert 'std::memset(lacPayload' not in source
assert 'JunctionInfoAction lJunctionInfo;' in source
assert source.count('std::memset(&mBoxRegionCache') == 2
head = git('rev-parse', 'HEAD').decode().strip()
assert git('rev-parse', 'HEAD^').decode().strip() == before['head']
assert git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines() == [SOURCE]
assert not git('diff', '--name-only').strip()
blob = git('show', 'HEAD:' + SOURCE)
assert hashlib.sha256(blob).hexdigest() == sha(ROOT / SOURCE)
patch = git('diff', '--binary', 'HEAD^', 'HEAD')
(HERE / 'owned.patch').write_bytes(patch)
result = {'base': before['head'], 'commit': head, 'source': SOURCE,
          'source_sha256': sha(ROOT / SOURCE), 'git_blob': git('rev-parse', 'HEAD:' + SOURCE).decode().strip(),
          'patch_sha256': sha(HERE / 'owned.patch'), 'report': str(report_path),
          'report_sha256': sha(report_path), 'actual_layout': layout['values'],
          'native_size_evidence': [line for line in sizes if '0x823BF4EC' in line],
          'native_final_pointer_evidence': [line for line in construct if '0x8236E324' in line],
          'native_stride_evidence': [line for line in construct if '0x8236E334' in line],
          'artifacts': artifacts, 'original_exports': originals, 'compiler_and_runtime': tools,
          'tracked_input_count': 6277, 'other_frozen_inputs_unchanged': 6276,
          'actual_included_headers': headers['count'], 'header_manifest': str(HERE / 'body-final1/actual-headers.json'),
          'header_manifest_sha256': sha(HERE / 'body-final1/actual-headers.json'),
          'baseline_note': 'The original measurement compiled successfully, then the v1 runner rejected '
                           'the C++-mangled data symbol. extract_layout.py read that same immutable object; '
                           'v2 fixes only the symbol lookup. No baseline rebuild or source change occurred.',
          'scope': 'Whole native DriveThru and GameStateModule CPPs plus actual-header layout. '
                   'Seven original absolute layout requirements replace an incorrect PC tail assumption. '
                   'No field, alignment, API or gate was fabricated or disabled.',
          'limits': 'Extra action zero-fills removed from original explicit store evidence; '
                    'no full-function semantic equivalence, whole vector ABI, link, image, runtime or hardware claim.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as stream:
    json.dump(result, stream, indent=2)
print(json.dumps({'commit': head, 'passed': report['passed'], 'headers': headers['count'],
                  'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
