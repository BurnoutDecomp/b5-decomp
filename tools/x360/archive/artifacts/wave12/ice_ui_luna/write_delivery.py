from pathlib import Path
import hashlib, json, os, subprocess

ROOT = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\gui_events_cpp03_luna_r10')
ARTIFACT = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave12\ice_ui_luna')
RUN = ARTIFACT / 'native-compile-01'
REPORT = RUN / 'compile-results.json'
BASE = '3200b4b62daf439ea358909ed98ec49dc2c5c7f7'
EXPECTED_BRANCH = 'codex/experimental-x360-ice-ui-luna-r16'
FALLBACK_ROOTS = [
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EABase\include\Common',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EASTL\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\EAThread\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\renderware\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\PPMalloc\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\coreallocator\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\dirtysdk\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\zlib\include',
    r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\vendor\lua\src',
]
EXPECTED_SOURCES = [
    'src/SDKs/Packages/ICE/ICEControllerRender.cpp',
    'src/SDKs/Packages/ICE/ICEWidget/ICEWidgetIcon.cpp',
    'src/SDKs/Packages/ICE/ICEWidget/ICEWidgetMenu.cpp',
    'src/SDKs/Packages/ICE/ICEWidget/ICEWidgetTimeBar.cpp',
    'src/SDKs/Packages/ICE/ICEWidget/ICEWidgetWideScrMarker.cpp',
]

