from pathlib import Path
import re,json,hashlib
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
r=json.loads((out/'attempt03-wrapper-typed-consumers/compile-results.json').read_text(encoding='utf-8'))
old=json.loads((out/'LIFETIME_LISTING_WITNESSES.json').read_text(encoding='utf-8'))
sel={'src/SDKs/Packages/ICE/ICEWrapper_wG_09.cpp':['??0ICEWrapper@BrnDirector@@'],'src/SDKs/Packages/ICE/ICEWrapper_wG_11.cpp':['?Construct@ICEWrapper@BrnDirector@@','?Destruct@ICEWrapper@BrnDirector@@','?PlayMovie@ICEWrapper@BrnDirector@@'],'src/GameSource/Director/BrnDirectorICEWrapperPrepare.cpp':['?Prepare@ICEWrapper@BrnDirector@@']}
rows=[]
code=lambda b:[m[1] for l in b for m in [re.search(r'^\s+[0-9a-f]{5}\s+([0-9a-f]{8})\s',l)] if m]
for x in r['results']:
 if x['source'] not in sel:continue
 ls=Path(x['assembly']).read_text(encoding='utf-8').splitlines()
 for prefix in sel[x['source']]:
  ix=[i for i,l in enumerate(ls) if l.startswith(prefix) and re.search(r'\bPROC\s',l)];assert len(ix)==1,(prefix,ix)
  i=ix[0];j=next(j for j in range(i+1,len(ls)) if ls[j].startswith(prefix) and re.search(r'\bENDP\b',ls[j]));block=ls[i:j+1]
  d={'source':x['source'],'symbol_prefix':prefix,'listing':x['assembly'],'line':i+1,'block':block,'code_words':code(block)}
  match=[q for q in old if q['symbol_prefix']==prefix]
  if match:d['same_compiled_words_as_before_wrapper_fix']=code(match[0]['block'])==code(block)
  rows.append(d)
  print(prefix,d.get('same_compiled_words_as_before_wrapper_fix','new'),len(d['code_words']))
  if prefix.startswith(('?Construct','?Prepare','?PlayMovie')):
   print('\n'.join(l for l in block if re.search(r'^\s+[0-9a-f]{5}\s',l) and re.search(r'20(F0|E4|E8)|12100|20f0|20e4|20e8|9B20|20F0h|20E4h|20E8h',l)))
(out/'FINAL_WRAPPER_LISTING_WITNESSES.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
# Bind full current source context and exact newly changed header.
wr=json.loads((out/'WRAPPER_EDIT_RECEIPT.json').read_text(encoding='utf-8'))
assert hashlib.sha256(Path(wr['source']).read_bytes()).hexdigest()==wr['post_sha256']
print('Wrapper source matches frozen gate input')

