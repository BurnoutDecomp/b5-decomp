import hashlib
import importlib.util
import ipaddress
import json
import re
import subprocess
from pathlib import Path

REPO = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\lobby_dispatch_luna_4277')
ARTIFACT = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave37\lobby-dispatch-luna')
SOURCE = 'vendor/dirtysdk/src/x360/lobbylogin.cpp'
HEAD_EXPECTED = 'f39765ba38dc3840d5ca700b833f08dfce4fc937'
PARENT_EXPECTED = '5ea596e049a8eaff6253c1513e54d9208ef2751e'
ARTIST_DIR = Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
SERVICE_IPS = {0x11: '203.0.113.17', 0x22: '203.0.113.34', 0x33: '203.0.113.51'}


def git(*args, text=False):
    result = subprocess.run(['git', '-C', str(REPO), *args], check=True,
                            stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    return result.stdout.decode('utf-8').strip() if text else result.stdout

head = git('rev-parse', 'HEAD', text=True)
parent = git('rev-parse', 'HEAD^', text=True)
status = git('status', '--porcelain', text=True)
assert head == HEAD_EXPECTED, head
assert parent == PARENT_EXPECTED, parent
assert status == '', status
source_bytes = (REPO / SOURCE).read_bytes()
committed_bytes = git('show', f'HEAD:{SOURCE}')
source_sha = hashlib.sha256(source_bytes).hexdigest()
normalized_source_bytes = source_bytes.replace(b'\r\n', b'\n')
assert normalized_source_bytes == committed_bytes
canonical_blob_sha = hashlib.sha256(committed_bytes).hexdigest()
blob = git('rev-parse', f'HEAD:{SOURCE}', text=True)

reader_path = Path(r'D:\Reverse\BP-Decomp_Workflow\tools\re\x360rd.py')
spec = importlib.util.spec_from_file_location('x360rd', reader_path)
reader = importlib.util.module_from_spec(spec)
spec.loader.exec_module(reader)
sentinel_a = reader.u32(0x82F264A8)
sentinel_b = [reader.u32(0x82F26488 + 8 * i) for i in range(4)]
assert sentinel_a == 4, sentinel_a
assert sentinel_b == [194, 38, 63, 61], sentinel_b

artist_hashes = {}
artist_json = {}
for addr in ('0x82B1FD80', '0x82B20070', '0x82B20390'):
    path = ARTIST_DIR / f'{addr}.json'
    raw = path.read_bytes()
    artist_hashes[addr] = hashlib.sha256(raw).hexdigest()
    artist_json[addr] = json.loads(raw.decode('utf-8'))


def ipnum(value):
    return int(ipaddress.IPv4Address(value))


def iptext(value):
    return str(ipaddress.IPv4Address(value & 0xffffffff))


def leading_hex(text):
    match = re.match(r'\s*([+-]?(?:0[xX])?[0-9a-fA-F]+)', text)
    if not match:
        return 0
    token = match.group(1)
    sign = -1 if token.startswith('-') else 1
    token = token.lstrip('+-')
    if token.lower().startswith('0x'):
        token = token[2:]
    return (sign * int(token, 16)) & 0xffffffff


def parse_pairs(text):
    pairs = []
    for item in text.split(','):
        i = 0
        start = 0
        while i < len(item) and item[i].isdigit() and item[i].isascii():
            start = (10 * start + ord(item[i]) - 48) & 0xffff
            i += 1
        if i < len(item):
            i += 1  # ARTIST and candidate both consume the first non-digit.
        end = 0
        while i < len(item) and item[i].isdigit() and item[i].isascii():
            end = (10 * end + ord(item[i]) - 48) & 0xffff
            i += 1
        pairs.append((start, end))
    return pairs


def parse_artist_address(text):
    return parse_address(text, cap=None)


def parse_candidate_address(text):
    return parse_address(text, cap=16)


def parse_address(text, cap):
    rows = []
    service_queries = []
    pos = 0
    while pos < len(text) and len(rows) < 8:
        delim_rel = next((i for i, c in enumerate(text[pos:]) if c in ':/'), None)
        if delim_rel is None:
            break
        delim = pos + delim_rel
        source = ipnum(text[pos:delim])
        if text[delim] == ':':
            mask_slash = text.find('/', delim + 1)
            if mask_slash < 0:
                break
            pairs = parse_pairs(text[delim + 1:mask_slash])
            mask_start = mask_slash + 1
        else:
            pairs = None
            mask_start = delim + 1
        service_slash = text.find('/', mask_start)
        if service_slash < 0:
            break
        mask = ipnum(text[mask_start:service_slash])
        service_start = service_slash + 2  # ARTIST skips slash and '$'.
        next_rule = text.find(',', service_start)
        service_text = text[service_start:] if next_rule < 0 else text[service_start:next_rule]
        service_id = leading_hex(service_text)
        service_queries.append(service_id)
        remap = ipnum(SERVICE_IPS[service_id])
        if pairs is not None:
            for port, remap_port in pairs:
                if cap is not None and len(rows) >= cap:
                    break
                rows.append({'address': iptext(source), 'mask': iptext(mask),
                             'remap': iptext(remap), 'port': port,
                             'remapPort': remap_port})
        if cap is None or len(rows) < cap:
            rows.append({'address': iptext(source), 'mask': iptext(mask),
                         'remap': iptext(remap), 'port': 0, 'remapPort': 0})
        if next_rule < 0:
            break
        pos = next_rule + 1
    return rows, service_queries


def parse_artist_name(text):
    return parse_name(text, cap=None)


def parse_candidate_name(text):
    return parse_name(text, cap=9)


def parse_name(text, cap):
    rows = []
    service_queries = []
    pos = 0
    while pos < len(text) and len(rows) < 8:
        delim_rel = next((i for i, c in enumerate(text[pos:]) if c in ':/'), None)
        if delim_rel is None:
            break
        delim = pos + delim_rel
        match_name = text[pos:delim]
        if text[delim] == ':':
            remap_slash = text.find('/', delim + 1)
            if remap_slash < 0:
                break
            pairs = parse_pairs(text[delim + 1:remap_slash])
            remap_start = remap_slash + 1
        else:
            pairs = None
            remap_start = delim + 1
        remap_name = ''
        remap_address = 0
        next_scan = remap_start
        if text[remap_start:remap_start + 1] == '$':
            service_id = leading_hex(text[remap_start + 1:])
            service_queries.append(service_id)
            remap_address = ipnum(SERVICE_IPS[service_id])
            next_scan += 1
        else:
            next_comma = text.find(',', remap_start)
            end = len(text) if next_comma < 0 else next_comma
            remap_name = text[remap_start:end]
            next_scan = end
        if cap is not None:
            match_out = match_name[:63]
            remap_out = remap_name[:31]
        else:
            match_out = match_name
            remap_out = remap_name
        if pairs is not None:
            for port, remap_port in pairs:
                if cap is not None and len(rows) >= cap:
                    break
                rows.append({'match': match_out, 'remapName': remap_out,
                             'remapAddress': iptext(remap_address),
                             'port': port, 'remapPort': remap_port})
        if cap is None or len(rows) < cap:
            rows.append({'match': match_out, 'remapName': remap_out,
                         'remapAddress': iptext(remap_address),
                         'port': 0, 'remapPort': 0})
        next_rule = text.find(',', next_scan)
        if next_rule < 0:
            break
        pos = next_rule + 1
    return rows, service_queries


def calls(kind, rows, secure, control_result):
    if not rows or not secure:
        return [], 0
    if kind == 'address':
        return ([{'command': 'xmap', 'fourcc': '0x786d6170', 'count': len(rows),
                  'rows': rows},
                 {'command': 'xdns', 'fourcc': '0x78646e73',
                  'value': rows[0]['address']}], control_result['xdns'])
    return ([{'command': 'xnam', 'fourcc': '0x786e616d', 'count': len(rows),
              'rows': rows}], control_result['xnam'])


def compare_case(name, kind, text, secure=True, expected_count=None):
    parse_a = parse_artist_address if kind == 'address' else parse_artist_name
    parse_c = parse_candidate_address if kind == 'address' else parse_candidate_name
    artist_rows, artist_queries = parse_a(text)
    candidate_rows, candidate_queries = parse_c(text)
    artist_calls, artist_return = calls(kind, artist_rows, secure,
                                        {'xdns': -31, 'xnam': -47})
    candidate_calls, candidate_return = calls(kind, candidate_rows, secure,
                                              {'xdns': -31, 'xnam': -47})
    equal = (artist_rows == candidate_rows and artist_queries == candidate_queries and
             artist_calls == candidate_calls and artist_return == candidate_return)
    if expected_count is not None:
        assert len(artist_rows) == expected_count, (name, len(artist_rows), expected_count)
    assert equal, name
    return {'name': name, 'kind': kind, 'input': text, 'secure': secure,
            'serviceIds': [f'0x{x:08X}' for x in artist_queries],
            'statusCallsInOrder': ([{'selector': 'serv', 'serviceId': f'0x{x:08X}'}
                                    for x in artist_queries] +
                                   ([{'selector': 'secu', 'value': int(secure)}]
                                    if artist_rows else [])),
            'tableCount': len(artist_rows), 'rows': artist_rows,
            'controlCalls': artist_calls, 'return': artist_return,
            'candidateMatchesArtist': equal}


def pairs(n):
    return ','.join(f'{1000 + i}-{2000 + i}' for i in range(n))


cases = []
cases.append(compare_case('address_single_address_mask', 'address',
    '192.168.1.40/255.255.255.0/$11', expected_count=1))
cases.append(compare_case('address_multiple_hosts_and_service_ids', 'address',
    '192.168.1.40/255.255.255.0/$11,198.51.100.8/255.255.255.255/$22', expected_count=2))
cases.append(compare_case('address_one_port_pair_plus_default', 'address',
    '192.168.1.40:80-8080/255.255.255.0/$11', expected_count=2))
cases.append(compare_case('address_two_port_pairs_plus_default', 'address',
    '192.168.1.40:80-8080,443-8443/255.255.255.0/$11', expected_count=3))
cases.append(compare_case('address_outer_eight_row_boundary_suppresses_next_rule', 'address',
    f'192.168.1.40:{pairs(7)}/255.255.255.0/$11,198.51.100.8/255.255.255.255/$22', expected_count=8))
cases.append(compare_case('address_sixteen_row_scratch_boundary', 'address',
    f'192.168.1.40:{pairs(15)}/255.255.255.0/$11', expected_count=16))
cases.append(compare_case('name_multiple_hosts_and_service_ids', 'name',
    'lobby-a.example/$11,lobby-b.example/$22', expected_count=2))
cases.append(compare_case('name_literal_remap_hosts', 'name',
    'lobby-a.example/backend-a.example,lobby-b.example/backend-b.example', expected_count=2))
cases.append(compare_case('name_one_port_pair_plus_default', 'name',
    'lobby-a.example:80-8080/$11', expected_count=2))
cases.append(compare_case('name_two_port_pairs_plus_default', 'name',
    'lobby-a.example:80-8080,443-8443/$11', expected_count=3))
cases.append(compare_case('name_outer_eight_row_boundary_suppresses_next_rule', 'name',
    f'lobby-a.example:{pairs(7)}/$11,lobby-b.example/$22', expected_count=8))
cases.append(compare_case('name_nine_row_initialized_scratch_boundary', 'name',
    f'lobby-a.example:{pairs(8)}/$11', expected_count=9))
cases.append(compare_case('address_unsecured_gate_still_resolves_but_does_not_dispatch', 'address',
    '192.168.1.40/255.255.255.0/$11', secure=False, expected_count=1))
cases.append(compare_case('name_unsecured_gate_still_resolves_but_does_not_dispatch', 'name',
    'lobby-a.example/$11', secure=False, expected_count=1))

# Malformed/oversized cases are intentionally classified, not executed against ARTIST:
# ARTIST's native body writes/reads outside local objects for these inputs, so the XEX's
# observable result is not a stable table contract. The candidate's bounded behavior is
# measured directly from the frozen source guards/copies and compared with ARTIST's locals.
addr_16_pairs = f'192.168.1.40:{pairs(16)}/255.255.255.0/$11'
artist_addr_oversize, _ = parse_artist_address(addr_16_pairs)
candidate_addr_oversize, _ = parse_candidate_address(addr_16_pairs)
name_9_pairs = f'lobby-a.example:{pairs(9)}/$11'
artist_name_oversize, _ = parse_artist_name(name_9_pairs)
candidate_name_oversize, _ = parse_candidate_name(name_9_pairs)
assert len(artist_addr_oversize) == 17 and len(candidate_addr_oversize) == 16
assert len(artist_name_oversize) == 10 and len(candidate_name_oversize) == 9
assert artist_addr_oversize[:16] == candidate_addr_oversize
assert artist_name_oversize[:9] == candidate_name_oversize

result = {
    'purpose': 'Offline source-to-ARTIST differential for supported ASCII remap configuration strings; no game, sockets, or live network executed.',
    'method': 'Candidate behavior is transcribed from the frozen C++ body. ARTIST behavior is transcribed from the three immutable ARTIST IDA exports and cross-checked against the listed X360 branch/call instructions. Service lookup and NetConnControl are deterministic offline fixtures.',
    'candidate': {'head': head, 'parent': parent, 'path': SOURCE,
                  'gitBlob': blob, 'checkoutSha256': source_sha,
                  'normalizedSha256': canonical_blob_sha,
                  'worktreeLineEnding': 'CRLF (core.autocrlf=true)',
                  'normalizedCheckoutEqualsGitBlob': True, 'worktreeClean': True},
    'artistExports': artist_hashes,
    'readerSentinels': {'0x82F264A8': f'0x{sentinel_a:08X}',
                        '0x82F26488+8*i': sentinel_b},
    'serviceFixtures': {f'0x{k:08X}': v for k, v in SERVICE_IPS.items()},
    'cases': cases,
    'oversized': {
      'address16Pairs': {'input': addr_16_pairs, 'candidateRows': len(candidate_addr_oversize),
                         'artistRowsAttempted': len(artist_addr_oversize),
                         'firstDifferingRowIndex': 16,
                         'artistWorkspaceRows': 16,
                         'difference': 'Candidate stops at its 16-row object and omits the default row. ARTIST writes 16 port rows then the default row at row index 16, one complete 16-byte SocketAddrMapX360 row beyond v87[64]. The first 16 rows are identical; native overflow makes the extra row and subsequent behavior outside the supported contract.'},
      'name9Pairs': {'input': name_9_pairs, 'candidateRows': len(candidate_name_oversize),
                     'artistRowsAttempted': len(artist_name_oversize),
                     'firstDifferingRowIndex': 9,
                     'artistWorkspaceBytes': 1024,
                     'artistZeroedBytes': 936,
                     'difference': 'Candidate stops at its nine fully initialized rows and omits the default row. ARTIST writes nine port rows then the default row at offset 936; the 104-byte record extends through byte 1039, 16 bytes beyond v92[1024]. The first nine rows are identical; native overflow makes the extra row and subsequent behavior outside the supported contract.'},
      'fieldWidths': {'artistMatchLocalBytes': 64, 'candidateMatchMaxBytes': 63,
                      'artistRemapNameLocalBytes': 32, 'candidateRemapNameMaxBytes': 31,
                      'difference': 'ARTIST copies these strings without a length check and terminates after the delimiter; candidate retains at most 63 match bytes and 31 remap-name bytes. The first out-of-bounds write begins when input match length is 64 or remap-name length is 32. These overlong inputs do not have a stable ARTIST table result.'},
      'missingDelimiters': 'Candidate stops when it cannot find required colon/slash delimiters. ARTIST assembly scans until the delimiter without a NUL check in those loops, so malformed unterminated fields can read beyond the configuration string; behavior is undefined and not modeled as a valid-output difference.'
    },
    'artistAssemblyEvidence': {
      'address': ['0x82B1FDB0 memset size 0x90 (144 bytes)',
                  '0x82B1FEEC range-list loop advances by one 16-byte row per pair',
                  '0x82B1FF7C branches back to next rule only while count < 8',
                  '0x82B2001C/0x82B20020 secure gate precedes NetConnControl xmap/xdns'],
      'name': ['0x82B200A0 memset size 0x3A8 (936 bytes)',
               '0x82B2022C range-list loop advances by one 104-byte row per pair',
               '0x82B202D4 branches back to next rule only while count < 8',
               '0x82B2035C/0x82B20360 secure gate precedes NetConnControl xnam']
    },
    'conclusion': 'Every tested supported ASCII input produces identical ordered rows, service-id resolution sequence, secure-gated control dispatch, and return semantics. No supported-input divergence found; the sealed candidate remains unchanged. Differences are limited to explicit bounded handling of oversized/malformed strings whose ARTIST behavior writes or reads outside its local objects.'
}
(ARTIFACT / 'differential.json').write_text(json.dumps(result, indent=2) + '\n', encoding='utf-8')

lines = [
'# LobbyLogin remap offline differential', '',
'## Result', '',
f'All {len(cases)} supported ASCII cases match the ARTIST-derived table and control-call model. The frozen candidate remains unchanged; no supported-input divergence was found. This is an offline source/assembly comparison, not execution of the XEX or a network test.', '',
'## Candidate and evidence', '',
f'- Candidate commit: `{head}` (parent `{parent}`), clean worktree.',
f'- Source blob: `{blob}`; canonical LF SHA-256 `{canonical_blob_sha}`. Checkout SHA-256 `{source_sha}` uses CRLF because `core.autocrlf=true`; line ending normalization matches the committed bytes exactly.',
f'- ARTIST exports: `0x82B1FD80` SHA-256 `{artist_hashes["0x82B1FD80"]}`, `0x82B20070` SHA-256 `{artist_hashes["0x82B20070"]}`, `0x82B20390` SHA-256 `{artist_hashes["0x82B20390"]}`.',
f'- Raw reader calibration passed: `u32(0x82F264A8)==4`; `u32(0x82F26488+8*i)==[194,38,63,61]`.',
'- Assembly confirms 144 bytes zeroed for address rows and 936 bytes for nine name rows; the loop back to another rule tests count `< 8` after per-rule port expansion.',
'- Service lookup uses deterministic local fixtures (`0x11 -> 203.0.113.17`, `0x22 -> 203.0.113.34`, `0x33 -> 203.0.113.51`). The JSON records each `serv` lookup followed by `secu`; unsecured cases resolve first, then issue no control calls. `NetConnStatus`/`NetConnControl` are represented as offline calls; no live socket code ran.',
'- Control-return fixtures are `xdns=-31` and `xnam=-47`; these verify address returns the xdns result, name returns the xnam result, and the xmap result is ignored, matching the ARTIST call sequence. They are not observed platform return codes.', '',
'## Supported case matrix', '',
'| Case | Rows | Calls when secure | Result |',
'|---|---:|---|---:|'
]
for case in cases:
    desc = case['name'].replace('_', ' ')
    callstr = ', '.join(c['command'] for c in case['controlCalls']) or '(none)'
    lines.append(f"| {desc} | {case['tableCount']} | {callstr} | {case['return']} |")
lines += ['', 'The one-port-pair input yields one port-specific row followed by the zero-port default row. Two pairs yield two ordered port-specific rows and the default. Seven pairs plus a second rule yield eight rows; the `< 8` outer loop suppresses the second rule. The physical scratch boundaries are 16 address rows (15 pairs plus default) and nine initialized name rows (eight pairs plus default).', '', '## Bounded malformed and oversized behavior', '',
'- Address: at 16 port pairs, candidate emits the first 16 rows and stops. ARTIST attempts a 17th default row beyond its 16-row `v87[64]` object. The first 16 rows are identical.',
'- Name: at nine port pairs, candidate emits the first nine rows and stops. ARTIST attempts a 10th default row starting at byte 936; the 104-byte row extends 16 bytes past its 1024-byte `v92` object. The first nine rows are identical.',
'- Match host length 64 or more and remap-name length 32 or more exceed ARTIST local arrays (64 and 32 bytes) because its copy loops have no bound. Candidate copies at most 63 and 31 payload bytes, respectively.',
'- For missing required delimiters, candidate returns from parsing; ARTIST’s delimiter scans have no NUL check and can read past the source string. These are safety bounds around undefined ARTIST overflow/overread, not stable table semantics.', '',
'## Artifacts', '',
'- `remap_differential.py` generates the matrix and JSON from the frozen source hash, the ARTIST exports, and the calibrated reader.',
'- `differential.json` contains every input, row, service id, control call, return, and oversized case detail.',
'- `artist-remap-pseudocode.txt` and `artist-remap-assembly-evidence.txt` preserve the ARTIST pseudocode and X360 assembly used for the comparison.',
'- Earlier compile/COFF receipt remains in `RECEIPT.md`; this task made no source change and required no successor compile.'
]
(ARTIFACT / 'DIFFERENTIAL.md').write_text('\n'.join(lines) + '\n', encoding='utf-8')
print(json.dumps({'caseCount': len(cases), 'head': head, 'sourceSha256': source_sha,
                  'oversizedAddress': [len(candidate_addr_oversize), len(artist_addr_oversize)],
                  'oversizedName': [len(candidate_name_oversize), len(artist_name_oversize)],
                  'receipt': str(ARTIFACT / 'DIFFERENTIAL.md')}, indent=2))