def git(*args, binary=False):
    result = subprocess.run(['git', '-C', str(ROOT), *args], check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout if binary else result.stdout.decode('utf-8').strip()

def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()

def sha_file(path):
    return sha_bytes(Path(path).read_bytes())

def file_info(path):
    p = Path(path)
    return {'path': str(p), 'bytes': p.stat().st_size, 'sha256': sha_file(p)}

def git_blob(ref, rel):
    spec = f'{ref}:{rel}'
    oid = git('rev-parse', spec)
    data = git('cat-file', 'blob', spec, binary=True)
    return oid, data

report = json.loads(REPORT.read_text(encoding='utf-8'))
if report.get('state') != 'complete' or report.get('target') != 'xbox360-ppc-be':
    raise SystemExit('Native compile report is not complete for xbox360-ppc-be')
if len(report.get('results', [])) != 5:
    raise SystemExit('Expected exactly five selected native compile results')
if sorted(r['source'] for r in report['results']) != sorted(EXPECTED_SOURCES):
    raise SystemExit('Compile report source selection differs from assigned five CPPs')
if any(not r.get('passed') or int(r.get('machine', -1)) != 0x1F2 for r in report['results']):
    raise SystemExit('A selected source failed or did not produce X360 machine 0x1F2')

branch = git('branch', '--show-current')
if branch != EXPECTED_BRANCH:
    raise SystemExit(f'Unexpected branch {branch}')
commit = git('rev-parse', 'HEAD')
commit_tree = git('rev-parse', 'HEAD^{tree}')
base_tree = git('rev-parse', f'{BASE}^{{tree}}')
parent = git('rev-parse', 'HEAD^')
if parent != BASE:
    raise SystemExit(f'Commit parent {parent} does not equal assigned base {BASE}')
status = git('status', '--porcelain=v1')
if status:
    raise SystemExit('Working tree is not clean after commit: ' + status)
committed_paths = git('diff-tree', '--no-commit-id', '--name-only', '-r', 'HEAD').splitlines()
if sorted(committed_paths) != sorted(EXPECTED_SOURCES):
    raise SystemExit('Commit does not contain exactly the five assigned CPP paths')
commit_message = git('show', '-s', '--format=%B', 'HEAD')
if 'Co-Authored-By: GPT-6 Luna Max <noreply@openai.com>' not in commit_message:
    raise SystemExit('Expected actual model co-author trailer is missing')

sdk = report['sdk']
compiler_path = report['results'][0]['command'][0]
compiler_hash = sha_file(compiler_path)
report_info = file_info(REPORT)

source_records = []
source_bindings = []
unique_includes = {}
exact_include_spellings = set()
raw_include_occurrences = 0
for result in report['results']:
    rel = result['source']
    path = ROOT / rel
    pre_oid, pre_git_bytes = git_blob(BASE, rel)
    post_oid, post_git_bytes = git_blob('HEAD', rel)
    current_bytes = path.read_bytes()
    normalized_oid = git('hash-object', str(path))
    raw_worktree_oid = git('hash-object', '--no-filters', str(path))
    if normalized_oid != post_oid:
        raise SystemExit(f'Clean-filtered worktree object does not match commit for {rel}')

    crlf_count = current_bytes.count(b'\r\n')
    lf_count = current_bytes.count(b'\n')
    bare_lf_count = lf_count - crlf_count
    bare_cr_count = current_bytes.count(b'\r') - crlf_count
    if crlf_count and bare_lf_count == 0 and bare_cr_count == 0:
        eol_mode = 'CRLF-only'
    elif lf_count and crlf_count == 0 and bare_cr_count == 0:
        eol_mode = 'LF-only'
    else:
        eol_mode = 'mixed-or-no-newlines'

    autocrlf = git('config', '--get', 'core.autocrlf')
    pre_worktree_sha256 = None
    pre_worktree_method = None
    if eol_mode == 'CRLF-only' and autocrlf.lower() == 'true':
        lf_blob = pre_git_bytes.replace(b'\r\n', b'\n')
        pre_checkout = lf_blob.replace(b'\n', b'\r\n')
        pre_worktree_sha256 = sha_bytes(pre_checkout)
        pre_worktree_method = 'reconstructed from clean base blob using core.autocrlf=true and observed CRLF-only checkout'
    elif eol_mode == 'LF-only' and autocrlf.lower() in ('false', 'input', ''):
        pre_worktree_sha256 = sha_bytes(pre_git_bytes)
        pre_worktree_method = 'reconstructed from clean base blob with observed LF-only checkout'

    outputs = {}
    for key in ('object', 'assembly', 'log'):
        outputs[key] = file_info(result[key])

    include_rows = []
    log_text = Path(result['log']).read_text(encoding='utf-8', errors='replace')
    for line in log_text.splitlines():
        marker = 'Note: including file:'
        if not line.startswith(marker):
            continue
        raw = line[len(marker):].strip()
        exact_include_spellings.add(raw)
        include_path = Path(raw)
        if not include_path.is_absolute():
            include_path = (ROOT / include_path).resolve()
        else:
            include_path = include_path.resolve()
        if not include_path.is_file():
            raise SystemExit(f'Resolved /showIncludes file missing on disk: {include_path}')
        include_key = os.path.normcase(os.path.normpath(str(include_path)))
        include_hash = sha_file(include_path)
        row = {'path': str(include_path), 'sha256': include_hash}
        include_rows.append(row)
        raw_include_occurrences += 1
        entry = unique_includes.setdefault(include_key, {
            'path': str(include_path), 'sha256': include_hash, 'used_by': [], 'occurrences': 0})
        if entry['sha256'] != include_hash:
            raise SystemExit(f'Inconsistent include hash for {include_path}')
        entry['occurrences'] += 1
        if rel not in entry['used_by']:
            entry['used_by'].append(rel)

    command = result['command']
    if '/showIncludes' not in command:
        raise SystemExit(f'/showIncludes missing from compiler command for {rel}')
    for root in FALLBACK_ROOTS:
        if '/I' + root.replace('\\', '/') not in command:
            raise SystemExit(f'Canonical fallback root missing from command: {root}')

    source_records.append({
        'path': rel,
        'pre': {
            'git_blob_oid_sha1': pre_oid,
            'git_blob_content_sha256': sha_bytes(pre_git_bytes),
            'reconstructed_clean_worktree_sha256': pre_worktree_sha256,
            'reconstructed_worktree_method': pre_worktree_method,
        },
        'post': {
            'git_blob_oid_sha1': post_oid,
            'git_blob_content_sha256': sha_bytes(post_git_bytes),
            'worktree_bytes_sha256': sha_bytes(current_bytes),
            'worktree_git_blob_oid_without_filters': raw_worktree_oid,
            'worktree_git_blob_oid_with_clean_filters': normalized_oid,
            'worktree_eol': eol_mode,
        },
        'native_compile': {
            'passed': bool(result['passed']),
            'machine_decimal': int(result['machine']),
            'machine_hex': f"0x{int(result['machine']):03X}",
            'compiler': compiler_path,
            'compiler_sha256': compiler_hash,
            'command': command,
            'object': outputs['object'],
            'assembly_listing': outputs['assembly'],
            'log': outputs['log'],
            'resolved_include_occurrences': len(include_rows),
        },
    })
    source_bindings.append({'source': rel, 'resolved_includes_in_log_order': include_rows})

if len(exact_include_spellings) != 157:
    raise SystemExit(f'Expected 157 distinct /showIncludes path spellings, found {len(exact_include_spellings)}')
if len(unique_includes) != 148:
    raise SystemExit(f'Expected 148 case-normalized resolved include files, found {len(unique_includes)}')

primary_export_specs = [
    ('target_box_four_lane_transform', r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x8253A008.json'),
    ('widescreen_marker_alpha_and_calls', r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x82537B88.json'),
    ('render_poly_lane_use_and_colour', r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX\0x82533578.json'),
]
primary_exports = [{'purpose': purpose, **file_info(path)} for purpose, path in primary_export_specs]

vector_headers = [
    ROOT / 'src/SDKs/EATech/include/rw/math/vpu/vector3.h',
    ROOT / 'src/SDKs/EATech/include/rw/math/vpu/vector4.h',
    ROOT / 'src/SDKs/EATech/include/rw/math/vpu/detail/vector3_type_inline.h',
    ROOT / 'src/SDKs/EATech/include/rw/math/vpu/detail/vector4_type_inline.h',
    ROOT / 'src/SDKs/EATech/include/rw/math/vpu/detail/vector4_operation_inline.h',
    ROOT / 'vendor/renderware/include/rw/math/vpu/types.h',
]
native_vector_headers = [file_info(p) for p in vector_headers]

all_source_commands = []
for result in report['results']:
    all_source_commands.append({'source': result['source'], 'argv': result['command']})

receipt = {
    'schema_version': 1,
    'task': 'R16 native ICE UI source repairs',
    'branch': branch,
    'base_commit': BASE,
    'base_tree': base_tree,
    'commit': commit,
    'final_tree': commit_tree,
    'commit_parent': parent,
    'committed_paths': committed_paths,
    'working_tree_clean': True,
    'commit_subject': git('show', '-s', '--format=%s', 'HEAD'),
    'coauthor_trailer': 'Co-Authored-By: GPT-6 Luna Max <noreply@openai.com>',
    'source_hash_conventions': {
        'git_blob_oid_sha1': 'SHA-1 object ID from git rev-parse revision:path over committed Git-normalized content.',
        'git_blob_content_sha256': 'SHA-256 of exact bytes returned by git cat-file blob for the committed source.',
        'worktree_bytes_sha256': 'SHA-256 of exact on-disk source bytes, including checkout line endings.',
        'worktree_git_blob_oid_without_filters': 'Git blob object ID computed from exact on-disk bytes using git hash-object --no-filters.',
        'pre_worktree_hash_note': 'Pre worktree SHA-256 is reconstructed only where the assigned checkout was clean, core.autocrlf=true, and current files show CRLF-only; the base blob is converted LF to CRLF.',
    },
    'sources': source_records,
    'native_compile': {
        'state': report['state'],
        'target': report['target'],
        'requested': report['requested'],
        'passed': sum(1 for r in report['results'] if r['passed']),
        'failed': sum(1 for r in report['results'] if not r['passed']),
        'machine_hex': '0x1F2',
        'sdk': sdk,
        'driver': {
            'script': str(ROOT / 'tools/build_x360.py'),
            'script_sha256': sha_file(ROOT / 'tools/build_x360.py'),
            'command': ['python', 'tools/build_x360.py', '--sdk', sdk, '--jobs', '1', '--timeout', '180', '--out', str(RUN),
                        *sum((['--source', s] for s in EXPECTED_SOURCES), []),
                        '--compiler-arg=/showIncludes',
                        *['--compiler-arg=/I' + r.replace('\\', '/') for r in FALLBACK_ROOTS]],
        },
        'compiler': {'path': compiler_path, 'sha256': compiler_hash},
        'report': report_info,
        'per_source_commands': all_source_commands,
    },
    'include_roots': {
        'canonical_fbaf_fallback_roots': FALLBACK_ROOTS,
        'bindings': source_bindings,
        'unique_resolved_files': len(unique_includes),
        'distinct_log_path_spellings': len(exact_include_spellings),
        'include_occurrences': raw_include_occurrences,
        'resolved_file_hashes': sorted(unique_includes.values(), key=lambda x: x['path'].casefold()),
    },
    'primary_x360_exports': primary_exports,
    'native_vector_headers': native_vector_headers,
    'change_summary': [
        'Replaced C++11-style braced assignments and nonexistent public vector fields with the native C++03 Vector3 and Vector4 constructors/accessors.',
        'Zeroed each ICE backdrop Vector3 with SetZero so all four register lanes are initialized.',
        'Reconstructed ICEController::RenderTargetBox with the full four-lane Vector4 multiply, two fused multiply-adds, vector add, and full-register store shown by ARTIST 0x8253A008.',
        'Set the widescreen marker RGBA vector to the ARTIST-exported {0, 0, 0, 200}; the -2.0 value remains the left-edge coordinate.',
        'Kept the native widget and controller debug draw calls and branch gates; no headers, layouts, core files, facades, stubs, or bypassed guards were changed.',
    ],
    'verification_limits': [
        'The evidence is five selected XDK compile-only gates with native object listings. It does not establish linking, launch, runtime rendering, gameplay behavior, or whole-campaign numerical byte parity.',
        'No full sweep, link, game run, or unrelated tests were run.',
    ],
    'raw_x360_decoder_fixtures_used': False,
}

out = ARTIFACT / 'DELIVERY.json'
out.write_text(json.dumps(receipt, indent=2) + '\n', encoding='utf-8')
print(json.dumps({
    'delivery': str(out),
    'delivery_bytes': out.stat().st_size,
    'commit': commit,
    'tree': commit_tree,
    'source_count': len(source_records),
    'unique_resolved_includes': len(unique_includes),
    'distinct_log_path_spellings': len(exact_include_spellings),
    'include_occurrences': raw_include_occurrences,
    'report_sha256': report_info['sha256'],
}, indent=2))
