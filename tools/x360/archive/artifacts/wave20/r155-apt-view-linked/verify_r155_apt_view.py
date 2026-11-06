from pathlib import Path
import hashlib, json, re, struct, subprocess

WAVE = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20')
OUT = WAVE / 'r155-apt-view-linked'
PREV_ROOT = WAVE / 'r154-apt-view-linked' / 'R154_APT_VIEW_LINKED_ROOT.json'
PREV_RECEIPT = WAVE / 'r154-apt-view-linked' / 'RECEIPT.json'
R153_ROOT = WAVE / 'r153-apt-view-linked' / 'R153_APT_VIEW_LINKED_ROOT.json'
R154_LINK = WAVE / 'r154-link'
R155_LINK = WAVE / 'r155-link'

EXPECTED = {
    'head': '1d617fd4fdbacdc73faee7dfffd7a8d43cc6b9b1',
    'tree': '6bdcd7954fa913359b809b7506984f0dc05d90ee',
    'seal_sha256': 'a969af86b6beeb39371dfbb536a4ba7eae109d541e5fef8868f96fdd5e27a28f',
    'xex_sha256': 'dc82daf3cb18c67831c2ba76b2e0fcbc589a16d8584c9418e7a6400d61558406',
    'pe_sha256': 'c172bc7b3d101a7ed092a99c7767c1f056d70664f5cf6ff1e9b875957d16d510',
    'map_sha256': '62833ae651d9f8e2839cf7924ebe7f403de3684e70d77af112aea7e3c3f03b5a'
}

def sha_bytes(data): return hashlib.sha256(data).hexdigest()
def sha(path): return sha_bytes(Path(path).read_bytes())

def pe_image(path):
    data = Path(path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3C)[0]
    machine, nsec, ts, symptr, nsym, optsz, chars = struct.unpack_from('<HHIIIHH', data, pe + 4)
    opt = pe + 24
    image_base = struct.unpack_from('<I', data, opt + 28)[0]
    sections = []
    for i in range(nsec):
        q = opt + optsz + i * 40
        name = data[q:q+8].split(b'\0')[0].decode('ascii', 'replace')
        vsize, rva, rawsz, rawptr = struct.unpack_from('<IIII', data, q + 8)
        sections.append({'name': name, 'rva': rva, 'vsize': vsize, 'raw_size': rawsz, 'raw_ptr': rawptr})
    return data, {'machine': machine, 'image_base': image_base, 'sections': sections}

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
        if m: result.setdefault(m.group(2), int(m.group(3), 16))
    return result

def function_symbol(object_path, name):
    data, machine, syms, sections = coff(object_path)
    hits = [s for s in syms.values() if s['name'] == name and s['section'] > 0]
    if len(hits) != 1: raise AssertionError(f'function not unique in object: {name}: {len(hits)}')
    sym = hits[0]
    return data, machine, sym, sections[sym['section']]

def mask_linked(path, va, size, object_path, symbol):
    _, machine, sym, sec = function_symbol(object_path, symbol)
    if machine != 0x1F2 or sym['value'] != 0 or sec['size'] < size:
        raise AssertionError(f'COFF/source layout mismatch for {symbol}')
    raw = bytearray(pe_read(path, va, size)); entries = []
    counts = {'0x02': 0, '0x06': 0, '0x10': 0, '0x11': 0, '0x12': 0}
    for rel in sec['relocs']:
        off, typ = rel['offset'], rel['type']
        counts[f'0x{typ:02X}'] = counts.get(f'0x{typ:02X}', 0) + 1
        if typ == 0x12: continue
        if off + 4 > size: raise AssertionError(f'relocation past function end: {symbol} {off:#x}')
        word = int.from_bytes(raw[off:off + 4], 'big')
        if typ == 0x02: raw[off:off + 4] = bytes(4)
        elif typ == 0x06: raw[off:off + 4] = (word & ~0x03FFFFFC).to_bytes(4, 'big')
        elif typ in (0x10, 0x11): raw[off + 2:off + 4] = bytes(2)
        else: raise AssertionError(f'unknown relocation {typ:#x} in {symbol}')
        entries.append({'offset': off, 'type': typ, 'symbol': rel['symbol']['name']})
    return {'raw_sha256': sha_bytes(pe_read(path, va, size)), 'masked_sha256': sha_bytes(raw),
            'nonpair_relocations': len(entries), 'pair_records': counts['0x12'],
            'relocation_counts': counts, 'entries': entries, 'section_size': sec['size']}

def resolve_targets(object_path, symbol, function_va, maps):
    _, machine, fn, sec = function_symbol(object_path, symbol)
    if machine != 0x1F2: raise AssertionError(f'not PPCBE COFF: {symbol}')
    counts = {'map': 0, 'owner_local': 0, 'unresolved': 0}; targets = []
    for rel in sec['relocs']:
        if rel['type'] == 0x12: continue
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
    return {'counts': counts, 'target_table_sha256': digest}

