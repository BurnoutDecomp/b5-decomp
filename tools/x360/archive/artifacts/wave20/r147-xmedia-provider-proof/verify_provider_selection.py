
import re, struct, sys
from pathlib import Path
from collections import defaultdict
sdk=Path('C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK/lib/xbox')
def archive(name):
    data=(sdk/(name+'.lib')).read_bytes()
    count,=struct.unpack_from('>I',data,68)
    offsets=struct.unpack_from(f'>{count}I',data,72)
    names=data[72+count*4:].split(b'\0',count)[:count]
    symidx=dict(zip((n.decode(errors='replace') for n in names),offsets))
    pos=8; longnames=b''
    while pos<len(data):
        label=data[pos:pos+16].strip()
        length=int(data[pos+48:pos+58])
        if label==b'//':
            longnames=data[pos+60:pos+60+length]
            break
        if label!=b'/':
            break
        pos+=60+length+(length%2)
    def member(off):
        name=data[off:off+16].strip()
        if name.startswith(b'/') and name[1:].isdigit():
            start=int(name[1:])
            name=longnames[start:].split(b'\0',1)[0].split(b'/\n',1)[0]
        length=int(data[off+48:off+58])
        return name.decode(errors='replace').rstrip('/'),data[off+60:off+60+length]
    return symidx,member
def coff(data):
    machine,sections,_,off,count,opt,_=struct.unpack_from('<HHIIIHH',data)
    if machine != 0x1f2 or not count:
        return [],[]
    start=off+18*count
    defs=[]; un=[]
    i=0
    while i<count:
        p=off+18*i
        a,b,val,sect,typ,store,aux=struct.unpack_from('<IIIhHBB',data,p)
        if store in (2,105):
            name=(data[start+b:data.index(0,start+b)] if not a else data[p:p+8].rstrip(b'\0')).decode(errors='replace')
            if sect >0 or sect==0 and val:
                defs.append(name)
            elif sect==0 and store==2:
                un.append(name)
        i+=1+aux
    return defs,un

from collections import deque
def edges(start,names,known=()):
    loaded=set()
    q=deque((s,None) for s in start)
    paths={}
    selected=[]
    indexed={name:archive(name) for name in names}
    known=set(known)
    while q:
        s,parent=q.popleft()
        if s in known: continue
        for lib,(idx,get) in indexed.items():
            if s not in idx: continue
            off=idx[s]
            if (lib,off) in loaded: break
            name,data=get(off)
            name=Path(name.replace('\\','/')).name
            defs,un=coff(data)
            known.update(defs)
            loaded.add((lib,off))
            paths[(lib,name)] = (s,parent,off)
            selected.append((lib,name,off))
            q.extend((d,(lib,name)) for d in un if d not in known)
            break
    return paths,selected
def strong_defs(data):
    machine,nsections,_,off,count,opt,_=struct.unpack_from('<HHIIIHH',data)
    if machine!=0x1f2 or not count:
        return set()
    start=off+18*count
    strong=set()
    i=0
    while i<count:
        p=off+18*i
        a,b,val,sect,typ,store,aux=struct.unpack_from('<IIIhHBB',data,p)
        if store==2 and sect>0:
            flags,=struct.unpack_from('<I',data,20+opt+40*(sect-1)+36)
            if not flags&0x1000:
                name=(data[start+b:data.index(0,start+b)] if not a else data[p:p+8].rstrip(b'\0')).decode(errors='replace')
                strong.add(name)
        i+=1+aux
    return strong

"""Verify the exact R147 xcam/xmedia provider selection without invoking a linker."""
import hashlib
import json

def sha_bytes(data):
    return hashlib.sha256(data).hexdigest()

def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()

