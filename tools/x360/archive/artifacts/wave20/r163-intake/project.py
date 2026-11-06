import json,hashlib,subprocess,re,collections,pathlib
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); H=pathlib.Path(__file__).parent; B=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r163-batch'); R=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r162-intake')
read=lambda p:json.loads(p.read_text()); sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
import sys
assert len(sys.argv)==2, 'Supply reviewed manifest SHA-256'
m=read(B/'MANIFEST.json'); assert sha(B/'MANIFEST.json')==sys.argv[1]
entries={x['path']:x for x in m['final_paths']};assert len(entries)>0 and len(entries)==len(m['final_paths']) and all(x['new'] for x in entries.values()) and {p for p,x in entries.items() if x['old'] is None}==set()
prior_done=read(R/'completion.json'); prior_audit=read(R/'composite-artifact-audit.json'); report=read(R/'composite-active-native.json'); assert m['base']=='cfa383bfbddd126156185e122979242f5a680683' and prior_done['head']==m['base'] and prior_done['status']=='SEALED_NO_REGRESSIONS' and prior_done['passed']==prior_done['requested']==3710 and prior_done['failed']==0 and prior_audit['head']==m['base'] and sha(R/'composite-active-native.json')==prior_done['report_sha256'] and sha(R/'composite-artifact-audit.json')==prior_done['audit_sha256']; old=prior_audit['source_snapshot']; rows={r['source']:r for r in report['results']};assert len(rows)==3710 and report['passed']==3710 and report['failed']==0
text_paths={p for p in entries if p.startswith(('src/','vendor/')) and pathlib.PurePosixPath(p).suffix.lower() in ('.cpp','.c','.h','.hpp','.inl','.inc','.ipp')}; files=set(old['hashes'])|text_paths; by=collections.defaultdict(list)
for p in files:by[pathlib.PurePosixPath(p).name.lower()].append(p)
lookup={p.lower():p for p in files}; deps={};hashes={};dyn=[]
for p in sorted(files):
 if p in entries:
  entry=entries[p]; assert entry['new']; oid=entry['new'].split()[-1]; content=subprocess.check_output(['git','cat-file','blob',oid],cwd=W)
 else: content=(W/p).read_bytes()
 hashes[p]=hashlib.sha256(content).hexdigest(); edges=set()
 for d in re.findall(r'^\s*#\s*include\s*([^\r\n]+)',content.decode('utf-8-sig',errors='replace'),re.M):
  q=re.match(r'[<"]([^>"\r\n]+)[>"]',d)
  if not q:dyn.append({'path':p,'directive':d});continue
  inc=q.group(1).replace('\\','/');edges.update(by.get(pathlib.PurePosixPath(inc).name.lower(),[])); loc=(W/pathlib.PurePosixPath(p).parent/inc).resolve()
  try: rel=loc.relative_to(W).as_posix().lower()
  except ValueError: rel=''
  if rel in lookup:edges.add(lookup[rel])
 deps[p]=sorted(edges)
changed=set(entries) # checkout bytes may differ from index blob because Git converts line endings
assert all(old['hashes'].get(p)==hashes.get(p) for p in set(hashes)-changed)
rev=collections.defaultdict(set)
for graph in (old['dependencies'],deps):
 for p,children in graph.items():
  for c in children:rev[c].add(p)
affected=set(changed);queue=collections.deque(changed)
while queue:
 for p in rev[queue.popleft()]:
  if p not in affected:affected.add(p);queue.append(p)
assert not ({'src/SDKs/EATech/rwcollision/volumelinequery.cpp','src/GameSource/Director/BrnDirectorResourceManagerICEWrapper.cpp'} & set(rows))
native={p for p in files if pathlib.PurePosixPath(p).suffix.lower() in ('.cpp','.c')};assert len(native)==m.get('expected_total_native',len(native))
# Every prior row's original include trace, not merely the last wave.
targets={str((W/p).resolve()).lower() for p in changed};groups=collections.defaultdict(list)
for p,r in rows.items():groups[str(pathlib.Path(r['origin_report']).parent.parent/'actual-includes.json')].append(p)
actual=set(); traces=[]
for path,sources in groups.items():
 trace=read(pathlib.Path(path))['rows'];assert set(sources)<=set(trace)
 for p in sources:
  if any(x.lower() in targets for x in trace[p]):actual.add(p)
 traces.append({'path':path,'sha256':sha(pathlib.Path(path)),'rows':len(sources)})