previous = json.loads(PREV_ROOT.read_text())
receipt = json.loads(PREV_RECEIPT.read_text())
if previous.get('schema') != 'r154-apt-view-linked-carry-v1' or previous.get('state') != 'pass':
    raise AssertionError('R154 prior carry proof is not passing')
if sha(PREV_ROOT) != receipt['artifacts']['R154_APT_VIEW_LINKED_ROOT.json']:
    raise AssertionError('R154 prior proof hash mismatch')
old_link = json.loads((R154_LINK / 'seal.json').read_text())
if old_link['head'] != previous['head'] or sha(R154_LINK / 'native-game.pe') != previous['official_r154_link']['pe_sha256']:
    raise AssertionError('R154 prior link does not match the saved proof')
seal = json.loads((R155_LINK / 'seal.json').read_text())
result = json.loads((R155_LINK / 'result.json').read_text())
if seal.get('schema') != 'r155-native-link-seal-v1' or seal.get('head') != EXPECTED['head']:
    raise AssertionError('R155 seal head/schema mismatch')
if subprocess.check_output(['git','rev-parse',EXPECTED['head']+'^{tree}'],cwd=Path(r'C:\Users\adri1\.codex\worktrees\bp360\wave4-intake'),text=True).strip()!=EXPECTED['tree']:
    raise AssertionError('R155 head/tree mismatch')
if sha(R155_LINK/'seal.json')!=EXPECTED['seal_sha256']:
    raise AssertionError('R155 seal hash mismatch')
for key,file in [('xex_sha256','native-game.exe'),('pe_sha256','native-game.pe'),('map_sha256','native-game.map')]:
    if sha(R155_LINK/file)!=EXPECTED[key]: raise AssertionError(f'R155 {file} hash mismatch')
if result.get('exit_code')!=0 or not result.get('all_post_checks_pass'):
    raise AssertionError('official R155 link result failed')

r153 = json.loads(R153_ROOT.read_text())
view_owner = r153['owners']['src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp']
queue_owner = r153['owners']['src/GameShared/GameClasses/Module/VariableEventQueue_256_16.cpp']
view_obj = Path(view_owner['selected_object_path']); queue_obj = Path(queue_owner['selected_object_path'])
if sha(view_obj)!=view_owner['selected_object_sha256'] or sha(queue_obj)!=queue_owner['selected_object_sha256']:
    raise AssertionError('R153 COFF owner hash mismatch')
if previous['owner_objects_unchanged'] != {'CgsGuiViewModule': view_owner['selected_object_sha256'], 'VariableEventQueue_256_16': queue_owner['selected_object_sha256']}:
    raise AssertionError('R154 prior proof owner objects differ from R153')
new_selected = {x['source']:x for x in seal['selected']}
for source, owner in [('src/GameShared/GameClasses/Gui/View/CgsGuiViewModule.cpp', view_owner),
                      ('src/GameShared/GameClasses/Module/VariableEventQueue_256_16.cpp', queue_owner)]:
    row = new_selected[source]
    if row['sha256'] != owner['selected_object_sha256'] or sha(row['object']) != row['sha256']:
        raise AssertionError(f'R155 selected owner object changed: {source}')

