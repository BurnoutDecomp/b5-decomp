import ast,collections,hashlib,json,pathlib,struct
A=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts')
D=A/'wave20/r26-root-providers'
P=A/'wave17/combined-provider-plan-terra/inventory-r21-repaired.py'
S=A/'wave20/r24-link/seal.json'
L=A/'wave20/r24-link/diagnostic-parsed.json'
def read(p):return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
nodes=[n for n in ast.parse(P.read_text()).body if isinstance(n,ast.FunctionDef) and n.name in ('parse','archive')]
ns={'struct':struct}
exec(compile(ast.Module(body=nodes,type_ignores=[]),str(P),'exec'),ns)
parse,archive=ns['parse'],ns['archive']
seal=read(S)
diag=read(L)
receipt=read(D/'DELIVERY.json')
report=read(D/'native/compile-results.json')
sources={r['source'] for r in report['results']}
before={r['source']:r for r in seal['selected'] if r['source'] in sources}
assert set(before)==sources
kinds={'strong','comdat','common','absolute'}
old_refs=set()
new_defs=set()
new_refs=set()
for row in before.values():
    obj=pathlib.Path(row['object'])
    assert sha(obj)==row['sha256']
    old_refs.update(n for n,kind,*_ in parse(obj.read_bytes()) if kind in ('undefined','weak'))
for row in report['results']:
    p=pathlib.Path(row['object'])
    assert sha(p)==receipt['verified_artifact_sha256'][row['source']]['object']
    for n,kind,*_ in parse(p.read_bytes()):
        if kind in kinds:new_defs.add(n)
        if kind in ('undefined','weak'):new_refs.add(n)
relevant={x['symbol'] for x in diag['unresolved']
          if any('Huffman' in p or 'CAltTablesEncoder' in p for p in x['sources'])
          or x['symbol'].startswith('?Release@BrnCoronaManager@@')}
proven={}
for n in sorted(relevant):
    if n in new_defs:proven[n]='defined by candidate cohort'
    elif n not in new_refs:proven[n]='old reference eliminated by recovered source'
    else:proven[n]='still externally referenced'
assert len(proven)==13 and all(v!='still externally referenced' for v in proven.values())
want=new_refs-new_defs-old_refs
providers=collections.defaultdict(list)
for row in seal['selected']:
    if row['source'] in sources:continue
    p=pathlib.Path(row['object'])
    for n,k,*_ in parse(p.read_bytes()):
        if n in want and k in kinds:
            assert sha(p)==row['sha256']
            providers[n].append({'type':'retained-coff','source':row['source']})
for row in seal['libraries']:
    p=pathlib.Path(row['path'])
    assert sha(p)==row['sha256']
    for member,data in archive(p):
        sy=parse(data)
        if sy is None:continue
        for n,k,*_ in sy:
            if n in want and k in kinds:providers[n].append({'type':'SDK archive','archive':p.name,'member':member})
missing=sorted(want-providers.keys())
assert not missing,missing
out={'head':receipt['head'],'r24_seal_sha256':sha(S),'r24_diagnostics_sha256':sha(L),
     'candidate_receipt_sha256':sha(D/'DELIVERY.json'),'source_units':sorted(sources),
     'native_coff_witnesses':proven,'new_external_ref_providers':providers,
     'scope':'Static provider inventory only. No whole-game link, extraction, or runtime claim.'}
(D/'coff-link-input-comparison.json').write_text(json.dumps(out,indent=2)+'\n')
print('targeted old names',len(proven),'new names',len(want),'without providers',len(missing))
for n in sorted(want):print(n,len(providers[n]))
