import json,collections
D=json.load(open(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave16\native-link-providers-terra\coff-records-r19.json'))['records']
o=next(x for x in D if x.get('source','').endswith('/exception.cpp'))
keys={s[0] for s in o['symbols'] if s[1] in ('strong','comdat','common','absolute','weak')}
for n in sorted(keys):
 providers=[(x.get('source') or x.get('path'),x.get('member'),s[1:]) for x in D for s in x['symbols'] if s[0]==n and s[1] in ('strong','comdat','common','absolute','weak') and x is not o]
 users=[(x.get('source') or x.get('path'),x.get('member')) for x in D for s in x['symbols'] if s[0]==n and s[1]=='undefined' and x is not o]
 print(n, '\n  PROVIDERS',providers[:12], 'TOTAL',len(providers),'\n  UNDEFINED',len(users),users[:8])