here=Path(__file__).resolve().parent
failed=here.parent/'r147-link'
expected_head='5509461903bd56c25d92208d84da7a37171a4fc4'
pins={
    'seal.json': 'db48703b7b1fae57ff3d4addb72bff3e97eeca5bb0da081a3b2dd402c332b801',
    'link.log': '736534d09300aba55acd0f865cb8e21ddc3d5ae240da679b099f140a69d4a008',
    'result.json': '55f52a47227c074292c4bf6f02be6ee6b773fb123c5769d9da7e141f187ec06c',
    'invocation.json': '6b0e1b3a0b2863b05837c6070ccc662910b51def5b52bf910f52f68bc1275c0b',
    'r147-strict.rsp': 'c438acdffa651e4f57376d7d0f1ef6fd90413dec2ea968eda9d54ee1d972b6a5',
    'source-inventory.json': 'eb02b8f24b273c4c660d770a741709b1dd4a95e2ed678c4c708c75a85c7eccf6',
    'searched-libraries.json': '92b42d87955d883669d7d66e4eaf1689641ac7a5e6c896aa11c6da6f50c762a9',
}
for path, digest in pins.items():
    assert sha(failed/path)==digest, path

def read(name):
    return json.loads((failed/name).read_text())

seal=read('seal.json')
result=read('result.json')
inv=read('invocation.json')
assert seal['head']==read('source-inventory.json')['head']==expected_head
assert result['exit_code']==1169 and result['log_sha256']==pins['link.log']
assert inv['sealed_sha256']==pins['seal.json'] and inv['rsp_sha256']==pins['r147-strict.rsp']
assert inv['objects']==seal['selected'] and inv['excluded']==seal['excluded']
assert len(seal['selected'])==3604 and len(seal['excluded'])==106
rsp=(failed/'r147-strict.rsp').read_text().splitlines()
assert [line.strip('"') for line in rsp if line.lower().endswith('.obj"')]==[row['object'] for row in seal['selected']]
assert not any('/FORCE' in line.upper() for line in rsp)

log=(failed/'link.log').read_text()
duplicates=defaultdict(set)
for member,obj in re.findall(r'^xmedia\.lib\(([^)]+)\) : error LNK2005: .* already defined in (\S+\.obj)',log,re.M):
    duplicates[obj].add(member)
assert len(duplicates)==14 and log.count('error LNK2005:')==24

sdk_libraries={Path(row['path']).stem: row for row in seal['libraries']}
searched_paths={Path(path).stem: Path(path) for path in read('searched-libraries.json')}
library_indices={}
for name,path in searched_paths.items():
    if name in sdk_libraries:
        assert path==Path(sdk_libraries[name]['path'])
        assert sha(path)==sdk_libraries[name]['sha256']
    library_indices[name]=archive(name)

rows={}
all_defs=defaultdict(set)
all_undef=defaultdict(set)
for row in seal['selected']:
    path=Path(row['object'])
    assert path.name not in rows
    data=path.read_bytes()
    assert sha_bytes(data)==row['sha256']
    ds,un=coff(data)
    rows[path.name]=(row,set(ds),set(un))
    for name in ds:
        all_defs[name].add(path.name)
    for name in un:
        all_undef[name].add(path.name)

media_index=library_indices['xmedia'][0]
logged=set(duplicates)
logged_defs=set().union(*(rows[name][1] for name in logged))
outside_logged={}
for symbol in sorted(logged_defs):
    clients=all_undef[symbol]-logged
    if clients and not (all_defs[symbol]-logged) and symbol not in media_index:
        outside_logged[symbol]=sorted(clients)
assert outside_logged=={
    '??0CLocalHuffman@@QAA@IPAI@Z': ['CLocalHuffmanEncoder-ed59a6ad.obj'],
    '?g_iAltTablesShortCodeFlag@@3HA': ['CLocalHuffmanEncoder-ed59a6ad.obj'],
    '?generateHuffman_balanced@@YAPAIPAIHI0H@Z': ['CLocalHuffmanEncoder-ed59a6ad.obj'],
}
companion='CLocalHuffmanEncoder-ed59a6ad.obj'
excluded=logged|{companion}
sources=[{key: row[key] for key in ('source','object','sha256')}
         for row in seal['selected'] if Path(row['object']).name in excluded]
assert len(sources)==15
remaining=set(rows)-excluded
remain_defs=set().union(*(rows[name][1] for name in remaining))
remain_refs=set().union(*(rows[name][2] for name in remaining))
excluded_defs=set().union(*(rows[name][1] for name in excluded))
losses=(remain_refs&excluded_defs)-remain_defs
assert not losses

