from pathlib import Path
import hashlib, json, struct, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
ORIGINAL = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
FIGS = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/DecFIGS_Burnout_Internal_PS3.ELF')

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

edit = read(HERE / 'before-edit.json')
before, after = read(HERE / 'source-before.json'), read(HERE / 'source-after.json')
assert git('rev-parse', 'HEAD^').decode().strip() == before['base'] == edit['base']
assert before['hashes'] == after['hashes'] and len(after['hashes']) == 6277
assert all(sha(ROOT / p) == v for p, v in after['hashes'].items())
assert set(git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines()) == set(edit['files'])
assert not git('diff', '--name-only').strip()
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B').decode()
r = read(HERE / 'native/compile-results.json')
assert r['state'] == 'complete' and r['passed'] == 4 and r['failed'] == 0
assert read(HERE / 'exit.json')['returncode'] == 0
headers, artifacts = {}, {}
for row in r['results']:
    assert row['passed'] and row['machine'] == 0x1F2
    assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1F2
    for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
        if 'including file:' in line:
            p = Path(line.split('including file:', 1)[1].strip()).resolve()
            headers[str(p)] = sha(p)
    for key in ('object', 'assembly', 'log'):
        artifacts[row[key]] = sha(row[key])
assert not any('wave4-intake' in p.lower() for p in headers)
comparison = read(HERE / 'native-comparison-final.json')
assert comparison['cases'] == 672 and not comparison['mismatches']
assert sha(comparison['object']) == comparison['object_sha256']
retention = read(HERE / 'retained-consumer-text.json')
assert sum(x['functions'] for x in retention['retained']) == 17
assert sum(x['bytes'] for x in retention['retained']) == 1764
tools = read(HERE.parent / 'native-formatter-root/DELIVERY.json')['compiler_and_runtime']
assert len(tools) == 7 and all(sha(p) == v for p, v in tools.items())
exports = [ORIGINAL / ('0x%s.json' % a) for a in ('82BBC878', '82BBC8D8', '82BBC918', '82BBD228', '82C73EB0')]
nodes = [FIGS / ('0x%s.json' % a) for a in ('DB1B30', 'DBA764', 'DBA7A4', 'DB1ACC')]
assert 'li        r5, 0x43' in read(exports[1])['assembly']
assert 'bl        atexit' in read(exports[-1])['assembly']
assert 'addi      r11, r11, 0x10' in read(exports[3])['assembly']
code = (ROOT / 'vendor/renderware/src/rw/OSGlobalManager.cpp').read_text()
assert '#include <intrin.h>' not in code and 'PC' not in code
assert code.count('CGS_STATIC_ASSERT(') == 5
assert 's_pOSGlobalManager->mRefCount.Increment();' in code
(HERE / 'owned.patch').write_bytes(git('diff', '--binary', 'HEAD^', 'HEAD'))
old_report = HERE.parent / 'wave7/full-stage1/compile-results.json'
old_row = next(row for row in read(old_report)['results'] if row['source'].endswith('rw/OSGlobalManager.cpp'))
assert not old_row['passed']
result = {'base': edit['base'], 'head': git('rev-parse', 'HEAD').decode().strip(),
          'sources': {name: sha(ROOT / name) for name in edit['files']},
          'git_blobs': {name: git('rev-parse', 'HEAD:' + name).decode().strip() for name in edit['files']},
          'patch_sha256': sha(HERE / 'owned.patch'),
          'native_report': str(HERE / 'native/compile-results.json'),
          'native_report_sha256': sha(HERE / 'native/compile-results.json'),
          'passed': 4, 'failed': 0, 'stable_source_count': 6277,
          'source_before_sha256': sha(HERE / 'source-before.json'),
          'source_after_sha256': sha(HERE / 'source-after.json'),
          'actual_headers': headers, 'header_count': len(headers), 'artifacts': artifacts,
          'compiler_and_runtime': tools,
          'original_exports': {str(p): sha(p) for p in exports},
          'decfigs_node_evidence': {str(p): sha(p) for p in nodes},
          'comparison': str(HERE / 'native-comparison-final.json'),
          'comparison_sha256': sha(HERE / 'native-comparison-final.json'),
          'comparison_groups': comparison['groups'], 'cases': 672,
          'retained_consumer_text': str(HERE / 'retained-consumer-text.json'),
          'retained_consumer_text_sha256': sha(HERE / 'retained-consumer-text.json'),
          'old_full_report': str(old_report), 'old_full_report_sha256': sha(old_report),
          'old_failure_log': old_row['log'], 'old_failure_log_sha256': sha(old_row['log']),
          'scope': 'Move the original anonymous/private manager out of the PC-size public vocabulary; native16-byte node sentinel, four-byte atomic, SDK28-byte critical section, manager48/align4; original67-byte aligned allocation and cookie. Genuine Init/Shutdown atomics/calls and non-gating manager increment. Restore CRT initialization and registered shutdown.',
          'limits': 'Original nested OSGlobalList declaration and exact atomic template signedness were not recovered; node-compatible sentinel and AtomicInt<unsigned int> represent the proven native member behavior/layout. Kernel services/atexit are controlled in672 software comparisons, including explicit original null-manager fault observation. Only process-counter relocation addresses are labeled in traces. Prior address/model failure reports and verifier versions are preserved. All17 executable sections/1764bytes and relocations retained for Formatter/Manager. No broad-header fresh full, strict link, real contention/hardware, image or launch claim; later-only outside current Wave7 mandatory repair cutoff.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'head': result['head'], 'passed': 4, 'cases': 672,
                  'header_count': len(headers), 'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
