from pathlib import Path
import hashlib, json, re, struct, subprocess

WAVE = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20')
OUT = WAVE / 'r154-apt-view-linked'
OLD_ROOT = WAVE / 'r153-apt-view-linked' / 'R153_APT_VIEW_LINKED_ROOT.json'
OLD_LINK = WAVE / 'r153-link'
NEW_LINK = WAVE / 'r154-link'
NEW_ROOT = OUT / 'R154_APT_VIEW_LINKED_ROOT.json'

def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()
def sha(path):
    return sha_bytes(Path(path).read_bytes())

def pe_image(path):
    data = Path(path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    sig = data[pe:pe + 4]
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', data, pe + 4)
    opt = pe + 24
    magic = struct.unpack_from('<H', data, opt)[0]
    image_base = struct.unpack_from('<I', data, opt + 28)[0]
    sections = []
    for i in range(nsec):
        q = opt + optsz + i * 40
        name = data[q:q+8].split(b'\0')[0].decode('ascii', 'replace')
        vsize, rva, rawsz, rawptr = struct.unpack_from('<IIII', data, q + 8)
        sections.append({'name': name, 'rva': rva, 'vsize': vsize, 'raw_size': rawsz, 'raw_ptr': rawptr})
    return data, {'signature': sig.decode('ascii', 'replace'), 'machine': machine, 'magic': magic,
                  'image_base': image_base, 'sections': sections}

def pe_read(path, va, length):
    data, meta = pe_image(path)
    rva = va - meta['image_base']
    for s in meta['sections']:
        if s['rva'] <= rva and rva + length <= s['rva'] + s['raw_size']:
            off = s['raw_ptr'] + rva - s['rva']
            return data[off:off + length]
    raise KeyError(f'VA not file-backed: {va:08X}+{length}')

def coff(path):
    data = Path(path).read_bytes()
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', data, 0)
    strbase = symptr + nsym * 18
    def symbol_name(raw):
        if raw[:4] == bytes(4):
            pos = strbase + struct.unpack_from('<I', raw, 4)[0]
            return data[pos:data.index(b'\0', pos)].decode('ascii', 'replace')
        return raw.rstrip(b'\0').decode('ascii', 'replace')
    syms, i = {}, 0
    while i < nsym:
        raw, value, sec, typ, storage, aux = struct.unpack_from('<8sIhHBB', data, symptr + i * 18)
        syms[i] = {'name': symbol_name(raw), 'value': value, 'section': sec, 'storage': storage}
        i += 1 + aux
    sections = {}
    for k in range(nsec):
        q = 20 + optsz + k * 40
        raw, vs, va, size, rawptr, relptr, linptr, nrel, nlin, flags = struct.unpack_from('<8sIIIIIIHHI', data, q)
        relocs = []
        for z in range(nrel):
            off, si, typ = struct.unpack_from('<IIH', data, relptr + z * 10)
            relocs.append({'offset': off, 'type': typ, 'symbol': syms[si]})
        sections[k + 1] = {'name': raw.rstrip(b'\0').decode('ascii', 'replace'), 'size': size,
                           'raw': data[rawptr:rawptr + size], 'relocs': relocs}
    return data, machine, syms, sections

def map_symbols(path):
    result = {}
    for line in Path(path).read_text(errors='replace').splitlines():
        m = re.match(r'^\s*[0-9A-Fa-f]{4}:([0-9A-Fa-f]{8})\s+(\S+)\s+([0-9A-Fa-f]{8})\s+', line)
        if m:
            result.setdefault(m.group(2), int(m.group(3), 16))
    return result

def function_symbol(object_path, name):
    data, machine, syms, sections = coff(object_path)
    hits = [s for s in syms.values() if s['name'] == name and s['section'] > 0]
    if len(hits) != 1:
        raise AssertionError(f'function not unique in object: {name}: {len(hits)}')
    sym = hits[0]
    return data, machine, sym, sections[sym['section']]

def mask_linked(path, va, size, object_path, symbol):
    data, machine, sym, sec = function_symbol(object_path, symbol)
    if machine != 0x1F2 or sym['value'] != 0 or sec['size'] < size:
        raise AssertionError(f'COFF/source layout mismatch for {symbol}')
    raw = bytearray(pe_read(path, va, size))
    entries = []
    counts = {'0x02': 0, '0x06': 0, '0x10': 0, '0x11': 0, '0x12': 0}
    for rel in sec['relocs']:
        off, typ = rel['offset'], rel['type']
        counts[f'0x{typ:02X}'] = counts.get(f'0x{typ:02X}', 0) + 1
        if typ == 0x12:  # PAIR carries no section bytes
            continue
        if off + 4 > size:
            raise AssertionError(f'relocation past function end: {symbol} {off:#x}')
        word = int.from_bytes(raw[off:off + 4], 'big')
        if typ == 0x02:  # ADDR32
            raw[off:off + 4] = bytes(4)
        elif typ == 0x06:  # REL24 displacement field
            raw[off:off + 4] = (word & ~0x03FFFFFC).to_bytes(4, 'big')
        elif typ in (0x10, 0x11):  # REFHI / REFLO low halfword
            raw[off + 2:off + 4] = bytes(2)
        else:
            raise AssertionError(f'unknown relocation {typ:#x} in {symbol}')
        entries.append({'offset': off, 'type': typ, 'symbol': rel['symbol']['name']})
    return {'raw_sha256': sha_bytes(pe_read(path, va, size)), 'masked_sha256': sha_bytes(raw),
            'nonpair_relocations': len(entries), 'pair_records': counts['0x12'],
            'relocation_counts': counts, 'entries': entries, 'section_size': sec['size']}

def resolve_targets(object_path, symbol, function_va, maps):
    _, machine, fn, sec = function_symbol(object_path, symbol)
    if machine != 0x1F2:
        raise AssertionError(f'not PPCBE COFF: {symbol}')
    counts = {'map': 0, 'owner_local': 0, 'unresolved': 0}
    targets = []
    for rel in sec['relocs']:
        if rel['type'] == 0x12:
            continue
        target = rel['symbol']
        if target['name'] in maps:
            basis, target_va = 'map', maps[target['name']]
        elif target['section'] == fn['section'] and 0 <= target['value'] < sec['size']:
            basis, target_va = 'owner_local', function_va + target['value'] - fn['value']
        else:
            basis, target_va = 'unresolved', None
        counts[basis] += 1
        targets.append({'offset': rel['offset'], 'type': rel['type'], 'symbol': target['name'],
                        'basis': basis, 'target': f'0x{target_va:08X}' if target_va is not None else None})
    digest = sha_bytes(json.dumps(targets, sort_keys=True, separators=(',', ':')).encode())
    return {'counts': counts, 'target_table_sha256': digest, 'targets': targets}

old = json.loads(OLD_ROOT.read_text())
if old.get('schema') != 'r153-apt-view-linked-carry-v1' or old.get('state') != 'pass':
    raise AssertionError('R153 proof is not a passing carry proof')
seal = json.loads((NEW_LINK / 'seal.json').read_text())
result = json.loads((NEW_LINK / 'result.json').read_text())
expected = {
    'head': '13dfc2ea9153ab2f36c06ad09e2570998e0a2d6d',
    'tree': 'ae764bcba446118b5bc8bda12ac461f5ede4a9a5',
    'seal_sha256': '024558f410971e92ae448b1482cdf1fc192e8da4a165ec14ff46fda2a797ed75',
    'xex_sha256': 'd477284ed9ff370e673fb67b0f848d9cc1d19125eb21e80cd53e6662c53885d1',
    'pe_sha256': '8895808adbacbd4c6847d3c4f5322e5ce6421250ba8f02ddbb75942208669c45',
    'map_sha256': '85ea268fb1c72e005910f97c36b9347be5c8fb545539e285c3700d471c393013'
}
if seal['head'] != expected['head'] or seal['schema'] != 'r154-native-link-seal-v1':
    raise AssertionError('R154 seal head/schema mismatch')
if subprocess.check_output(['git', 'rev-parse', expected['head'] + '^{tree}'], cwd=Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\audio_monitor_luna_3534'), text=True).strip() != expected['tree']:
    raise AssertionError('R154 commit tree mismatch')
if sha(NEW_LINK / 'seal.json') != expected['seal_sha256']:
    raise AssertionError('R154 seal hash mismatch')
for key, file in [('xex_sha256', 'native-game.exe'), ('pe_sha256', 'native-game.pe'), ('map_sha256', 'native-game.map')]:
    if sha(NEW_LINK / file) != expected[key]:
        raise AssertionError(f'R154 {file} hash mismatch')
if result.get('exit_code') != 0 or not result.get('all_post_checks_pass'):
    raise AssertionError('official R154 link result failed')

# R153 and R154 link seals must select the same exact owner objects for these bodies.
old_view = old['owners']['src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp']
old_q = old['owners']['src/GameShared/GameClasses/Module/VariableEventQueue_256_16.cpp']
obj_view = Path(old_view['selected_object_path'])
obj_queue = Path(old_q['selected_object_path'])
if sha(obj_view) != old_view['selected_object_sha256'] or sha(obj_queue) != old_q['selected_object_sha256']:
    raise AssertionError('R153 owner COFF hash changed')
new_selected = {x['source']: x for x in seal['selected']}
for source, old_owner in [('src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp', old_view),
                          ('src/GameShared/GameClasses/Module/VariableEventQueue_256_16.cpp', old_q)]:
    row = new_selected[source]
    if row['sha256'] != old_owner['selected_object_sha256'] or sha(row['object']) != row['sha256']:
        raise AssertionError(f'R154 selected object changed: {source}')

maps = map_symbols(NEW_LINK / 'native-game.map')
# All six retained roles, with both queue names resolving to the same linked COMDAT.
func_specs = {
    'ProcessIncomingLoadNotification': ('?ProcessIncomingLoadNotification@ViewModule@CgsGui@@MAAXPBUEvent@CgsModule@@@Z', obj_view),
    'gui_release': ('?Release@ViewModule@CgsGui@@UAA_NXZ', obj_view),
    'view': ('?ProcessIncomingLanguageEvent@ViewModule@CgsGui@@QAAXPBXHHPAUOutputBuffer@ViewIO@2@@Z', obj_view),
    'dispatch': ('?ProcessIncomingViewEvents@ViewModule@CgsGui@@QAAXPBV?$GuiEventQueueBase@$0BAAAA@$0BA@@2@PAUOutputBuffer@ViewIO@2@@Z', obj_view),
    'queue:CgsGuiViewModule': ('?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z', obj_view),
    'queue:VariableEventQueue_256_16': ('?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z', obj_view)
}
functions = {}
for role, (symbol, obj) in func_specs.items():
    prior = old['functions'][role]
    va = maps.get(symbol)
    if va is None:
        raise AssertionError(f'R154 map missing {symbol}')
    size = prior.get('bytes', prior.get('size'))
    r153_check = mask_linked(OLD_LINK / 'native-game.pe', int(prior.get('r153_address') or prior.get('address'), 16) if isinstance(prior.get('r153_address') or prior.get('address'), str) else (prior.get('r153_address') or prior.get('address')), size, obj, symbol)
    r154_check = mask_linked(NEW_LINK / 'native-game.pe', va, size, obj, symbol)
    want_mask = prior.get('masked_code_sha256')
    if r153_check['masked_sha256'] != want_mask or r154_check['masked_sha256'] != want_mask:
        raise AssertionError(f'relocation-masked function body mismatch: {role}')
    expected_relocs = prior['bindings']['r153_coff_non_pair_count']
    if r154_check['nonpair_relocations'] != expected_relocs:
        raise AssertionError(f'COFF relocation count mismatch: {role}')
    r154_bindings = resolve_targets(obj, symbol, va, maps)
    expected_map = prior['bindings']['map_resolved_targets']
    expected_local = prior['bindings']['owner_local_targets']
    if (r154_bindings['counts']['map'] != expected_map or
            r154_bindings['counts']['owner_local'] != expected_local or
            r154_bindings['counts']['unresolved'] != 0):
        raise AssertionError(f'R154 relocation-target resolution mismatch: {role}: {r154_bindings["counts"]}')
    functions[role] = {'symbol': symbol, 'r153_address': prior.get('r153_address', prior.get('address')),
                       'r154_address': f'0x{va:08X}', 'bytes': size,
                       'r153_raw_image_sha256': r153_check['raw_sha256'],
                       'r154_raw_image_sha256': r154_check['raw_sha256'],
                       'relocation_masked_sha256': r154_check['masked_sha256'],
                       'prior_r153_masked_sha256': want_mask,
                       'nonpair_relocation_count': r154_check['nonpair_relocations'],
                       'pair_records': r154_check['pair_records'],
                       'r154_target_bindings': {'map_resolved': r154_bindings['counts']['map'],
                           'owner_local': r154_bindings['counts']['owner_local'],
                           'unresolved': r154_bindings['counts']['unresolved'],
                           'target_table_sha256': r154_bindings['target_table_sha256']}}

# Verify the native LOADED=1 store precedes the resolved AddAptData branch in the final R154 PE.
load_symbol = func_specs['ProcessIncomingLoadNotification'][0]
load_va = maps[load_symbol]
load_size = old['functions']['ProcessIncomingLoadNotification']['bytes']
load = pe_read(NEW_LINK / 'native-game.pe', load_va, load_size)
def word(off): return int.from_bytes(load[off:off + 4], 'big')
li_word, store_word, branch_word = word(0xA0), word(0xB0), word(0xB4)
if (li_word, store_word) != (0x39400001, 0x91440014):
    raise AssertionError('R154 LOADED=1/store words mismatch')
li = branch_word & 0x03FFFFFC
if li & (1 << 25): li -= 1 << 26
branch_target = load_va + 0xB4 + li
add_apt_symbol = '?AddAptData@AptDataHandler@CgsGui@@QAAXPAUAptDataHeader@2@@Z'
add_apt_va = maps.get(add_apt_symbol)
if add_apt_va is None or branch_target != add_apt_va:
    raise AssertionError(f'R154 AddAptData branch mismatch: {branch_target:08X} vs {add_apt_va}')
loaded_store = {'function': load_symbol, 'address': f'0x{load_va:08X}', 'loaded_value': 1,
                'field_offset': 0x14, 'li_r10_1_offset': '0xA0', 'li_word': f'0x{li_word:08X}',
                'stw_r10_0x14_r4_offset': '0xB0', 'store_word': f'0x{store_word:08X}',
                'handler_call_offset': '0xB4', 'branch_word': f'0x{branch_word:08X}',
                'handler_symbol': add_apt_symbol, 'handler_address': f'0x{add_apt_va:08X}',
                'decoded_branch_target': f'0x{branch_target:08X}', 'store_before_handler_call': True}

out = {'schema': 'r154-apt-view-linked-carry-v1', 'state': 'pass',
       'head': expected['head'], 'tree': expected['tree'],
       'r153_proof': {'path': str(OLD_ROOT), 'sha256': sha(OLD_ROOT), 'head': old['head'], 'tree': old['tree']},
       'official_r154_link': {'seal_path': str(NEW_LINK / 'seal.json'), 'seal_sha256': expected['seal_sha256'],
          'result_path': str(NEW_LINK / 'result.json'), 'exit_code': result['exit_code'],
          'all_post_checks_pass': result['all_post_checks_pass'], 'xex_sha256': expected['xex_sha256'],
          'pe_sha256': expected['pe_sha256'], 'map_sha256': expected['map_sha256']},
       'owner_objects_unchanged': {'CgsGuiViewModule': old_view['selected_object_sha256'],
          'VariableEventQueue_256_16': old_q['selected_object_sha256']},
       'relocation_mask': old['relocation_mask'], 'functions': functions,
       'native_loaded_store': loaded_store,
       'carry': {'all_six_r153_entries_carried': True,
                 'queue_aliases_resolve_to_same_r154_address': functions['queue:CgsGuiViewModule']['r154_address'] == functions['queue:VariableEventQueue_256_16']['r154_address'],
                 'loaded_store_rebound_to_r154_add_apt_data': True},
       'scope': 'Static carry of R153 Apt/View linked-body evidence to exact final R154 PE. No launch.'}
NEW_ROOT.write_text(json.dumps(out, indent=2) + '\n')
print(json.dumps({'head':out['head'],'functions':{k:{'va':v['r154_address'],'mask':v['relocation_masked_sha256'],'pass':v['relocation_masked_sha256']==v['prior_r153_masked_sha256']} for k,v in functions.items()},'native_loaded_store':loaded_store,'state':out['state']},indent=2))
