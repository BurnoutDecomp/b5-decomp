import hashlib
import json
import subprocess
from pathlib import Path

REPO = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\voip_worker_luna_4277')
ARTIFACT = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave41\voip-worker-luna')
BASE = '6fbd85919f6e848425bc89af9560dfef0200209a'
SOURCE = 'vendor/dirtysdk/src/x360/voip_x360.cpp'
SDK = Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp\build\x360\sdk-6534.4\portable\XDK')
COMPILER = SDK / 'bin/win32/cl.exe'


def git(*args, text=True):
    result = subprocess.run(['git', '-C', str(REPO), *args], check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout.decode('utf-8').strip() if text else result.stdout


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()

commit = git('rev-parse', 'HEAD')
parent = git('rev-parse', 'HEAD^')
branch = git('branch', '--show-current')
status = git('status', '--porcelain')
changed = git('diff', '--name-only', f'{BASE}..HEAD').splitlines()
assert parent == BASE, parent
assert status == '', status
assert changed == [SOURCE], changed

checkout_bytes = (REPO / SOURCE).read_bytes()
blob_bytes = git('show', f'HEAD:{SOURCE}', text=False)
normalized = checkout_bytes.replace(b'\r\n', b'\n')
assert normalized == blob_bytes
blob = git('rev-parse', f'HEAD:{SOURCE}')

compile_path = ARTIFACT / 'gate1/compile-results.json'
compile_report = json.loads(compile_path.read_text(encoding='utf-8'))
assert compile_report['state'] == 'complete'
assert compile_report['requested'] == 1
assert compile_report['passed'] == 1 and compile_report['failed'] == 0
row = compile_report['results'][0]
assert row['passed'] and row['returncode'] == 0 and row['machine'] == 0x01f2
assert row['source'] == SOURCE
object_path = Path(row['object'])
listing_path = Path(row['assembly'])
log_path = Path(row['log'])
assert object_path.is_file() and listing_path.is_file() and log_path.is_file()
argv = row['command']
assert argv[0] == str(COMPILER)
assert '/showIncludes' in argv
fallback_roots = [
    'vendor/EABase/include/Common', 'vendor/EASTL/include',
    'vendor/EAThread/include', 'vendor/renderware/include',
    'vendor/PPMalloc/include', 'vendor/coreallocator/include',
    'vendor/dirtysdk/include', 'vendor/zlib/include', 'vendor/lua/src'
]
for root in fallback_roots:
    expected = '/I' + str(Path(r'C:\Users\adri1\.codex\worktrees\fbaf\BP-Decomp_Workflow\b5-decomp') / root)
    assert expected in argv, expected

coff = json.loads((ARTIFACT / 'coff-symbols.json').read_text(encoding='utf-8'))
assert coff['machine'] == '0x1f2'
raw = json.loads((ARTIFACT / 'raw-evidence.json').read_text(encoding='utf-8'))
assert raw['sentinels']['0x82F264A8'] == 4
assert raw['sentinels']['0x82F26488+8*i'] == [194, 38, 63, 61]

artist_dir = Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
artist_hashes = {}
for addr in ('0x82B28688', '0x82B28CC0', '0x82B28E80'):
    artist_hashes[addr] = sha(artist_dir / f'{addr}.json')

compiler_version = (ARTIFACT / 'compiler-version.txt').read_text(encoding='utf-8').splitlines()[0]
assert 'Version 14.00.6502' in compiler_version
source_checkout_sha = hashlib.sha256(checkout_bytes).hexdigest()
source_blob_sha = hashlib.sha256(blob_bytes).hexdigest()

result = {
    'schema': 'r41-native-voip-worker-repair-v1',
    'candidate': {'commit': commit, 'parent': parent, 'branch': branch,
                  'worktree': str(REPO), 'clean': True,
                  'changedPaths': changed, 'sourceGitBlob': blob,
                  'sourceCheckoutSha256': source_checkout_sha,
                  'sourceCanonicalLfSha256': source_blob_sha,
                  'normalizedCheckoutEqualsGitBlob': True},
    'repairScope': [
        {'function': 'VoipRegisterRemoteTalker', 'artist': '0x82B28A98',
         'changes': ['return before XHV calls when remote XUID is zero',
                     'set VoipRef+0xFC mute-list dirty after either nonzero register or unregister path']},
        {'function': 'VoipXhvUpdate', 'artist': '0x82B28CC0',
         'changes': ['log the XNotifyGetNext r6 payload value',
                     'reload pStartedLocalMode after each returned GetLocalChatData buffer',
                     'reuse dataSize in/out value without resetting after VoipConnectionSend']},
        {'function': 'VoipUpdateVoiceActivity', 'artist': '0x82B28688',
         'changes': ['preserve all friends-mask bits for max peer count >= 32, matching slw(-1,max) complemented on PPC']}
    ],
    'artistRaw': raw,
    'artistExports': artist_hashes,
    'nativeCompile': {
        'wrapper': str(REPO / 'tools/build_x360.py'),
        'stage': compile_report['stage'], 'target': compile_report['target'],
        'sdk': compile_report['sdk'], 'compiler': str(COMPILER),
        'compilerVersion': compiler_version,
        'compilerSha256': sha(COMPILER), 'jobs': 1, 'showIncludes': True,
        'timeoutSeconds': 180, 'requested': 1, 'passed': 1, 'failed': 0,
        'source': SOURCE, 'command': argv,
        'fallbackIncludeRoots': fallback_roots,
        'compileResults': {'path': str(compile_path), 'sha256': sha(compile_path)},
        'object': {'path': str(object_path), 'sha256': sha(object_path),
                   'machine': '0x01f2'},
        'listing': {'path': str(listing_path), 'sha256': sha(listing_path)},
        'includeLog': {'path': str(log_path), 'sha256': sha(log_path)}
    },
    'coff': coff,
    'listingEvidence': [
        'VoipRegisterRemoteTalker: emitted zero-XUID compare/branch precedes XHV calls; dirty store at list+0xFC follows both nonzero branches.',
        'VoipUpdateVoiceActivity: emitted maxPeers>=32 branch selects 0xffffffff; smaller counts use shift-one/subtract-one.',
        'VoipXhvUpdate: emitted payload load from the stack word passed in r6; mode pointer reload occurs after each successful GetLocalChatData; next call reuses the same dataSize word after send.'
    ],
    'safety': {'liveTraffic': False, 'link': False, 'launch': False,
               'sharedOrCanonicalChanges': False,
               'forbiddenHelpersChanged': False,
               'runtimeTests': False}
}
(ARTIFACT / 'receipt.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

lines = [
    '# R41 native X360 VoIP worker repair receipt', '',
    '## Candidate', '',
    f'- Commit `{commit}`, parent `{parent}`, branch `{branch}`.',
    f'- Private worktree `{REPO}`; clean after commit.',
    f'- Only changed path: `{SOURCE}`. Its checkout line endings normalize exactly to committed blob `{blob}`.',
    '- Scope: `VoipRegisterRemoteTalker`, internal `VoipXhvUpdate`, and `VoipUpdateVoiceActivity` only.', '',
    '## ARTIST raw proof', '',
    '- Calibrated `x360rd.py` assertions passed: `u32(0x82F264A8)==4`; words at `0x82F26488+8*i` are `[194,38,63,61]`.',
    '- Raw image: `D:/Reverse/IDA_Files/artist_i64.raw`; Capstone PPC big-endian 5.0.7. See `raw-ppc-disassembly.txt` and `raw-evidence.json` for range hashes.',
    '- `0x82B28A98`: `ld` XUID; `cmpldi` + `beq` skips calls at `0x82B28AAC-0xB8`. Both nonzero branches converge at `0x82B28B34-0xB38`, storing 1 to `0xFC(r30)`.',
    '- `0x82B286C0-0xD4`: loads `-1`, `slw`s by max peers, `nor`s the result, then stores it at `0xDC(r31)`. A shift count of 32 makes `slw` return zero, so the complement is all bits set.',
    '- `0x82B28CD0-0xD00`: r6 is the `pParam` output; the log reads the DWORD at that address. `0x82B28DF4-0xE58` reloads active mode per returned voice buffer and calls `GetLocalChatData` again with the same size word after sending.', '',
    '## Targeted native compile and COFF', '',
    f'- Portable Xbox 360 SDK 6534.4 compiler: `{COMPILER}`; {compiler_version}; SHA-256 `{sha(COMPILER)}`.',
    '- Jobs 1, `/showIncludes`, all nine canonical fallback include roots, timeout 180 seconds.',
    f'- Compile gate: 1 requested, 1 passed, 0 failed. Result: `{compile_path}`.',
    f'- PPC big-endian COFF object: `{object_path}` (machine `0x01f2`), assembly listing `{listing_path}`, include log `{log_path}`.',
    '- COFF inspection found function definitions for `VoipRegisterRemoteTalker`, `VoipUpdateVoiceActivity`, `VoipXhvUpdate`, and `VoipWorkerThread`; see `coff-symbols.json`.',
    '- The emitted assembly shows the zero-XUID early return, dirty-state store, max-peer `>=32` all-ones branch, payload load, per-buffer mode reload, and data-size reuse.', '',
    'No live traffic, link, or launch was performed. No shared checkout, intake, canonical state, other helper, or other source path was changed.'
]
(ARTIFACT / 'RECEIPT.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print(json.dumps({'commit': commit, 'parent': parent, 'changedPaths': changed,
                  'objectMachine': coff['machine'], 'symbols': [s['name'] for s in coff['definitions']],
                  'receipt': str(ARTIFACT / 'RECEIPT.md')}, indent=2))
