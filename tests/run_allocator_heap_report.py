"""Exercise the whole production allocator and the exact unchanged5faee8a control.

Keep the existing fxgs_common full-TU build recipe. Its one build/run subprocess
is delegated to the existing hash-bound Windows Job helper; no allocator body is
extracted or replaced. Root must make the exact baseline object available first.
"""
from __future__ import annotations

import argparse
from contextlib import nullcontext
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from types import SimpleNamespace

sys.dont_write_bytecode = True
REPO = Path(__file__).resolve().parents[1]
BASE = '5faee8a45c139e076bb0c7578568052026f587f2'
SOURCE = 'vendor/PPMalloc/src/EAGeneralAllocator.cpp'
HEADER = 'vendor/PPMalloc/include/ppmalloc/EAGeneralAllocator.h'
HARNESS = 'tests/AllocatorHeapReport.cpp'
HELPER = 'tests/online_save_route_owned.py'
COMMON = 'tests/fxgs_common.py'
TYPES = 'src/types.hpp'
PINS = {
    HARNESS: 'e31da557d335481a22c3af7bade394106a5f891121e08e97459833f069c196ce',
    HELPER: 'c21a018a37c094eec2c6befcd19b8862f8c33c84b66f84c33c55eac600649179',
    COMMON: '6230372da7ef60e446a165470a61850964b7b7498576eaec93dbb58555ae9033',
}
SOURCE_PINS = {
    SOURCE: '0d01fc549b5ae2456dfc52992b48857282e12cc5019db664b32429c9eed56db7',
    HEADER: 'e36a09f0c715921828a3dcaf21b3f904f9c73b5e6e198e3fa18f2d1fbf228e72',
    TYPES: '89b8022b07bab80fb0522f9a57e412761b0b787c6887f45b2653aa897b3aa308',
}
BASE_PINS = {
    SOURCE: ('5610cbeb18e0197d40b5c1c6b06813f98441c1133fbdce89d83893e19159d01f', '425b16e967b950e13ef0bfd03780c567cdab7881'),
    HEADER: ('e36a09f0c715921828a3dcaf21b3f904f9c73b5e6e198e3fa18f2d1fbf228e72', '9f026ef48004eef94f478d96657f4e1d76830125'),
    TYPES: ('89b8022b07bab80fb0522f9a57e412761b0b787c6887f45b2653aa897b3aa308', None),
}
EXPECTED_FAILURES = [
    'trace caller storage reports real live allocations and context',
    'trace owned storage hides its context and restores capacity',
    'trace callback and context defaults remain independent',
]


def identity(path: Path) -> dict:
    raw = path.read_bytes()
    return {'raw_bytes': len(raw), 'raw_sha256': hashlib.sha256(raw).hexdigest(),
            'lf_sha256': hashlib.sha256(raw.replace(b'\r\n', b'\n')).hexdigest()}


def write_json(path: Path, value) -> None:
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')


def load_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    if spec is None or spec.loader is None:
        raise RuntimeError('cannot load exact existing helper: ' + str(path))
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def validate(repo: Path) -> dict:
    files = {}
    for root, pins in ((REPO, PINS), (repo, SOURCE_PINS)):
        for relative, wanted in pins.items():
            path = (root / relative).resolve()
            got = identity(path)
            if not path.is_relative_to(root) or got['lf_sha256'] != wanted:
                raise RuntimeError('bound fixture/production source changed: ' + relative)
            files[str(path)] = got
    files[str(Path(__file__).resolve())] = identity(Path(__file__).resolve())
    return files


