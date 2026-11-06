import hashlib,json,pathlib,re,sys
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave18\ctor-provider-terra')
report=json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r21-game-cxx03-repair-20261103\composite-active-native.json').read_text())
sources=set(json.loads((out/'invocation.json').read_text())['sources'])
paths=set()
for row in report['results']:
 if row['source'] not in sources: continue
 for line in pathlib.Path(row['log']).read_text(encoding='utf8',errors='replace').splitlines():
  m=re.search(r'Note: including file:\s*(.*)',line,re.I)
  if m:
   p=pathlib.Path(m.group(1).strip())
   if p.is_file() and ('/vendor/' in p.as_posix().lower() or '/portable/xdk/' in p.as_posix().lower()): paths.add(p)
data={str(p):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(paths)}
(out/('external-'+sys.argv[1]+'.json')).write_text(json.dumps(data,indent=2))
print(len(data))