roots=['XCamGetStatus','XCamCreateStreamEngine']
archive_paths,sequence=edges(roots,['xcam','xmedia'],remain_defs)
baseline_paths,_=edges(['XCamGetStatus'],['xcam','xmedia'],remain_defs)
assert len(sequence)==179 and len(baseline_paths)==2
assert not any(lib=='xmedia' for lib,_ in baseline_paths)
archive_defs=set()
archive_undef=set()
archive_strong=defaultdict(list)
members=[]
for lib,member,offset in sequence:
    data=library_indices[lib][1](offset)[1]
    ds,un=coff(data)
    archive_defs.update(ds)
    archive_undef.update(un)
    for name in strong_defs(data):
        archive_strong[name].append(lib+':'+member)
    symbol,parent,recorded_offset=archive_paths[(lib,member)]
    assert recorded_offset==offset
    members.append({
        'archive': lib+'.lib', 'member': member, 'header_offset': hex(offset),
        'machine': '0x1f2', 'sha256': sha_bytes(data), 'selected_by': symbol,
        'from_member': ':'.join(parent) if parent else None,
    })
assert len({(row['archive'],row['member']) for row in members})==len(members)
direct={symbol: sorted(all_undef[symbol]&remaining)
        for symbol in sorted((remain_refs&archive_defs)-remain_defs)}
assert direct=={symbol: ['BrnNetworkCameraX360-7ba1c7c3.obj'] for symbol in roots}
assert not ((archive_undef&remain_defs)-archive_defs)

outside_archive=sorted(archive_undef-archive_defs-remain_defs)
outside_providers={symbol: sorted(name+'.lib' for name,(index,_) in library_indices.items() if symbol in index)
                   for symbol in outside_archive}
missing=sorted(symbol for symbol,providers in outside_providers.items() if not providers)
assert not missing
strong_overlaps={}
for name in sorted(remaining):
    row,defs,_=rows[name]
    if defs & archive_strong.keys():
        overlap=sorted(strong_defs(Path(row['object']).read_bytes()) & archive_strong.keys())
        if overlap:
            strong_overlaps[name]=overlap
assert not strong_overlaps
assert len(remaining)==3589 and len(seal['excluded'])+len(excluded)==121

receipt={
    'schema': 'r147-xmedia-provider-selection-v1',
    'state': 'pass',
    'head': expected_head,
    'original_seal_sha256': pins['seal.json'],
    'original_log_sha256': pins['link.log'],
    'original_result_sha256': pins['result.json'],
    'original_failed_link': {
        'folder': str(failed), 'exit_code': result['exit_code'], 'hashes': pins,
        'selected': len(seal['selected']), 'excluded': len(seal['excluded']),
        'LNK2005_messages': 24, 'LNK2005_objects': 14,
    },
    'selected': len(remaining),
    'excluded': len(seal['excluded'])+len(excluded),
    'sources': sources,
    'logged_collision_members_by_object': {key: sorted(value) for key,value in sorted(duplicates.items())},
    'private_consumer': {
        'object': companion, 'external_symbols_lost_by_removing_only_reported_objects': outside_logged,
        'reason': 'The only retained explicit consumer of three private symbols absent from xmedia.lib. No surviving explicit object requires a symbol defined solely in the fifteen exclusions.',
    },
    'archives': {name+'.lib': sdk_libraries[name] for name in ('xcam','xmedia')},
    'archive_members': members,
    'archive_member_counts': {
        'xcam.lib': sum(row['archive']=='xcam.lib' for row in members),
        'xmedia.lib': sum(row['archive']=='xmedia.lib' for row in members),
        'xcam_before_stream_engine': sorted(':'.join(key) for key in baseline_paths),
    },
    'surviving_direct_archive_refs': direct,
    'surviving_direct_refs_to_excluded_providers': sorted(losses),
    'archive_outside_providers_by_symbol': outside_providers,
    'missing_count': len(missing)+len(losses),
    'strong_conflict_count': len(strong_overlaps),
    'scope': 'Static PPC COFF closure from the sealed native object set. Archive indexing is a preflight check. This receipt records no new linker or runtime result.',
}
output=json.dumps(receipt,indent=2)+'\n'
if '--write' in sys.argv:
    path=here/'PROVIDER_SELECTION.json'
    with path.open('x',encoding='utf-8') as stream:
        stream.write(output)
    print(path,sha(path))
else:
    print(output,end='')

