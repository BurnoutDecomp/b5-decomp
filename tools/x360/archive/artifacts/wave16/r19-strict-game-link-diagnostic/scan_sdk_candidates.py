import json,pathlib,hashlib,collections
p=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/r19-strict-game-link-diagnostic');base=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/native-link-providers-terra');ns={};src=(base/'inventory-r19.py').read_text();exec(src.split('records=[]; failures=[]')[0],ns);parse=ns['parse'];archive=ns['archive'];sdk=ns['sdk'];actual=json.loads((p/'diagnostic-parsed.json').read_text());symbols={x['symbol'] for x in actual['unresolved']}; names=['xaudio.lib','xaudioi.lib','xcam.lib','xact.lib','xacti.lib','xact3.lib','xact3i.lib','x3daudio.lib','x3daudioi.lib','xmedia.lib','xmedia2.lib','xmcore.lib','xmcorei.lib','xonline.lib','xhv.lib','xhv2.lib'];hits=collections.defaultdict(list);libhash={}
for name in names:
 path=sdk/name;libhash[name]=hashlib.sha256(path.read_bytes()).hexdigest()
 for member,b in archive(path):
  rec=parse(b)
  if rec is not None:
   for n,k,s,c,v in rec:
    if n in symbols and k in ('strong','comdat','common','absolute'):hits[n].append({'library':name,'member':member,'kind':k,'selection':c})
  elif len(b)>=20 and b[:4]==b'\0\0\xff\xff':
   n=b[20:].split(b'\0')[0].decode('latin1')
   for n in (n,'__imp_'+n):
    if n in symbols:hits[n].append({'library':name,'member':member,'kind':'import'})
 print(name,'done',flush=True)
(p/'sdk-additional-provider-candidates.json').write_text(json.dumps({'scope':'Read-only symbol-index candidates; these libraries were NOT link inputs, and matches are not proof that library selection, extraction or runtime dependency is correct.','libraries_sha256':libhash,'symbol_candidates':hits},indent=2));print('matching distinct actual unresolved names',len(hits));print('counts',collections.Counter(x['library'] for xs in hits.values() for x in xs))