selected=(affected&native)|actual|(set(entries)&native);assert selected<=native
named=read(H/'guard-baseline.json')
guards=[]
for r in named['rows']:
 hit=[p for p in r['actual_included_headers'] if p.lower() in targets]; src=str(pathlib.Path(r['source']).resolve()).lower() in targets
 guards.append({'name':r['name'],'changed_source':src,'changed_actual_headers':hit})
# Added headers may shadow already-resolved names outside the worktree. Scan every
# frozen external/SDK include file for literal edges, then walk their reverse graph.
new_headers={p for p,x in entries.items() if p in text_paths and x['old'] is None and pathlib.PurePosixPath(p).suffix.lower() not in ('.c','.cpp')}
new_names={pathlib.PurePosixPath(p).name.lower() for p in new_headers}
audit=read(R/'composite-artifact-audit.json'); before=read(R/'retention-before.json'); ext={}
for root,tree in audit['external_inputs']['include_trees'].items():
 for rel,h in tree['files'].items():ext[str((pathlib.Path(root)/rel).resolve()).lower()]=h
ext.update(before['sdk_include_hashes']); ext_rev=collections.defaultdict(set); external_direct=set()
for path,expected in ext.items():
 content=pathlib.Path(path).read_bytes();assert hashlib.sha256(content).hexdigest()==expected,path
 if b'\x00' in content:continue
 for d in re.findall(r'^\s*#\s*include\s*[<"]([^>"\r\n]+)[>"]',content.decode('utf-8-sig',errors='replace'),re.M):
  name=pathlib.PurePosixPath(d.replace('\\','/')).name.lower();ext_rev[name].add(path)
  if name in new_names:external_direct.add(path)
ext_affected=set(external_direct); q=collections.deque(external_direct)
while q:
 for parent in ext_rev[pathlib.Path(q.popleft()).name.lower()]:
  if parent not in ext_affected:ext_affected.add(parent);q.append(parent)
shadow=set(); shadow_guards={}
for path,sources in groups.items():
 trace=read(pathlib.Path(path))['rows']
 for p in sources:
  if any(x.lower() in ext_affected or pathlib.Path(x).name.lower() in new_names for x in trace[p]):shadow.add(p)
for r in named['rows']:
 hits=[p for p in r['actual_included_headers'] if p.lower() in ext_affected or pathlib.Path(p).name.lower() in new_names]
 if hits:shadow_guards[r['name']]=hits
selected|=shadow
for g in guards:
 if g['name'] in shadow_guards:g['changed_actual_headers']+=shadow_guards[g['name']]
assert selected<=native
out={'base':m['base'],'manifest_sha256':sha(B/'MANIFEST.json'),'changed_paths':sorted(changed),'native_count':len(native),'previous_native_count':len(rows),'new_native':sorted(native-set(rows)),'selected_sources':sorted(selected),'lexical_selected_count':len(affected&native),'actual_prior_consumers_count':len(actual),'actual_only':sorted(actual-(affected&native)),'prior_include_trace_origins':traces,'named_guard_impacts':guards,'added_header_shadow':{'new_headers':sorted(new_headers),'external_files_scanned':len(ext),'direct_external_edges':sorted(external_direct),'external_transitive_consumers':sorted(ext_affected),'prior_trace_consumers':sorted(shadow),'guard_trace_hits':shadow_guards},'projected_snapshot':{'hashes':hashes,'dependencies':deps,'nonliteral_includes':dyn,'method':'Projected using frozen new git blobs; all literal basename matches and both old/new reverse graphs.'}}
(H/'projection.json').write_text(json.dumps(out,indent=2)+'\n');print('selected',len(selected),'lexical',len(affected&native),'actual',len(actual),'actual_only',len(actual-(affected&native)),'guards',guards)
