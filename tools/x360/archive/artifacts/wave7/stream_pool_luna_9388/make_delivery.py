import hashlib, json, subprocess
from pathlib import Path

work = Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\stream_pool_luna_9388')
art = Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave7\stream_pool_luna_9388')
exp = Path(r'D:\Reverse\BP-Decomp_Workflow\.ida-exports\BURNOUT_X360_ARTIST.XEX')
objdir = art/'provider-closure'/'objects'
baseout = art/'baseline'/'objects'
source_rel = 'vendor/renderware/src/rw/audio/core/StreamPool.cpp'
provider_rel = 'src/SDKs/EATech/rwcore/filesys/stream.cpp'
caller_rel = 'vendor/renderware/src/rw/audio/core/plugins/SndPlayer1.cpp'
report = json.loads((objdir/'compile-results.json').read_text(encoding='utf-8'))
base_report = json.loads((baseout/'compile-results.json').read_text(encoding='utf-8'))
rows = {row['source']: row for row in report['results']}
assert report['state']=='complete' and report['passed']==3 and report['failed']==0
assert base_report['state']=='complete' and base_report['failed']==1

def sha(p): return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git_bytes(*args): return subprocess.check_output(['git',*args],cwd=work)
def symbols(path):
    pub, ext = set(), set()
    for line in Path(path).read_text(encoding='utf-8',errors='replace').splitlines():
        fields=line.strip().split()
        if len(fields)<2: continue
        if fields[0]=='PUBLIC': pub.add(fields[1])
        elif fields[0]=='EXTRN': ext.add(fields[1].split(':')[0])
    return pub, ext
pool_row = rows[source_rel]
provider_row = rows[provider_rel]
caller_row = rows[caller_rel]
pool_pub,pool_ext=symbols(pool_row['assembly'])
provider_pub,provider_ext=symbols(provider_row['assembly'])
caller_pub,caller_ext=symbols(caller_row['assembly'])
methods=['QueueFile','GetChunk','ReleaseChunk','GetRequestState','GetState']
stream_provider_matches={}
for method in methods:
    candidates=[sym for sym in provider_pub if sym.startswith(f'?{method}@Stream@filesys@core@rw@@')]
    assert len(candidates)==1,(method,candidates)
    sym=candidates[0]
    assert sym in caller_ext,(method,sym)
    stream_provider_matches[method]=sym
kill=next(sym for sym in provider_pub if sym.startswith('?Kill@Stream@filesys@core@rw@@'))
assert kill in pool_ext
pool_caller_matches={}
for method in ['GetInstance','AcquireStream','ReleaseStream','SetStreamPriority','GetRwCoreStream']:
    candidates=[sym for sym in pool_pub if sym.startswith(f'?{method}@StreamPool@core@audio@rw@@')]
    assert len(candidates)==1,(method,candidates)
    sym=candidates[0]
    assert sym in caller_ext,(method,sym)
    pool_caller_matches[method]=sym
source=work/source_rel
text=source.read_text(encoding='utf-8')
assert 'UnreachableRwCoreStreamPath' not in text and 'std::abort' not in text and '[[noreturn]]' not in text
assert 'pRwCoreStream->Kill()' in text
base_blob=git_bytes('show',f'9388e2a58178bd4403a2b34353105b608a8feb08:{source_rel}')
base_sha=hashlib.sha256(base_blob).hexdigest()
exports={}
for addr in ['0x82B6BAB0','0x82B6BC48','0x82BC04A0','0x82BBD878','0x82BC09C8','0x82BBD9D0','0x82BBD990','0x82BBFFD8']:
    p=exp/(addr+'.json')
    d=json.loads(p.read_text(encoding='utf-8'))
    exports[addr]={'path':str(p),'sha256':sha(p),'name':d.get('name'),'prototype':d.get('prototype'),'assembly_head':d['assembly'].splitlines()[:12]}
receipt={
 'status':'private compile/provider recovery; not pushed or integrated',
 'workspace':str(work),
 'base':'9388e2a58178bd4403a2b34353105b608a8feb08',
 'head':subprocess.check_output(['git','rev-parse','HEAD'],cwd=work,text=True).strip(),
 'commit':subprocess.check_output(['git','log','-1','--format=%H%n%s%n%(trailers:key=Co-Authored-By,valueonly)'],cwd=work,text=True).strip().splitlines(),
 'changed_paths':subprocess.check_output(['git','show','--format=','--name-only','HEAD'],cwd=work,text=True).splitlines(),
 'clean':not subprocess.check_output(['git','status','--porcelain'],cwd=work,text=True).strip(),
 'source':{'path':source_rel,'sha256':sha(source),'base_blob_content_sha256':base_sha},
 'native_compile':{'report':str(objdir/'compile-results.json'),'report_sha256':sha(objdir/'compile-results.json'),'requested':report['requested'],'passed':report['passed'],'failed':report['failed'],'artifacts':[]},
 'preserved_baseline_failure':{'report':str(baseout/'compile-results.json'),'report_sha256':sha(baseout/'compile-results.json'),'requested':base_report['requested'],'passed':base_report['passed'],'failed':base_report['failed'],'log':base_report['results'][0]['log'],'log_sha256':sha(base_report['results'][0]['log'])},
 'provider_and_caller_symbol_binding':{
   'provider_source':provider_rel,'caller_source':caller_rel,
   'stream_api_provider_publics_matched_by_SndPlayer1_externs':stream_provider_matches,
   'Kill_provider_public_matched_by_StreamPool_extern':kill,
   'StreamPool_publics_matched_by_SndPlayer1_externs':pool_caller_matches,
   'proof':'The actual stream.cpp and SndPlayer1.cpp compile as native XDK objects; their decorated PUBLIC/EXTRN names match exactly. StreamPool references the same Kill provider symbol. This is symbol-level provider binding evidence, not a final executable link.'
 },
 'artist_exports':exports,
 'notes':['The C++17 noreturn helper and five abort-only definitions were removed because ARTIST exports show these are real Stream operations, and the existing stream.cpp owns their bodies.','ReleaseStream restores the original Stream::Kill call at 0x82B6BC70..74 before clearing allocated at 0x82B6BC7C.','No shared header or stream.cpp implementation was changed.'],
 'limits':['XDK compile and decorated-symbol evidence only; no final game link, hardware, or runtime claims.','The existing intentionally empty StreamPool registry remains as reconstructed; this task did not change its registration behavior.']
}
for rel,row in rows.items():
    receipt['native_compile']['artifacts'].append({'source':rel,'object':row['object'],'object_sha256':sha(row['object']),'assembly':row['assembly'],'assembly_sha256':sha(row['assembly']),'log':row['log'],'log_sha256':sha(row['log']),'machine':row['machine']})
out=art/'delivery.json'
out.write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
print('delivery',out,'sha256',sha(out),'head',receipt['head'],'clean',receipt['clean'])
print('native compile',report['passed'],'/',report['requested'],'provider API matches',len(stream_provider_matches),'StreamPool caller matches',len(pool_caller_matches))
