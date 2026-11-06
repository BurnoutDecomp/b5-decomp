import argparse, collections, datetime, hashlib, json, os, pathlib, re, subprocess
P = pathlib.Path
OUT = P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r142-link')
OLD = P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/combined-provider-plan-terra/R21_REPAIRED_COMBINED_PROVIDER.json')
BASE = P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r141-link/diagnostic-parsed.json')
SDK = P(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK')
HOST = P(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/tool-host')
def sha(p):
    h=hashlib.sha256()
    with P(p).open('rb') as f:
        for chunk in iter(lambda:f.read(1024*1024),b''): h.update(chunk)
    return h.hexdigest()
def checked(x):
    assert sha(x['path']) == x['sha256'], x['path']
def key(s): return s.replace('\\','/').lower()
def write(name, value): (OUT/name).write_text(json.dumps(value,indent=2)+'\n',encoding='utf-8')
def parse(log, selected):
    byname=collections.defaultdict(list)
    for x in selected: byname[P(x['object']).name.lower()].append(x['source'])
    rows=[]; duplicates=[]
    for line in log.splitlines():
        m=re.match(r'^(.*?)\s*:\s*error (LNK2001|LNK2019): unresolved external symbol (.*?)(?: referenced in function (.*))?$',line)
        if m:
            obj,code,raw,ref=m.groups(); s=re.search(r'\(([^()]*)\)$',raw)
            rows.append(dict(input=obj,sources=byname.get(P(obj).name.lower(),[]),code=code,symbol=s.group(1) if s else raw,display=raw,referenced_in=ref,line=line))
        elif 'error LNK2005:' in line: duplicates.append(line)
    return rows,duplicates
def groups(rows):
    d=collections.defaultdict(lambda: {'symbols':set(),'sources':set(),'messages':0})
    for r in rows:
        for source in r['sources'] or [r['input']]:
            parts=source.replace('\\','/').split('/'); g='/'.join(parts[:4]) if parts[0] in ('src','vendor') else 'archive or unknown'
            d[g]['symbols'].add(r['symbol']); d[g]['sources'].add(source); d[g]['messages']+=1
    return {g:{'symbols':sorted(v['symbols']),'sources':sorted(v['sources']),'messages':v['messages']} for g,v in sorted(d.items())}
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--sealed',required=True); ap.add_argument('--sealed-sha256',required=True); ap.add_argument('--release',required=True); args=ap.parse_args()
    assert sha(args.sealed)==args.sealed_sha256
    assert sha(BASE)=='45260f4452d3b9793033c76144cf1989b735ceaf42a31c3a6547a194c9190ef2'
    seal=json.loads(P(args.sealed).read_text()); release=json.loads(P(args.release).read_text())
    assert release=={'action':'one strict R142 native link','sealed_sha256':args.sealed_sha256}, 'Missing exact root release'
    assert seal['schema']=='r142-native-link-seal-v1'
    for x in (seal['report'],seal['boundary'],seal['source_inventory']): checked(x)
    for x in seal['sources']+seal['tool_files']+seal['libraries']+seal['provenance']: checked(x)
    report=json.loads(P(seal['report']['path']).read_text()); rows=report['results']
    assert report['passed']==len(rows)==3710 and report['failed']==0 and all(x['passed'] and x['returncode']==0 for x in rows)
    selected=seal['selected']; excluded=seal['excluded']; allrows=selected+excluded
    assert not any(key(x['source'])=='src/sdks/eatech/rwcollision/volumelinequery.cpp' for x in allrows)
    assert any(key(x['source'])=='src/vendor/renderware/collision/volumequery.cpp' for x in selected)
    assert not any(key(x['source'])=='src/gamesource/director/brndirectorresourcemanagericewrapper.cpp' for x in allrows)
    assert len(selected)==3604 and len(excluded)==106
    assert len(allrows)==3710 and len({key(x['source']) for x in allrows})==3710
    assert {key(x['source']):key(x['object']) for x in allrows}=={key(x['source']):key(x['object']) for x in rows}
    assert len({key(x['object']) for x in allrows})==3710
    for x in allrows: assert sha(x['object'])==x['sha256'], x['object']
    assert len(seal['sources'])>=3710 and len({key(x['path']) for x in seal['sources']})==len(seal['sources'])
    source_paths={key(str(P(x['path']).resolve())) for x in seal['sources']}
    source_root=P(seal['source_root'])
    for rel in ('tools/extract_x360_loading_textures.py', 'build/x360/generated/BrnLoadingScreenTextures.generated.inc', 'build/x360/generated/BrnLoadingScreenTextures.manifest.json'):
        assert key(str((source_root/rel).resolve())) in source_paths, rel
    assert all(key(str((P(seal['source_root'])/x['source']).resolve())) in source_paths for x in allrows)
    old=json.loads(OLD.read_text()); prior={key(x['source']) for x in old['excluded']}; current={key(x['source']) for x in excluded}; lua={'vendor/lua/src/linit.c','vendor/lua/src/loslib.c'}
    extra={'src/gamesource/movie/cdxvawmv.cpp','src/sdks/xam/ccriticalsection.cpp'}
    assert current in (prior|extra,prior|lua|extra), 'Unexpected exclusions'
    boundary=json.loads(P(seal['boundary']['path']).read_text())
    assert boundary['schema']=='r142-link-boundary-v1' and boundary['selected_objects_sha256']==seal['selected_objects_sha256']
    assert boundary['all_selected_objects_scanned'] and boundary['no_excluded_cohort_outside_references'] and not boundary['errors']
    if lua <= current: assert boundary['lua_linit_loslib_no_outside_references']
    selection=json.dumps(sorted((key(x['source']),key(x['object']),x['sha256']) for x in selected),separators=(',',':')).encode()
    assert hashlib.sha256(selection).hexdigest()==seal['selected_objects_sha256']
    libs=seal['libraries']; expected={P(x['path']).name.lower() for x in old['libraries']}|{'oldnames.lib','xapilib.lib','xffb.lib','xmp.lib','xhv.lib'}
    assert len(libs)==17 and {P(x['path']).name.lower() for x in libs}==expected
    assert all(P(x['path']).resolve().parent==(SDK/'lib/xbox').resolve() for x in libs)
    toolpaths={key(str(P(x['path']).resolve())) for x in seal['tool_files']}
    required={key(str(p.resolve())) for p in HOST.iterdir() if p.is_file()}|{key(str((SDK/'bin/win32/imagexex.exe').resolve()))}
    assert toolpaths==required
    OUT.mkdir(parents=True,exist_ok=True); (OUT/'tmp').mkdir(exist_ok=True)
    marker=OUT/'invocation.json'; assert not marker.exists(), 'Already invoked or invocation attempted'
    explicit=[next(x['path'] for x in libs if P(x['path']).name.lower()==P(y['path']).name.lower()) for y in old['libraries']]+[next(x['path'] for x in libs if P(x['path']).name.lower()==n) for n in ('xffb.lib','xmp.lib','xhv.lib')]
    rsp=OUT/'r142-strict.rsp'; opts=['/NOLOGO','/MACHINE:PPCBE','/SUBSYSTEM:XBOX','/ENTRY:mainCRTStartup','/VERBOSE:LIB',f'/OUT:{OUT}/native-game.exe',f'/PDB:{OUT}/native-game.pdb',f'/MAP:{OUT}/native-game.map',f'/LIBPATH:{SDK}/lib/xbox']
    rsp.write_text('\n'.join('"'+s+'"' for s in opts+[x['object'] for x in selected]+explicit)+'\n')
    env=os.environ.copy(); env['TEMP']=env['TMP']=str(OUT/'tmp'); env['PATH']=str(SDK/'bin/win32')+os.pathsep+env.get('PATH',''); env['LIB']=str(SDK/'lib/xbox')
    info={'sealed':str(P(args.sealed).resolve()),'sealed_sha256':args.sealed_sha256,'release':str(P(args.release).resolve()),'release_sha256':sha(args.release),'rsp_sha256':sha(rsp),'objects':selected,'excluded':excluded,'sources':seal['sources'],'libraries':libs,'tool_files':seal['tool_files'],'command':[str(HOST/'link.exe'),'@'+str(rsp)],'environment_overrides':{'TEMP':env['TEMP'],'TMP':env['TMP'],'PATH':env['PATH'],'LIB':env['LIB']},'started_utc':datetime.datetime.now(datetime.timezone.utc).isoformat()}
    with marker.open('x') as f: json.dump(info,f,indent=2)
    with (OUT/'link.log').open('xb') as f: proc=subprocess.run(info['command'],cwd=OUT,env=env,stdout=f,stderr=subprocess.STDOUT)
    checks={x['path']:sha(x['path'])==x['sha256'] for x in seal['sources']+seal['tool_files']+libs+seal['provenance']}
    checks.update({x['object']:sha(x['object'])==x['sha256'] for x in allrows}); checks.update({x['path']:sha(x['path'])==x['sha256'] for x in (seal['report'],seal['boundary'],seal['source_inventory'])}); checks[str(P(args.sealed).resolve())]=sha(args.sealed)==args.sealed_sha256; checks[str(rsp)]=sha(rsp)==info['rsp_sha256']
    logtext=(OUT/'link.log').read_text(errors='replace')
    searched=sorted({m.group(1).strip() for m in re.finditer(r'^\s*Searching (.*?\.lib):\s*$',logtext,re.I|re.M)})
    pinned={key(str(P(x['path']).resolve())) for x in libs}
    checks['all_searched_archives_are_pinned']=all(key(str(P(p).resolve())) in pinned for p in searched)
    write('searched-libraries.json',searched)
    write('post-link-checks.json',checks)
    unresolved,duplicates=parse(logtext,selected); previous=json.loads(BASE.read_text()); before=groups(previous['unresolved']); after=groups(unresolved)
    diff={g:{'added_symbols':sorted(set(after.get(g,{}).get('symbols',[]))-set(before.get(g,{}).get('symbols',[]))),'removed_symbols':sorted(set(before.get(g,{}).get('symbols',[]))-set(after.get(g,{}).get('symbols',[]))),'r141_messages':before.get(g,{}).get('messages',0),'r142_messages':after.get(g,{}).get('messages',0)} for g in sorted(set(before)|set(after))}
    write('diagnostic-parsed.json',{'unresolved_messages':len(unresolved),'unique_symbols':len({x['symbol'] for x in unresolved}),'unresolved':unresolved,'duplicate_messages':duplicates,'caller_groups':after,'caller_group_diff_from_r141':diff})
    write('result.json',{'exit_code':proc.returncode,'all_post_checks_pass':all(checks.values()),'log_sha256':sha(OUT/'link.log'),'outputs':{str(p):{'size':p.stat().st_size,'sha256':sha(p)} for p in OUT.glob('native-game.*')},'note':'Actual diagnostics only; no launch.'})
if __name__=='__main__': main()