maps = map_symbols(R155_LINK/'native-game.map')
func_specs = {
 'ProcessIncomingLoadNotification': ('?ProcessIncomingLoadNotification@ViewModule@CgsGui@@MAAXPBUEvent@CgsModule@@@Z', view_obj),
 'gui_release': ('?Release@ViewModule@CgsGui@@UAA_NXZ', view_obj),
 'view': ('?ProcessIncomingLanguageEvent@ViewModule@CgsGui@@QAAXPBXHHPAUOutputBuffer@ViewIO@2@@Z', view_obj),
 'dispatch': ('?ProcessIncomingViewEvents@ViewModule@CgsGui@@QAAXPBV?$GuiEventQueueBase@$0BAAAA@$0BA@@2@PAUOutputBuffer@ViewIO@2@@Z', view_obj),
 'queue:CgsGuiViewModule': ('?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z', view_obj),
 'queue:VariableEventQueue_256_16': ('?AddEvent@?$VariableEventQueue@$0BAA@$0BA@@CgsModule@@QAA_NPBUEvent@2@HH@Z', view_obj)
}
functions = {}
for role,(symbol,obj) in func_specs.items():
    prior=previous['functions'][role]; va=maps.get(symbol)
    if va is None: raise AssertionError(f'R155 map missing {symbol}')
    size=prior['bytes']; old_mask=prior['relocation_masked_sha256']
    prior_map=prior['r154_target_bindings']['map_resolved'];prior_local=prior['r154_target_bindings']['owner_local']
    r154_check=mask_linked(R154_LINK/'native-game.pe',int(prior['r154_address'],16),size,obj,symbol)
    r155_check=mask_linked(R155_LINK/'native-game.pe',va,size,obj,symbol)
    if r154_check['masked_sha256']!=old_mask or r155_check['masked_sha256']!=old_mask:
        raise AssertionError(f'relocation-masked body mismatch: {role}')
    if r155_check['nonpair_relocations']!=prior['nonpair_relocation_count']:
        raise AssertionError(f'COFF relocation count mismatch: {role}')
    bindings=resolve_targets(obj,symbol,va,maps)
    if bindings['counts']!={'map':prior_map,'owner_local':prior_local,'unresolved':0}:
        raise AssertionError(f'R155 relocation target resolution mismatch: {role}: {bindings["counts"]}')
    functions[role]={'symbol':symbol,'r154_address':prior['r154_address'],'r155_address':f'0x{va:08X}',
        'bytes':size,'r154_raw_image_sha256':r154_check['raw_sha256'],'r155_raw_image_sha256':r155_check['raw_sha256'],
        'relocation_masked_sha256':r155_check['masked_sha256'],'prior_r154_masked_sha256':old_mask,
        'nonpair_relocation_count':r155_check['nonpair_relocations'],'pair_records':r155_check['pair_records'],
        'r155_target_bindings':{'map_resolved':bindings['counts']['map'],'owner_local':bindings['counts']['owner_local'],
            'unresolved':bindings['counts']['unresolved'],'target_table_sha256':bindings['target_table_sha256']}}

load_symbol=func_specs['ProcessIncomingLoadNotification'][0];load_va=maps[load_symbol];load_size=previous['functions']['ProcessIncomingLoadNotification']['bytes'];load=pe_read(R155_LINK/'native-game.pe',load_va,load_size)
def word(off):return int.from_bytes(load[off:off+4],'big')
li_word,store_word,branch_word=word(0xA0),word(0xB0),word(0xB4)
if (li_word,store_word)!=(0x39400001,0x91440014):raise AssertionError('R155 LOADED=1 store words mismatch')
li=branch_word&0x03FFFFFC
if li&(1<<25):li-=1<<26
branch_target=load_va+0xB4+li
add_symbol='?AddAptData@AptDataHandler@CgsGui@@QAAXPAUAptDataHeader@2@@Z';add_va=maps.get(add_symbol)
if add_va is None or branch_target!=add_va:raise AssertionError('R155 AddAptData branch target mismatch')
loaded={'function':load_symbol,'address':f'0x{load_va:08X}','loaded_value':1,'field_offset':0x14,
        'li_offset':'0xA0','li_word':f'0x{li_word:08X}','store_offset':'0xB0','store_word':f'0x{store_word:08X}',
        'handler_call_offset':'0xB4','branch_word':f'0x{branch_word:08X}','handler_symbol':add_symbol,
        'handler_address':f'0x{add_va:08X}','decoded_branch_target':f'0x{branch_target:08X}',
        'store_before_handler_call':True}

out={'schema':'r155-apt-view-linked-carry-v1','state':'pass','head':EXPECTED['head'],'tree':EXPECTED['tree'],
     'previous_r154_proof':{'path':str(PREV_ROOT),'sha256':sha(PREV_ROOT),'head':previous['head'],'tree':previous['tree']},
     'official_r155_link':{'seal_path':str(R155_LINK/'seal.json'),'seal_sha256':EXPECTED['seal_sha256'],
       'result_path':str(R155_LINK/'result.json'),'exit_code':result['exit_code'],'all_post_checks_pass':result['all_post_checks_pass'],
       'xex_sha256':EXPECTED['xex_sha256'],'pe_sha256':EXPECTED['pe_sha256'],'map_sha256':EXPECTED['map_sha256']},
     'owner_objects_unchanged':previous['owner_objects_unchanged'],'functions':functions,
     'native_loaded_store':loaded,
     'carry':{'all_six_r154_entries_carried':True,
       'queue_aliases_resolve_to_same_r155_body':functions['queue:CgsGuiViewModule']['r155_address']==functions['queue:VariableEventQueue_256_16']['r155_address'],
       'all_relocation_targets_resolved':all(x['r155_target_bindings']['unresolved']==0 for x in functions.values()),
       'loaded_store_rebound_before_add_apt_data':True},
     'scope':'Static R154 Apt/View carry to exact final R155 PE; no launch.'}
root_path=OUT/'R155_APT_VIEW_LINKED_ROOT.json'
root_path.write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({'head':out['head'],'functions':{k:{'address':v['r155_address'],'masked':v['relocation_masked_sha256'],'bindings':v['r155_target_bindings']} for k,v in functions.items()},'loaded_store':loaded,'state':'pass'},indent=2))