def good_run(run: dict, code: int) -> bool:
    cleanup = run.get('cleanup', {})
    return (run.get('exit_code') == code and run.get('timed_out') is False and
            run.get('owned_job') is True and run.get('transcript_hash_after_cleanup') is True and
            cleanup.get('complete') is True and cleanup.get('job_active_processes_after_cleanup') == 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', type=Path, help='exact candidate allocator checkout; defaults to this test checkout')
    parser.add_argument('--out', type=Path, help='fresh retained result directory; Root hosted step should always supply it')
    args = parser.parse_args()
    if os.name != 'nt':
        parser.error('the real production allocator and owned helper require Windows x64 MSVC')
    repo = args.source_root.resolve() if args.source_root else REPO
    workflow = next(parent for parent in REPO.parents if (parent / 'tools/build/msvc_flags.txt').is_file())
    if args.out and args.out.exists() and (not args.out.is_dir() or any(args.out.iterdir())):
        parser.error('fresh empty retained result directory required')
    directory = nullcontext(str(args.out.resolve())) if args.out else tempfile.TemporaryDirectory(prefix='allocator_trace26_')
    os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
    os.environ['VSLANG'] = '1033'
    with directory as value:
        out = Path(value)
        out.mkdir(parents=True, exist_ok=True)
        result = {'fixture': 'AllocatorHeapReport', 'success': False, 'executed': False,
                  'candidate_root': str(repo), 'baseline_commit': BASE, 'variants': {}, 'git_reads': []}
        try:
            result['inputs'] = validate(repo)
            result['workflow_inputs'] = {str(workflow / relative): identity(workflow / relative) for relative in
                ('tools/build/msvc_flags.txt', 'tools/build/msvc_includes.txt', 'tools/build/msvc_env.bat')}
            owned = load_module('allocator_exact_owned', REPO / HELPER)
            common = load_module('allocator_existing_common', REPO / COMMON)
            common.WORKFLOW = workflow
            git_path = shutil.which('git.exe')
            if not git_path:
                raise RuntimeError('Git is required to read the exact existing baseline object')

            def git_read(arguments: list[str], label: str) -> bytes:
                log = out / ('git_' + label + '.log')
                command = [str(Path(git_path).resolve()), '--no-optional-locks', '-C', str(repo), *arguments]
                run = owned.run_owned(command, repo, log, 20)
                result['git_reads'].append({'command': command, 'run': run})
                if not good_run(run, 0):
                    raise RuntimeError('exact baseline Git object unavailable; Root must fetch/prepare it before this runner')
                return log.read_bytes()

            if git_read(['rev-parse', '--verify', BASE + '^{commit}'], 'baseline_commit').decode().strip() != BASE:
                raise RuntimeError('exact baseline commit differs')
            result['candidate_head'] = git_read(['rev-parse', 'HEAD'], 'candidate_head').decode().strip()
            baseline = out / 'unchanged_5faee8a'
            result['baseline_sources'] = {}
            for index, (relative, (digest, blob)) in enumerate(BASE_PINS.items()):
                raw = git_read(['cat-file', 'blob', BASE + ':' + relative], 'baseline_blob_' + str(index))
                got_blob = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
                if hashlib.sha256(raw).hexdigest() != digest or (blob and got_blob != blob):
                    raise RuntimeError('unchanged whole production control differs: ' + relative)
                path = baseline / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(raw)
                result['baseline_sources'][relative] = {'path': str(path), 'git_blob': got_blob, **identity(path)}
            original_settings = common.settings
            for variant, source_root in (('candidate', repo), ('control', baseline)):
                def settings(name):
                    values = original_settings(name)
                    if name != 'msvc_includes.txt':
                        return values
                    mapped = []
                    for setting in values:
                        parts = Path(setting).parts
                        private = len(parts) >= 2 and parts[:2] == ('b5-decomp', 'src')
                        private |= len(parts) >= 3 and parts[:3] in (
                            ('b5-decomp', 'vendor', 'renderware'), ('b5-decomp', 'vendor', 'PPMalloc'))
                        mapped.append(str(source_root.joinpath(*parts[1:])) if private else setting)
                    return mapped
                common.settings = settings
                calls = []

                def run_command(command, *, cwd, capture_output, text):
                    # The existing common recipe has one cmd /c run.cmd operation.
                    # Put that compiler/linker/test tree in the existing bounded Job.
                    cwd = Path(cwd).resolve()
                    if (len(command) != 3 or command[0] != 'cmd' or command[1] != '/c' or
                        Path(command[2]).resolve() != (cwd / 'run.cmd')):
                        raise RuntimeError('unexpected common runner subprocess')
                    cmd = (Path(os.environ['SystemRoot']) / 'System32/cmd.exe').resolve()
                    actual = [str(cmd), *map(str, command[1:])]
                    log = cwd / 'owned_process.log'
                    run = owned.run_owned(actual, cwd, log, 120)
                    calls.append({'command': actual, 'run': run})
                    if identity(log)['raw_sha256'] != run.get('log_raw_sha256'):
                        raise RuntimeError('owned transcript changed after cleanup')
                    return SimpleNamespace(returncode=run['exit_code'], stdout=log.read_text('utf-8', errors='replace'), stderr='')
                common.subprocess = SimpleNamespace(run=run_command)
                dest = out / variant
                row = {'owned_processes': calls, 'whole_production_source': {'path': str(source_root / SOURCE), **identity(source_root / SOURCE)}}
                result['variants'][variant] = row
                result['executed'] = True
                macro = '/DALLOCATOR_TRACE_PREIMAGE_CONTROL=1' if variant == 'control' else ''
                numeric = common.compile_and_run(REPO / HARNESS, 'unused.inc', '', 'AllocatorHeapReport',
                    extra_flags=macro, extra_sources=[source_root / SOURCE], output_dir=dest)
                stdout = (dest / 'run.stdout.txt').read_text('utf-8')
                failures = re.findall(r'^FAIL (.+)$', stdout, re.MULTILINE)
                summaries = re.findall(r'^AllocatorHeapReport: (\d+) checks, (\d+) failures$', stdout, re.MULTILINE)
                expected = (26, 0) if variant == 'candidate' else (22, 3)
                code = 0 if variant == 'candidate' else 1
                wanted = [] if variant == 'candidate' else EXPECTED_FAILURES
                valid = (numeric == expected and len(summaries) == 1 and tuple(map(int, summaries[0])) == expected and
                         failures == wanted and len(calls) == 1 and good_run(calls[0]['run'], code))
                row.update({'numeric': numeric, 'expected_numeric': expected, 'failure_labels': failures,
                    'valid': valid, 'status': 'pass' if valid and variant == 'candidate' else 'expected_failure_observed' if valid else 'fail',
                    'artifacts': {str(path.relative_to(dest)): identity(path) for path in dest.iterdir() if path.is_file()}})
                if not valid:
                    raise RuntimeError('whole production allocator witness differs: ' + variant)
            if validate(repo) != result['inputs']:
                raise RuntimeError('bound fixture/helper/common/production inputs changed during run')
            for relative, entry in result['baseline_sources'].items():
                if identity(Path(entry['path'])) != {key: entry[key] for key in ('raw_bytes', 'raw_sha256', 'lf_sha256')}:
                    raise RuntimeError('unchanged baseline source changed during run: ' + relative)
            for path, before in result['workflow_inputs'].items():
                if identity(Path(path)) != before:
                    raise RuntimeError('canonical build input changed during run: ' + path)
            result['success'] = True
            print('AllocatorHeapReport hosted PASS: candidate26/26; unchanged5faee8a control22 checks with exactly3 missing-callback failures')
            return 0
        except Exception as error:
            result['error'] = str(error)
            print('AllocatorHeapReport FAIL: ' + str(error), file=sys.stderr)
            return 1
        finally:
            write_json(out / 'result.json', result)


if __name__ == '__main__':
    raise SystemExit(main())
