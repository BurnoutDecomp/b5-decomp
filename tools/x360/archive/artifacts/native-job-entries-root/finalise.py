from pathlib import Path
import hashlib, json, struct, subprocess

ROOT = Path('D:/Reverse/BP-X360-Experimental-Worktrees/drive-thru-native-root')
HERE = Path(__file__).resolve().parent
AUTHOR = HERE.parent / 'parallel/job_entries_author_terra'
PROVIDER = HERE.parent / 'parallel/relocator_job_terra'

def read(p):
    return json.loads(Path(p).read_text(encoding='utf-8'))

def sha(p):
    return hashlib.sha256(Path(p).read_bytes()).hexdigest()

def git(*args):
    return subprocess.check_output(['git', '-C', str(ROOT), *args])

edit, polygon = read(HERE / 'before-edit.json'), read(HERE / 'polygon-before-edit.json')
owned = sorted(set(edit['files']) | set(polygon))
assert len(owned) == 8
assert git('rev-parse', 'HEAD^').decode().strip() == edit['base']
assert git('rev-parse', 'HEAD').decode().strip() == '9acbf432092522daf57bfc087b42c9a0f9375f46'
assert git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').decode().splitlines() == owned
assert not git('diff', '--name-only').strip() and not git('diff', '--cached', '--name-only').strip()
assert 'Co-Authored-By: Codex GPT-6.1 Sol <noreply@openai.com>' in git('log', '-1', '--format=%B').decode()
before, after = read(HERE / 'source-before.json'), read(HERE / 'source-after.json')
assert before['hashes'] == after['hashes'] and len(after['hashes']) == 6450
assert all(sha(ROOT / p) == v for p, v in after['hashes'].items())
r = read(HERE / 'native/compile-results.json')
assert r['state'] == 'complete' and r['passed'] == r['requested'] == 17 and r['failed'] == 0
assert read(HERE / 'exit.json')['returncode'] == 0
headers, artifacts = {}, {}
for row in r['results']:
    assert not any(part.lower() == 'pc' for part in Path(row['source']).parts)
    assert row['passed'] and row['machine'] == 0x1F2
    assert struct.unpack_from('<H', Path(row['object']).read_bytes())[0] == 0x1F2
    for line in Path(row['log']).read_text(encoding='utf-8').splitlines():
        if 'including file:' in line:
            p = Path(line.split('including file:', 1)[1].strip()).resolve()
            headers[str(p)] = sha(p)
    for key in ('object', 'assembly', 'log'):
        artifacts[row[key]] = sha(row[key])
assert not any('wave4-intake' in p.lower() for p in headers)
own = read(HERE / 'job-id-arena-comparison.json')
assert own['cases'] == 1536 and not own['mismatches']
assert own['old_negative']['total'] == len(own['old_negative']['failed_cases']) == 512
author = read(AUTHOR / 'RECEIPT.json')
assert author['cases'] == {'four_entries': 32, 'linked_relocator_entry_worker': 48}
assert author['source_snapshot_equal']
assert sha(HERE / 'source-before.json') == author['source_snapshot_before_sha256']
assert sha(HERE / 'source-after.json') == author['source_snapshot_after_sha256']
for source, value in author['object_sha256'].items():
    row = next(row for row in r['results'] if row['source'] == source)
    assert sha(row['object']) == value
assert read(AUTHOR / 'entry-comparison.json')['cases'] == 32
linked = read(AUTHOR / 'linked-relocator-comparison.json')
assert linked['cases'] == 48 and not linked['mismatches']
combined = PROVIDER / 'combined-792/COMBINED_RECEIPT.json'
assert sha(combined) == '2c9bc8e3575a4485bfe19ed53fc3c69ac1337da7da540c45d7ed49482b916133'
tools = read(HERE.parent / 'native-osglobal-root/DELIVERY.json')['compiler_and_runtime']
assert len(tools) == 7 and all(sha(p) == v for p, v in tools.items())
(HERE / 'owned.patch').write_bytes(git('diff', '--binary', 'HEAD^', 'HEAD'))
prereqs = HERE / 'foreign-prerequisites.json'
scope = [HERE / 'job-id-arena-comparison.json', AUTHOR / 'RECEIPT.json',
         AUTHOR / 'entry-comparison.json', AUTHOR / 'linked-relocator-comparison.json',
         combined, PROVIDER / 'WORKER_PROVIDER_RECEIPT.json',
         Path('D:/Reverse/job-assert-terra-build/job-worker-original-constraints.json')]
result = {'base': edit['base'], 'head': git('rev-parse', 'HEAD').decode().strip(),
          'sources': {p: sha(ROOT / p) for p in owned},
          'git_blobs': {p: git('rev-parse', 'HEAD:' + p).decode().strip() for p in owned},
          'patch_sha256': sha(HERE / 'owned.patch'),
          'foreign_prerequisites': read(prereqs), 'foreign_prerequisites_sha256': sha(prereqs),
          'native_report': str(HERE / 'native/compile-results.json'),
          'native_report_sha256': sha(HERE / 'native/compile-results.json'),
          'passed': 17, 'failed': 0, 'stable_tracked_code_inputs': 6450,
          'source_before_sha256': sha(HERE / 'source-before.json'),
          'source_after_sha256': sha(HERE / 'source-after.json'),
          'actual_headers': headers, 'header_count': len(headers),
          'artifacts': artifacts, 'compiler_and_runtime': tools,
          'evidence': {str(p): sha(p) for p in scope},
          'comparison_cases': {'thread_id_conversion': 512, 'thread_id_getter': 512,
                               'polygon_allocator_in_bounds': 512,
                               'four_entry_points': 32, 'linked_relocator': 48},
          'negative_cases': {'old_wrong_polygon_arena': 512},
          'new_native_cpp': 'src/SDKs/EATech/eajobs/job_thread_id.cpp',
          'scope': 'Four native worker callbacks with six native handle-derived contexts, genuine two job-id APIs, Relocator r5 worker ID, shared8192-byte diagnostic buffer and two native assertion/MSR sequences. Polygon arena/cursor/tag/stride original offsets restored; observed64-byte global ARRAY STORAGE property preserved independently of unrecovered type/member alignment.',
          'limits': 'All6450 tracked code hashes include inert PC files as provenance only;17 actual native CPP inputs exclude PC paths. Comparisons execute original/compiled PPC in software; thread/diagnostic/copy services controlled and privileged effects modeled. Polygon allocator checks stay below pre-existing overflow assertion. Original Polygon type/member declarations remain unknown. Relocator prerequisites retain explicitly authorized unrecovered prefix+04..0F and maximum-record storage representations, not original source bound claims. Author witnesses ran before Git commit against identical frozen raw sources; combined foreign checkout differs only by line endings for six files. No fresh broad-header full sweep, strict whole-program link, real threading/hardware, image or launch claim.',
          'verifier_sha256': sha(__file__)}
with (HERE / 'DELIVERY.json').open('x', encoding='utf-8') as f:
    json.dump(result, f, indent=2)
print(json.dumps({'head': result['head'], 'passed': 17, 'cases': 1616,
                  'headers': len(headers), 'delivery_sha256': sha(HERE / 'DELIVERY.json')}, indent=2))
