import json
from pathlib import Path
import re
import struct
import sys

sys.path.insert(0, 'D:/Reverse/BP-Decomp_Workflow/tools/re')
import findinit
import x360rd

ROOT = Path('D:/Reverse/BP-Decomp_Workflow/.ida-exports/BURNOUT_X360_ARTIST.XEX')
OUT = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/replay-module-recovery-sol')
assert x360rd.u32(0x82815570) == 0x7c8a2378 and x360rd.u32(0x82F264A8) == 4
data = [json.loads(p.read_text(encoding='utf-8')) for p in ROOT.glob('*.json')]
names = {int(d['address'], 16): d['name'] for d in data}
windows = [(0x900, 0x908), (0x90c, 0x910), (0x4930, 0x4940), (0x5b80, 0x5c30), (0x5c33, 0x5c38)]


def signed(v):
    return v - 0x10000 if v & 0x8000 else v


def add(value, offset):
    return (value[0], value[1] + offset)


results = []
for d in data:
    n = d['name']
    if not n.startswith(('BrnReplays::ReplayModule::', 'BrnReplays::DebugComponent::')):
        continue
    r = {3: ('debug', 0) if n.startswith('BrnReplays::DebugComponent::') else ('module', 0)}
    if n == 'BrnReplays::DebugComponent::Construct':
        r[4] = ('module', 0)
    accesses = []
    calls = []
    addresses = []
    in_windows = []
    for line in (d.get('assembly') or '').splitlines():
        match = re.match(r'0x([0-9A-Fa-f]+)\s+', line)
        if not match:
            continue
        site = int(match[1], 16)
        w = x360rd.u32(site)
        op, rd, ra, rb = w >> 26, (w >> 21) & 31, (w >> 16) & 31, (w >> 11) & 31
        imm = signed(w & 0xffff)
        if op in (14, 15):
            base = ('constant', 0) if ra == 0 else r.get(ra)
            if base is not None:
                r[rd] = add(base, imm << (16 if op == 15 else 0))
                if r[rd][0] == 'module':
                    row = {'site': hex(site), 'module_offset': hex(r[rd][1]), 'kind': 'derived-address', 'assembly': line}
                    addresses.append(row)
                    if any(a <= r[rd][1] < b for a, b in windows):
                        in_windows.append(row)
            else:
                r.pop(rd, None)
            continue
        if op in (24, 25):
            v = r.get(rd)
            if v and v[0] == 'constant':
                r[ra] = ('constant', v[1] | ((w & 0xffff) << (16 if op == 25 else 0)))
            else:
                r.pop(ra, None)
            continue
        if op in findinit._DFORM or op in findinit._DSFORM:
            disp = signed((w & 0xfffc) if op in findinit._DSFORM else w & 0xffff)
            base = r.get(ra)
            target = add(base, disp) if base else None
            if target and target[0] == 'module':
                row = {'site': hex(site), 'module_offset': hex(target[1]), 'kind': 'memory', 'assembly': line}
                accesses.append(row)
                if any(a <= target[1] < b for a, b in windows):
                    in_windows.append(row)
            load_module_pointer = op == 32 and target == ('debug', 0x0c)
            if op == 46:
                for rr in list(r):
                    if rr >= rd:
                        r.pop(rr, None)
            elif op in findinit._WRITES_RD or (op == 58 and (w & 3) != 1):
                r.pop(rd, None)
            if load_module_pointer:
                r[rd] = ('module', 0)
            if op in findinit._WRITES_RA or (op in findinit._DSFORM and (w & 3) == 1):
                r.pop(ra, None)
            continue
        if op == 18:
            displacement = w & 0x3fffffc
            if displacement & 0x2000000:
                displacement -= 0x4000000
            target = displacement if w & 2 else site + displacement
            callee = names.get(target, hex(target))
            args = {str(rr): {'base': r[rr][0], 'offset': hex(r[rr][1])}
                    for rr in range(3, 11) if rr in r and r[rr][0] == 'module'}
            if args:
                calls.append({'site': hex(site), 'callee': callee, 'module_arguments': args})
            if w & 1 and 'savegpr' not in callee:
                for rr in findinit._VOLATILE:
                    r.pop(rr, None)
            continue
        if op == 31:
            xo = (w >> 1) & 0x3ff
            if xo == 444 and rd == rb:
                if rd in r:
                    r[ra] = r[rd]
                else:
                    r.pop(ra, None)
            elif xo == 266:
                x, y = r.get(ra), r.get(rb)
                if x and y and x[0] == 'constant':
                    r[rd] = add(y, x[1])
                elif x and y and y[0] == 'constant':
                    r[rd] = add(x, y[1])
                else:
                    r.pop(rd, None)
            else:
                if xo in findinit._X_UPDATE:
                    r.pop(ra, None)
                if xo in findinit._X_WRITES_RA:
                    r.pop(ra, None)
                elif xo not in findinit._X_NO_GPR_DEST:
                    r.pop(rd, None)
            continue
        if op in findinit._NO_GPR_WRITE or op in (4, 5, 6):
            continue
        if op in findinit._WRITES_RA_FORM:
            r.pop(ra, None)
        else:
            r.pop(rd, None)
    results.append({'address': d['address'], 'name': n, 'module_accesses': accesses,
                    'module_derived_addresses': addresses, 'module_argument_calls': calls,
                    'window_candidates': in_windows})
result = {'method': 'Linear affine register pass over every exported ReplayModule and DebugComponent body; all branch arms scanned. A candidate pass for manual closure inspection, not a CFG proof.',
          'functions': len(results), 'window_candidates': sum(len(r['window_candidates']) for r in results),
          'rows': results}
(OUT / 'AFFINE_MODULE_VIEW_CANDIDATES.json').write_text(json.dumps(result, indent=2), encoding='utf-8')
print('Functions',len(results),'window candidates',result['window_candidates'])
for row in results:
    if row['window_candidates']:
        print(row['name'], row['window_candidates'])
print('Helper calls receiving module/interior pointers:')
for row in results:
    if row['module_argument_calls']:
        print(row['name'])
        for c in row['module_argument_calls']:
            print(c)
