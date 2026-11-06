import json,hashlib,subprocess,re,collections,pathlib
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'); H=pathlib.Path(__file__).parent; B=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/r22-batch-repaired'); R=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r21-game-cxx03-repair-20261103')
read=lambda p:json.loads(p.read_text()); sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
import sys
assert len(sys.argv)==2, 'Supply reviewed manifest SHA-256'
m=read(B/'MANIFEST.json'); assert sha(B/'MANIFEST.json')==sys.argv[1]
entries={x['path']:x for g in m['groups'] for x in g['paths']};assert len(entries)==102
old=read(R/'composite-artifact-audit.json')['source_snapshot']; report=read(R/'composite-active-native.json'); rows={r['source']:r for r in report['results']};assert len(rows)==3616
files=set(old['hashes'])|set(entries); by=collections.defaultdict(list)
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
changed={p for p in set(old['hashes'])|set(hashes) if old['hashes'].get(p)!=hashes.get(p)}; assert changed==set(entries)
rev=collections.defaultdict(set)
for graph in (old['dependencies'],deps):
 for p,children in graph.items():
  for c in children:rev[c].add(p)
affected=set(changed);queue=collections.deque(changed)
while queue:
 for p in rev[queue.popleft()]:
  if p not in affected:affected.add(p);queue.append(p)
native={p for p in files if pathlib.PurePosixPath(p).suffix.lower() in ('.cpp','.c')};assert len(native)==3642
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
named=read(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave8/repair-r18-intake-20261103/named-safeguard-results.json'))
guards=[]
for r in named['rows']:
 hit=[p for p in r['actual_included_headers'] if p.lower() in targets]; src=str(pathlib.Path(r['source']).resolve()).lower() in targets
 guards.append({'name':r['name'],'changed_source':src,'changed_actual_headers':hit})
out={'base':m['base'],'manifest_sha256':sha(B/'MANIFEST.json'),'changed_paths':sorted(changed),'native_count':len(native),'previous_native_count':len(rows),'new_native':sorted(native-set(rows)),'selected_sources':sorted(selected),'lexical_selected_count':len(affected&native),'actual_prior_consumers_count':len(actual),'actual_only':sorted(actual-(affected&native)),'prior_include_trace_origins':traces,'named_guard_impacts':guards,'projected_snapshot':{'hashes':hashes,'dependencies':deps,'nonliteral_includes':dyn,'method':'Projected using frozen new git blobs; all literal basename matches and both old/new reverse graphs.'}}
(H/'projection.json').write_text(json.dumps(out,indent=2)+'\n');print('selected',len(selected),'lexical',len(affected&native),'actual',len(actual),'actual_only',len(actual-(affected&native)),'guards',guards)
