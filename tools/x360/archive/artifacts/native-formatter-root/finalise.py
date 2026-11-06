from pathlib import Path
import hashlib, json, re, struct, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

edit = read(HERE / 'edit.json')
before, after = read(HERE / 'source-before.json'), read(HERE / 'source-after.json')
assert before['base'] == edit['base'] == git('rev-parse', 'HEAD^').decode().strip()
assert before['hashes'] == after['hashes']
assert len(before['hashes']) == 6277
assert all(sha(ROOT / p) == v for p, v in after['hashes'].items())
assert git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines() == [edit['path']]
assert not git('diff', '--name-only').strip()
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B').decode()
report = read(HERE / 'native/compile-results.json')
assert report['state'] == 'complete' and report['passed'] == 2 and report['failed'] == 0
assert read(HERE / 'exit.json')['returncode'] == 0
headers, artifacts = {}, {}
for row in report['results']:
    assert row['passed'] and row['machine'] == 0x1F2
    assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1F2
    for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
        if 'including file:' in line:
            p = Path(line.split('including file:', 1)[1].strip()).resolve()
            headers[str(p)] = sha(p)
    for key in ('object', 'assembly', 'log'):
        artifacts[row[key]] = sha(row[key])
assert not any('wave4-intake' in p.lower() for p in headers)
formatter = next(row for row in report['results'] if row['source'] == edit['path'])
listing = Path(formatter['assembly']).read_text(encoding='utf-8')
for name in ('_snprintf', '_strdate', '_strtime', 'vsnprintf'):
    assert re.search(r'\bbl\s+' + name + r'\b', listing)
original = read(read(HERE / 'crt-proof.json')['format_export'])['assembly']
assert 'li        r4, 0x20' in original and 'bl        sub_82C10930' in original
assert all('bl        ' + name in original for name in ('_strdate', '_strtime', 'vsnprintf'))
proof = read(HERE / 'crt-proof.json')
assert proof['snprintf']['size'] == 276
tools = read(HERE.parent / 'native-particle-description-root/DELIVERY.json')['compiler_and_runtime']
assert len(tools) == 7 and all(sha(p) == v for p, v in tools.items())
old_report = HERE.parent / 'wave7/full-stage1/compile-results.json'
old_row = next(row for row in read(old_report)['results'] if row['source'] == edit['path'])
assert not old_row['passed'] and old_row['returncode'] == 2
(HERE / 'owned.patch').write_bytes(git('diff', '--binary', 'HEAD^', 'HEAD'))
result = {'base': edit['base'], 'head': git('rev-parse', 'HEAD').decode().strip(),
          'source': edit['path'], 'source_sha256': sha(ROOT / edit['path']),
          'git_blob': git('rev-parse', 'HEAD:' + edit['path']).decode().strip(),
          'patch_sha256': sha(HERE / 'owned.patch'),
          'report': str(HERE / 'native/compile-results.json'),
          'report_sha256': sha(HERE / 'native/compile-results.json'),
          'passed': 2, 'failed': 0, 'stable_source_count': len(after['hashes']),
          'actual_headers': headers, 'header_count': len(headers), 'artifacts': artifacts,
          'compiler_and_runtime': tools,
          'source_before_sha256': sha(HERE / 'source-before.json'),
          'source_after_sha256': sha(HERE / 'source-after.json'),
          'crt_proof': str(HERE / 'crt-proof.json'), 'crt_proof_sha256': sha(HERE / 'crt-proof.json'),
          'old_full_report': str(old_report), 'old_full_report_sha256': sha(old_report),
          'old_failure_log': old_row['log'], 'old_failure_log_sha256': sha(old_row['log']),
          'scope': 'Four native CRT spellings in Formatter::Format; existing prefix order, sizes, va_list forwarding and allocation hooks retained.',
          'limits': 'Whole Formatter and Manager CPPs compile. Original helper identified by all 276 bytes after four genuine external branch relocations; Format itself is not claimed byte-matched or runtime-tested. No header, linker, GPU, image or launch change. Later candidate outside the closed Wave7 mandatory repair cohort.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'head': result['head'], 'passed': 2, 'failed': 0,
                  'headers': len(headers), 'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
