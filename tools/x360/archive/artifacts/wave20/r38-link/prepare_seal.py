import ast,collections,hashlib,json,pathlib,struct,subprocess,sys
P=pathlib.Path
OUT=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r38-link'); IN=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r38-intake'); ROOT=P(r'C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); OLD=P(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/combined-provider-plan-terra/R21_REPAIRED_COMBINED_PROVIDER.json'); MOD=OLD.parent/'inventory-r21-repaired.py'; SDK=P(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK'); HOST=SDK.parents[2]/'tool-host'
def sha(p):
 h=hashlib.sha256()
 with P(p).open('rb') as f:
  for c in iter(lambda:f.read(1048576),b''):h.update(c)
 return h.hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
def write(p,x):
 with P(p).open('x',encoding='utf-8') as f:json.dump(x,f,indent=2);f.write('\n')
def pathhash(p):return {'path':str(P(p).resolve()),'sha256':sha(p)}
def norm(s):return str(s).replace('\\','/').lower()
# Only load the parser functions: the original script's top-level code has side effects.
nodes=[n for n in ast.parse(MOD.read_text()).body if isinstance(n,ast.FunctionDef) and n.name in ('parse','archive')];ns={'struct':struct};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(MOD),'exec'),ns);parse,archive=ns['parse'],ns['archive']
def relocations(path,targets):
 b=P(path).read_bytes(); machine,nsec,_,so,nn,opt,_=struct.unpack_from('<HHIIIHH',b);assert machine==0x1f2
 st=b[so+nn*18:]; sy={};i=0
 while i<nn:
  raw,val,sec,typ,cl,aux=struct.unpack_from('<8sIhHBB',b,so+i*18); off=struct.unpack_from('<I',raw,4)[0] if raw[:4]==bytes(4) else 0; end=st.find(b'\0',off) if off else -1
  assert not off or (4<=off<len(st) and end>=0);name=st[off:end].decode('latin1') if off else raw.rstrip(bytes(1)).decode('latin1');sy[i]=(name,sec,cl);i+=1+aux
 for j in range(nsec):
  h=struct.unpack_from('<8sIIIIIIHHI',b,20+opt+40*j);assert not h[9]&0x01000000
  for z in range(h[7]):
   addr,idx,typ=struct.unpack_from('<IIH',b,h[5]+10*z);assert idx in sy
   name,sec,cl=sy[idx]
   if name in targets:yield {'symbol':name,'section':j+1,'offset':addr,'type':typ,'target_section':sec,'storage_class':cl}
