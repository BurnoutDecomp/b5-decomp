import ast, hashlib, json, pathlib, re, struct
P=pathlib.Path
H=P(__file__).resolve().parent
MOD=H.parent.parent/'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
def sha(p):
 with P(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
nodes=[n for n in ast.parse(MOD.read_text()).body if isinstance(n,ast.FunctionDef) and n.name=='parse'];ns={'struct':struct};exec(compile(ast.Module(body=nodes,type_ignores=[]),str(MOD),'exec'),ns);parse=ns['parse']
seal=read(H/'seal.json');result=read(H/'result.json');assert result['exit_code']==0 and result['all_post_checks_pass'] and not read(H/'diagnostic-parsed.json')['unique_symbols'] and not read(H/'diagnostic-parsed.json')['duplicate_messages']
refs=[]
for row in seal['selected']:
 assert sha(row['object'])==row['sha256'];sy=parse(P(row['object']).read_bytes());assert sy is not None
 for n,k,*_ in sy:
  if 'XContentCreateEnumerator' in n and k in ('undefined','weak'):refs.append({'source':row['source'],'symbol':n,'kind':k})
map_path=H/'native-game.map'; assert sha(map_path)==result['outputs'][str(map_path)]['sha256']; lines=map_path.read_text(errors='replace').splitlines();matched=[line for line in lines if 'XContentCreateEnumerator' in line]
if refs:
 for ref in refs:
  exact=[line for line in matched if re.search(r'(?<!\S)'+re.escape(ref['symbol'])+r'(?!\S)',line)]
  assert exact and any('xapilibi:' in line.lower() for line in exact), (ref,exact)
assert all('xapilibi:' in line.lower() for line in matched),matched
report={'head':seal['head'],'seal_sha256':sha(H/'seal.json'),'result_sha256':sha(H/'result.json'),'map_sha256':sha(map_path),'selected_undefined_references':refs,'map_symbol_rows':matched,'provider':'xapilibi' if refs else None,'status':'resolved from xapilibi' if refs else 'not referenced by selected objects','scope':'static object references and linked map only'}
with (H/'xcontent-provider-verification.json').open('x',encoding='utf-8') as f:json.dump(report,f,indent=2);f.write('\n')
print(report['status'],'references',len(refs))
