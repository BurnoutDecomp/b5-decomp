import re,json,collections,pathlib
p=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/r19-strict-game-link-diagnostic'); plan=json.load(open(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra/R19_GAME_PROVIDER_PLAN.json')); byname=collections.defaultdict(list)
for x in plan['selected']:byname[pathlib.PureWindowsPath(x['object']).name.lower()].append(x['source'])
lines=(p/'link.log').read_text(errors='replace').splitlines(); rows=[];dup=[]
for line in lines:
 m=re.match(r'^(.*?)\s*:\s*error (LNK2001|LNK2019): unresolved external symbol (.*?)(?: referenced in function (.*))?$',line)
 if m:
  obj,code,raw,ref=m.groups();sym=re.search(r'\(([^()]*)\)$',raw);symbol=sym.group(1) if sym else raw; paths=byname.get(pathlib.PureWindowsPath(obj).name.lower(),[]);rows.append({'input':obj,'sources':paths,'code':code,'symbol':symbol,'display':raw,'referenced_in':ref,'line':line})
 elif 'error LNK2005:' in line:dup.append(line)
groups=collections.defaultdict(lambda:{'messages':0,'symbols':set(),'objects':set()})
for r in rows:
 for path in r['sources'] or [r['input']]:
  parts=path.replace('\\','/').split('/'); group='/'.join(parts[:4]) if parts[0] in ('src','vendor') else 'archive or unknown';g=groups[group];g['messages']+=1;g['symbols'].add(r['symbol']);g['objects'].add(path)
summary={k:{'messages':v['messages'],'unique_symbols':len(v['symbols']),'objects':len(v['objects'])} for k,v in sorted(groups.items(),key=lambda i:-len(i[1]['symbols']))}; unique=set(x['symbol'] for x in rows);cats=collections.Counter('xbdm import' if x.startswith('__imp_') else 'weak deleting-destructor E' if x.startswith('??_E') else 'other' for x in unique);result={'unresolved_messages':len(rows),'unique_symbols':len(unique),'classification_by_name':dict(cats),'groups':summary,'unresolved':rows,'duplicate_messages':dup};(p/'diagnostic-parsed.json').write_text(json.dumps(result,indent=2));print('unique',len(unique),'messages',len(rows),'class',cats);print(json.dumps(summary,indent=2)[:6500]);print('duplicate category',collections.Counter('xgraphics' if x.lower().startswith('xgraphics.lib') else 'crt' if x.lower().startswith(('libcmt.lib','libcpmt.lib')) else 'game' for x in dup))
