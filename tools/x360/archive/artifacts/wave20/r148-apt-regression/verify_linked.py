"""Rebind the sealed R147 Apt proof to a sealed R148 PPC image."""
import argparse, hashlib, json, runpy, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
BASE_RECEIPT_SHA = 'c9ff81208cc9c09a8e214101d4c6655b153b01d699db590637d59dc08397960a'
MANIFEST_SHA = '46d182100c312d64f1b6cb3f900122dd2a7df95ecdaa41093538074fa0795339'
TREE = '292de86d70d0b034d502995355f6169a766a7d7e'
PARENT = '5509461903bd56c25d92208d84da7a37171a4fc4'
sha = lambda data: hashlib.sha256(data).hexdigest()
def load(path, digest=None):
    data = path.read_bytes()
    if digest is not None: assert sha(data) == digest, path
    return json.loads(data)

R145 = load(WAVE/'r147-apt-regression/R147_APT_LINKED.json', BASE_RECEIPT_SHA)
manifest = load(WAVE/'r148-batch/MANIFEST.json', MANIFEST_SHA)
assert manifest['base'] == PARENT and manifest['projected_tree'] == TREE
assert R145['head'] == PARENT and R145['tree'] == manifest['base_tree']
verify = runpy.run_path(str(WAVE/'r145-linked-proof/verify_linked.py'))
old = verify['old']
composed = load(WAVE/'r145-composed-proof/R145_COMPOSED_COFF.json', verify['COMPOSED_SHA'])

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('link_dir', type=Path)
    parser.add_argument('--expected-head', required=True)
    args = parser.parse_args()
    assert len(args.expected_head)==40 and all(c in '0123456789abcdef' for c in args.expected_head)
    seal, objects, entries, read, hashes = verify['sealed_link'](args.link_dir.resolve(),args.expected_head)
    provider_sha = 'c5c92ccf8c16714ff97a55d668503d9bcec7279687c1a54cd6ec5dca6e2c9833'
    provider_path = WAVE/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json'
    provider = load(provider_path, provider_sha)
    assert provider['head'] == PARENT and provider['state'] == 'pass'
    assert seal['provider_selection']['sha256'] == provider_sha
    assert Path(seal['provider_selection']['path']).resolve() == provider_path.resolve()
    assert seal['provider_cohort'] == provider['sources'] and len(seal['provider_cohort']) == 15
    assert seal['prior_successful_seal']['sha256'] == R145['link_sha256']['seal']
    git=['git','--no-optional-locks','-C',seal['source_root']]
    def g(*args): return subprocess.check_output([*git,*args],text=True).strip()
    assert g('rev-parse',args.expected_head+'^{tree}') == TREE
    assert g('rev-parse',args.expected_head+'^') == PARENT
    diff=g('diff','--name-only',PARENT,args.expected_head).splitlines()
    assert set(diff)=={r['path'] for r in manifest['final_paths']}
    assert set(objects)==set(R145['selected_objects'])
    prior_seal=load(WAVE/'r147-sdk-link/seal.json', R145['link_sha256']['seal'])
    def seal_sources(doc):
        root=Path(doc['source_root'])
        return {str(Path(row['path']).relative_to(root)).replace('\\','/'): row['sha256'] for row in doc['sources'] if Path(row['path']).is_relative_to(root)}
    before_sources=seal_sources(prior_seal)
    current_sources=seal_sources(seal)
    frozen={}
    blobs={}
    for stem, row in R145['selected_objects'].items():
        path,data,newrow=objects[stem]
        assert row['source']==newrow['source']
        old_data=Path(row['object']).read_bytes()
        assert sha(old_data)==row['sha256']
        assert sha(data)==newrow['sha256']
        # Exact byte identity proves the R146 link selected the already-verified provider.
        assert newrow['sha256']==row['sha256'], stem
        src=row['source']
        base_blob=g('rev-parse',PARENT+':'+src)
        new_blob=g('rev-parse',args.expected_head+':'+src)
        assert base_blob==new_blob, src
        data_blob=subprocess.check_output([*git,'show',args.expected_head+':'+src])
        blob_hash=sha(data_blob)
        source_hash=current_sources[src]
        assert before_sources[src]==source_hash, src
        assert source_hash in {blob_hash, sha(data_blob.replace(b'\n',b'\r\n'))}, src
        blobs[src]={'git_blob':new_blob,'blob_sha256':blob_hash,'sealed_source_sha256':source_hash}
        frozen[stem]=old_data
    linked,proofs,only=verify['bind'](objects,frozen,verify['FUNCTIONS'],entries,read)
    assert len(proofs)==48 and len(only)==3
    assert set(proofs)==set(R145['functions']) and set(only)==set(R145['compiled_only'])
    slots=old['verify_dispatch'](linked,proofs,entries)
    legacy=old['verify_paths'](linked,proofs,entries,read)
    classes=old['verify_class_paths'](linked,proofs)
    native=verify['check_new_paths'](linked,proofs,only,composed,entries,read)
    receipt={'schema':'r148-apt-linked-v1','head':args.expected_head,'tree':TREE,'base':PARENT,
      'manifest_sha256':MANIFEST_SHA,'r147_receipt_sha256':BASE_RECEIPT_SHA, 'provider_selection_sha256':provider_sha,
      'link_sha256':hashes,'selected_objects':{stem:row for stem,(_,_,row) in objects.items()},
      'selected_source_blobs':blobs,'dispatch_slots':slots,'retained_paths':legacy,'class_paths':classes,
      'native_paths':native,'functions':proofs,'compiled_only':only,'runtime':'not run'}
    out=ROOT/'R148_APT_LINKED.json'
    with out.open('x',encoding='utf8',newline='\n') as f: json.dump(receipt,f,indent=2); f.write('\n')
    print(str(out),sha(out.read_bytes()))
if __name__=='__main__': main()