def main():
 done=read(IN/'completion.json');report_path=IN/'composite-active-native.json';audit_path=IN/'composite-artifact-audit.json'; report=read(report_path);audit=read(audit_path)
 assert done['status']=='SEALED_NO_REGRESSIONS' and done['passed']==done['requested']==3695 and done['failed']==0
 assert sha(report_path)==done['report_sha256'] and sha(audit_path)==done['audit_sha256'] and audit['head']==done['head']
 assert audit['base']=='5ea596e049a8eaff6253c1513e54d9208ef2751e'
 netconn_path=IN/'netconn-codegen.json'; netconn=read(netconn_path)
 assert netconn['ok'] and sha(netconn_path)==done['netconn_codegen_report_sha256']
 entropy_path=IN/'entropy-frame-verification.json'; entropy=read(entropy_path)
 assert entropy['status']=='pass' and sha(entropy_path)==done['entropy_frame_report_sha256']
 loading_path=IN/'loading-coff-verification.json'; loading=read(loading_path)
 loading_input_path=IN/'loading-input.json'; loading_input=read(loading_input_path)
 loading_binding_path=IN/'loading-check-binding.json'; loading_binding=read(loading_binding_path)
 assert sha(loading_path)==done['loading_coff_report_sha256'] and sha(loading_input_path)==done['loading_input_sha256']
 assert loading_input['head']==done['head'] and loading_input['generated']==audit['generated_inputs']
 assert loading_input['input_contract_sha256']==loading_binding['contract']['sha256']
 loading_contract=read(loading_binding['contract']['path'])
 loading_ref=read(loading_binding['coff_reference']['path'])
 assert loading['machine']=='0x1f2' and loading['section']=='.data writable'
 assert len(loading['arrays'])==4 and loading['arrays']==loading_ref['arrays']
 loading_generated={
  'build/x360/generated/BrnLoadingScreenTextures.generated.inc':loading['include_sha256'],
  'build/x360/generated/BrnLoadingScreenTextures.manifest.json':loading['manifest_sha256']}
 assert {norm(P(p).resolve()):h for p,h in loading_input['generated'].items()}=={norm((ROOT/p).resolve()):h for p,h in loading_generated.items()}
 assert loading_generated=={p:r['sha256'] for p,r in loading_contract['generated_paths'].items()}
 assert all(loading[k]==loading_ref[k] for k in ('include_sha256','manifest_sha256'))
 loading_pins=[pathhash(p) for p in (loading_path,loading_input_path,loading_binding_path,IN/'verify_loading_coff.py',IN/'loading-coff-invocation.json')]
 assert loading_pins[3]['sha256']==loading_binding['adapted_verifier_sha256']
 for k in ('contract','coff_reference','stage'):
  assert sha(loading_binding[k]['path'])==loading_binding[k]['sha256']
  loading_pins.append(loading_binding[k])
 reader=loading_contract['reader']
 assert sha(reader['path'])==reader['sha256']
 loading_pins.append({'path':reader['path'],'sha256':reader['sha256']})
 deletion_path=OUT.parent/'r31-root-preflight/R30_LEGACY_LINE_QUERY_PROOF.json'; deletion=read(deletion_path)
 deletion_seal=OUT.parent/'r30-link/seal.json'
 assert sha(deletion_path)=='814b7731d11d29330c80988f38158fe2881d1c64695ab30ab5b7518daa1e30d8'
 assert sha(deletion_seal)==deletion['seal_sha256']
 retired=deletion['legacy']['source']
 canonical=deletion['canonical']['source']
 assert retired=='src/SDKs/EATech/rwcollision/volumelinequery.cpp'
 assert canonical=='src/vendor/renderware/collision/VolumeQuery.cpp'
 assert retired not in audit['artifact_manifest'] and retired not in audit['source_snapshot']['hashes'] and not (ROOT/retired).exists()
 assert deletion['objects_screened']==3695 and not deletion['legacy_outside_references']
 retired_obj=P(deletion['legacy']['object'])
 assert sha(retired_obj)==deletion['legacy']['sha256']
 retired_symbols={n for n,k,*_ in parse(retired_obj.read_bytes()) if 'VolumeLineQuery@collision@rw@@' in n and k in ('comdat','strong','undefined')}
 assert len(retired_symbols)==6
 shake_path=OUT.parent/'r34-ice-shake-root/f2eb2b794-COFF_BOUNDARY.json'; shake=read(shake_path)
 shake_seal=OUT.parent/'r33-link/seal.json'; old_seal=read(shake_seal)
 assert sha(shake_path)=='06d45c362113108380b2d77fd4e9661e448b37f8d85de457b340988e71363c17'
 assert sha(shake_seal)==shake['seal_sha256']=='793a095e626e144a77caeaa7abc86a5fce4f9bc7e52f28cb9777b5f7dd6926c5'
 assert shake['head']==old_seal['head']=='f2eb2b7941b3af91bc6b1262ee82f52ab0d8de1e' and shake['objects_screened']==3695 and shake['archives_screened']==16
 shake_source=shake['source']; shake_obj=P(shake['object']['object'])
 assert shake_source=='src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp'
 assert shake_source not in audit['artifact_manifest'] and shake_source not in audit['source_snapshot']['hashes'] and not (ROOT/shake_source).exists()
 assert shake['object'] in old_seal['selected'] and sha(shake_obj)==shake['object']['sha256'] and not shake['outside_shake_references']
 shake_symbols={row[0] for row in shake['symbols'][shake_source]}; assert len(shake_symbols)==2
 retired_symbols.update(shake_symbols)
 rows=report['results'];assert len(rows)==3695 and report['passed']==3695 and report['failed']==0 and all(r['passed'] and r['returncode']==0 for r in rows)
 assert len({norm(r['source']) for r in rows})==3695 and len({norm(r['object']) for r in rows})==3695
 old=read(OLD);ex={norm(x['source']):x for x in old['excluded']};assert len(ex)==102
 proof_path=OUT.parent.parent/'wave18/xmedia-ctor-provider/SDK_PROVIDER_PROOF.json';proof=read(proof_path);assert proof['all_body_bytes_equal'] and proof['no_relocations'] and not proof['outside_symbol_records']
 xmedia=norm(proof['source_object']['source']);assert xmedia not in ex;ex[xmedia]={'group':'xmedia exact native decoder constructor'}
 sdk_policy_path=OUT.parent.parent/'wave19/native-sdk-ffb-xmp/LINK_POLICY.json';policy=read(sdk_policy_path);assert policy['native_nonrelocated_instruction_matches']==9 and not policy['outside_symbol_records'] and not policy['source_object_static_initializers']
 xam=norm(policy['source_object']['source']);assert xam not in ex;ex[xam]={'group':'xmp matching native critical-section destructor'}
 source_hashes={}
 def add(path,expected=None):
  p=str(P(path).resolve()); h=sha(p);assert expected is None or h==expected,(p,h,expected);k=norm(p)
  if k in source_hashes:assert source_hashes[k]['sha256']==h
  source_hashes[k]={'path':p,'sha256':h}
 for rel,h in audit['source_snapshot']['hashes'].items():add(ROOT/rel,h)
 for path,h in loading_input['generated'].items():add(path,h)
 generator='tools/extract_x360_loading_textures.py'
 generator_contract=loading_contract['source_git_objects'][generator]
 git=lambda *args:subprocess.check_output(['git','--no-optional-locks',*args],cwd=ROOT)
 assert git('rev-parse','HEAD').decode().strip()==done['head']
 assert hashlib.sha256(git('show',done['head']+':'+generator)).hexdigest()==generator_contract['sha256']
 assert generator_contract['blob'] in (git('hash-object','--no-filters','--',generator).decode().strip(),git('hash-object','--path='+generator,'--',generator).decode().strip())
 add(ROOT/generator)
 for tree,entry in audit['external_inputs']['include_trees'].items():
  for rel,h in entry['files'].items():add(P(tree)/rel,h)
 objects=[]
 for r in rows:
  add(ROOT/r['source']);p=P(r['object']);h=sha(p);assert h==audit['artifact_manifest'][r['source']]['object_sha256'];sy=parse(p.read_bytes());assert sy is not None, p
  assert r['source'] not in (retired,shake_source) and all(n not in retired_symbols for n, *_ in sy),r['source']
  objects.append({'source':r['source'],'object':str(p.resolve()),'sha256':h,'symbols':sy,'group':ex.get(norm(r['source']),{}).get('group')})
 assert sum(x['group'] is not None for x in objects)==104
 x=next(x for x in objects if norm(x['source'])==xam);assert x['sha256']==policy['source_object']['sha256'];assert {n for n,k,*_ in x['symbols'] if k in ('strong','comdat','common','absolute','weak')}==set(policy['excluded_provider_own_symbols'])
 x=next(x for x in objects if norm(x['source'])==xmedia);assert x['sha256']==proof['source_object']['sha256'];assert {n for n,k,*_ in x['symbols'] if k in ('strong','comdat','common','absolute')}=={proof['only_external_definition']}
 assert sha(proof['sdk_library']['path'])==proof['sdk_library']['sha256']
 libs=[x['path'] for x in old['libraries']]+[str(SDK/'lib/xbox/oldnames.lib'),str(SDK/'lib/xbox/xapilib.lib')]+[x['path'] for x in policy['libraries']]; libhash=[pathhash(x) for x in libs];assert len({norm(x['path']) for x in libhash})==16
 for x in policy['libraries']:assert sha(x['path'])==x['sha256']
 for x in old['libraries']:assert sha(x['path'])==x['sha256'],x['path']
 defaults=read(OLD.parent.parent/'r21-repaired-full-native-link-diagnostic/default-libraries-post-link.json')['post_link_sdk_archive_sha256']
 for path,h in defaults.items():assert sha(path)==h,path
 sdkdefs=collections.defaultdict(list);parse_fail=[];archive_members=collections.Counter()
 for lib in libs:
  for member,data in archive(P(lib)):
   sy=parse(data)
   if sy is None and len(data)>=20 and data[:4]==b'\0\0\xff\xff':
    name=data[20:].split(b'\0')[0].decode('latin1');sy=[(name,'import',0,None,0),('__imp_'+name,'import',0,None,0)]
   if sy is None:
    parse_fail.append({'library':lib,'member':member,'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)});continue
   archive_members[P(lib).name]+=1
   for n,k,sec,selection,val in sy:
    if k in ('strong','comdat','common','absolute','import'):sdkdefs[n].append({'library':lib,'member':member,'kind':k,'selection':selection})
 # Unparsed archive members are not assumed to be providers and are recorded for review.
 bysource={norm(x['source']):x for x in objects}; lua={'vendor/lua/src/linit.c','vendor/lua/src/loslib.c'};assert lua<=bysource.keys()
 all_defs=collections.defaultdict(list)
 for x in objects:
  for n,k,sec,sel,val in x['symbols']:
   if k in ('strong','comdat','common','absolute'):all_defs[n].append({'source':x['source'],'kind':k,'selection':sel})
 lua_defs={n for n,defs in all_defs.items() if any(norm(x['source']) in lua for x in defs)}
 lua_refs=[]
 for x in objects:
  if norm(x['source']) in lua or x['group'] is not None:continue
  for n,k,*_ in x['symbols']:
   if n in lua_defs and k in ('undefined','weak'):lua_refs.append({'source':x['source'],'symbol':n,'kind':k})
  for hit in relocations(x['object'],lua_defs):
   if hit['target_section']==0:lua_refs.append({'source':x['source'],'kind':'relocation',**hit})
 # Absence of retained object references to any exported linit/loslib symbol includes luaopen_os and luaL_openlibs.
 lua_exclude=not lua_refs and {'luaopen_os','luaL_openlibs'}<=lua_defs
 excluded=[x for x in objects if x['group'] is not None or (lua_exclude and norm(x['source']) in lua)];selected=[x for x in objects if x not in excluded]; seldefs=collections.defaultdict(list);weak=collections.defaultdict(list);cohort=collections.defaultdict(list)
 for x in selected:
  for n,k,sec,sel,val in x['symbols']:
   if k in ('strong','comdat','common','absolute'):seldefs[n].append({'source':x['source'],'kind':k,'selection':sel})
   elif k=='weak':weak[n].append({'source':x['source'],'fallback':val,'search':sel})
 for x in excluded:
  for n,k,sec,sel,val in x['symbols']:
   if k in ('strong','comdat','common','absolute'):cohort[n].append({'source':x['source'],'group':x['group'] or 'lua','kind':k,'selection':sel})
 refs=collections.defaultdict(lambda:{'undefined':[],'relocations':[],'weak':[]})
 for x in selected:
  for n,k,*tail in x['symbols']:
   if n in cohort and k in ('undefined','weak'):refs[n][k].append({'source':x['source'],'details':tail})
  for hit in relocations(x['object'],cohort):refs[hit['symbol']]['relocations'].append({'source':x['source'],**hit})
 detail={};blockers={}
 for n,ref in refs.items():
  relevant=ref['undefined'] or ref['weak'] or [r for r in ref['relocations'] if r['target_section']==0]
  row={'excluded_definitions':cohort[n],**ref,'retained_definitions':seldefs[n],'sdk_definitions':sdkdefs[n],'weak_aliases':weak[n]};detail[n]=row
  # Weak aliases count only when their fallback is a real retained/native SDK symbol.
  alternate=seldefs[n] or sdkdefs[n] or any(w['fallback'] in seldefs or w['fallback'] in sdkdefs for w in weak[n])
  if relevant and not alternate:blockers[n]=row
 selected_hashes={norm(row['source']):row['sha256'] for row in selected}
 assert selected_hashes[norm(canonical)]==deletion['canonical']['sha256']
 assert selected_hashes['src/gamesource/game/brnloadingscreentextures.cpp']==loading['obj_sha256']
 assert selected_hashes['vendor/dirtysdk/src/x360/protossl_random_x360.cpp']==entropy['object_sha256']
 assert selected_hashes['vendor/dirtysdk/src/x360/netconn_friends_x360.cpp']==netconn['friends_sha256']
 assert selected_hashes['vendor/dirtysdk/src/x360/netconn_owner_x360.cpp']==netconn['owner_sha256']
 assert next(row['sha256'] for row in libhash if P(row['path']).name.lower()=='libcmt.lib')==netconn['crt_sha256']
 compact=lambda x:{k:x[k] for k in ('source','object','sha256','group')}
 sel=[compact(x) for x in selected];exc=[compact(x) for x in excluded]; digest=hashlib.sha256(json.dumps(sorted((norm(x['source']),norm(x['object']),x['sha256']) for x in sel),separators=(',',':')).encode()).hexdigest()
 boundary={'schema':'r38-link-boundary-v1','head':done['head'],'selected_objects_sha256':digest,'all_selected_objects_scanned':True,'no_excluded_cohort_outside_references':not blockers,'lua_linit_loslib_no_outside_references':lua_exclude,'lua_references':lua_refs,'lua_definitions':sorted(lua_defs),'errors':list(blockers),'blockers':blockers,'references':detail,'archive_members_parsed':archive_members,'archive_unparsed_members':parse_fail,'parser':pathhash(MOD),'scan_script':pathhash(__file__),'note':'Outside references are accepted only when a retained object, SDK archive, or weak alias with a real retained/SDK fallback provides the symbol; archive candidates are not proof of extraction.'}
 write(OUT/'boundary.json',boundary);print('lua excluded',lua_exclude,'outside blockers',len(blockers),'unparsed archive members',len(parse_fail),flush=True)
 if blockers:raise RuntimeError('Boundary unresolved; inspect boundary.json, no seal created')
 inventory={'schema':'r38-source-inventory-v1','head':done['head'],'files':list(source_hashes.values())};write(OUT/'source-inventory.json',inventory)
 toolfiles=[pathhash(p) for p in sorted(HOST.iterdir()) if p.is_file()]+[pathhash(SDK/'bin/win32/imagexex.exe')]
 provenance=[pathhash(p) for p in (IN/'completion.json',audit_path,IN/'command.json',IN/'actual-includes.json',IN/'commit-binding.json',IN/'ROOT_RELEASE.json',netconn_path,entropy_path,OLD,MOD,P(__file__),proof_path,sdk_policy_path)]
 for p,h in audit['external_inputs']['compiler_and_runtime'].items():
  assert sha(p)==h;provenance.append({'path':p,'sha256':h})
 origin={}
 for r in rows:
  for k,v in r.items():
   if k.endswith('_sha256') and isinstance(v,str) and k[:-7] in r and isinstance(r[k[:-7]],str) and P(r[k[:-7]]).is_file():
    path=r[k[:-7]]; key=norm(path)
    if key not in origin:assert sha(path)==v,(k,path);origin[key]={'path':path,'sha256':v}
    else:assert origin[key]['sha256']==v,(k,path)
 provenance.extend(origin.values())
 provenance.extend(loading_pins)
 provenance.extend(pathhash(p) for p in (deletion_path,deletion_seal,retired_obj,shake_path,shake_seal,shake_obj))
 provenance.append(pathhash(OUT/'run_once.py'))
 provenance.append(pathhash(OUT.parent/'r36-link/diagnostic-parsed.json'))
 seal={'schema':'r38-native-link-seal-v1','head':done['head'],'report':pathhash(report_path),'boundary':pathhash(OUT/'boundary.json'),'source_inventory':pathhash(OUT/'source-inventory.json'),'source_root':str(ROOT),'sources':list(source_hashes.values()),'tool_files':toolfiles,'libraries':libhash,'selected':sel,'excluded':exc,'selected_objects_sha256':digest,'provenance':provenance}
 write(OUT/'seal.json',seal);print('SEAL',sha(OUT/'seal.json'),'selected',len(sel),'excluded',len(exc),'sources',len(source_hashes),flush=True)
if __name__=='__main__':main()





