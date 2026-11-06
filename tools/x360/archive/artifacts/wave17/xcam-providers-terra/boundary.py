import json,pathlib,collections,hashlib
p=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra'); ns={}; exec((p/'inventory-r19.py').read_text().split('records=[]; failures=[]')[0],ns)
inv=json.load(open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/r19-strict-game-link-diagnostic/invocation.json'))
parse,archive=ns['parse'],ns['archive']; sdk=ns['sdk']; objs=[]
for x in inv['objects']:
 s=parse(pathlib.Path(x['object']).read_bytes()); objs.append((x['source'],s))
x=[(p,s) for p,s in objs if p.lower().startswith('src/sdks/xcam/')]; outer=[(p,s) for p,s in objs if not p.lower().startswith('src/sdks/xcam/')]
sdkdefs=collections.defaultdict(list)
for member,b in archive(sdk/'xcam.lib'):
 for rec in parse(b) or []:
  if rec[1] in ('strong','comdat','common','absolute'):sdkdefs[rec[0]].append(member)
xdefs=collections.defaultdict(list)
for p,s in x:
 for n,k,*_ in s:
  if k in ('strong','comdat','common','absolute'):xdefs[n].append(p)
uses=collections.defaultdict(set)
for p,s in outer:
 for n,k,*_ in s:
  if k in ('undefined','weak') and n in xdefs:uses[n].add(p)
entries=[{'symbol':n,'current_providers':xdefs[n],'outside_consumers':sorted(v),'sdk_xcam_providers':sdkdefs.get(n,[])} for n,v in sorted(uses.items())]
out={'sdk_sha256':hashlib.sha256((sdk/'xcam.lib').read_bytes()).hexdigest(),'selected_xcam_cpp_count':len(x),'selected_xcam_cpp':[p for p,s in x],'defined_count':len(xdefs),'outside_referenced':entries,'all_xcam_definitions_missing_exact_sdk':[{'symbol':n,'providers':p} for n,p in xdefs.items() if n not in sdkdefs]}
path=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave17/xcam-providers-terra/XCAM_BOUNDARY.json');path.write_text(json.dumps(out,indent=2)+'\n')
print('selected',len(x),'defined',len(xdefs),'outside-used',len(entries),'outside missing SDK',sum(not a['sdk_xcam_providers'] for a in entries))
for e in entries: print(e['symbol'], '|',','.join(e['outside_consumers']),'|',','.join(e['sdk_xcam_providers']))
