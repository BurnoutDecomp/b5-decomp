import hashlib
import json
import re
import subprocess
from pathlib import Path

root = Path(r'C:\Users\adri1\.codex\worktrees\bp360\vertex_state_terra')
artifact = Path(__file__).parent
output = artifact / 'combined'
provider = 'a7e47de982c74ae5bfcbdf7e4477025d162ad21d'
donor = '849292f69191863d1ccc7cc52d392df722cd3c1a'
provider_paths = [
    'src/GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.h',
    'src/GameShared/GameClasses/System/FileSystem/CgsDeviceAsyncOp.cpp',
    'src/GameShared/GameClasses/System/FileSystem/Devices/CgsDeviceAsyncOp.cpp',
]
owner_paths = [
    'src/GameSource/Resource/BrnDLCManager.h',
    'src/GameSource/Resource/BrnDLCManager.cpp',
    'src/GameSource/Resource/BrnDLCManagerMonitor.cpp',
]

def git(*args):
    return subprocess.check_output(['git', *args], cwd=root, text=True).strip()

def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

def blob_binding(path, commit):
    expected = git('rev-parse', commit + ':' + path)
    filtered = git('hash-object', path)
    unfiltered = git('hash-object', '--no-filters', path)
    assert expected in (filtered, unfiltered), path
    return {'git_blob': expected, 'filtered_blob': filtered,
            'unfiltered_blob': unfiltered, 'sha256': sha(root / path)}

assert git('rev-parse', 'HEAD') == donor
report_path = output / 'compile-results.json'
report = json.loads(report_path.read_text(encoding='utf-8'))
assert report['state'] == 'complete' and report['requested'] == 10
assert report['passed'] == 10 and report['failed'] == 0 and len(report['results']) == 10
bindings = []
for path in provider_paths + owner_paths:
    expected_commit = provider if path in provider_paths else donor
    bindings.append({'path': path, 'expected_commit': expected_commit,
                     **blob_binding(path, expected_commit)})

objects = []
for row in report['results']:
    assert row['passed'] and row['returncode'] == 0 and row['machine'] == 0x01f2
    source = row['source']
    expected_commit = provider if source in provider_paths else donor
    binding = blob_binding(source, expected_commit)
    log = Path(row['log']).read_text(encoding='utf-8', errors='replace')
    assert not re.search(r'\b(?:warning|error)\b', log, re.IGNORECASE), source
    objects.append({'source': source, 'source_sha256': sha(root / source),
                    'git_blob': binding['git_blob'],
                    'filtered_blob': binding['filtered_blob'],
                    'unfiltered_blob': binding['unfiltered_blob'],
                    'object': row['object'], 'object_sha256': sha(row['object']),
                    'assembly': row['assembly'], 'assembly_sha256': sha(row['assembly'])})

checks = {}
for name in ('prepare-listing-check.json', 'monitor-listing-check.json'):
    check = json.loads((output / name).read_text(encoding='utf-8'))
    assert check['pass'], name
    checks[name] = sha(output / name)

result = {'stage': 'focused native compile', 'pass': True,
          'donor_head': donor, 'provider_commit': provider,
          'source_bindings': bindings, 'objects': objects,
          'compile_report_sha256': sha(report_path), 'listing_checks': checks,
          'provider_patch_sha256': sha(artifact / 'async-provider-snapshot.patch'),
          'scope_limit': 'No dependency-closure build, final link, XEX, or runtime verification.'}
(output / 'BINDING.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')
print(json.dumps({'pass': True, 'sources': len(objects), 'output': str(output / 'BINDING.json')}, indent=2))
