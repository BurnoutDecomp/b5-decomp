"""Rebind the sealed R145 Apt proof to a sealed R146 PPC image."""
import argparse, hashlib, json, runpy, subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
WAVE = ROOT.parent
R145_RECEIPT_SHA = '3068c9e1865e975e0ccb2ad694b79fd3dbaa0898f7a8079bdde00d55b709b257'
R146_MANIFEST_SHA = '45ac5262e4e502cbef5b1a810398924ca84d6aac925c4e55e6ff5d6d54a2269a'
TREE = '57da5aa21cbb1aa7959477b5ddd41cec69288541'
PARENT = '43044d5e77737e18572b7848676c6691720d5b95'
sha = lambda data: hashlib.sha256(data).hexdigest()
def load(path, digest=None):
    data = path.read_bytes()
    if digest is not None: assert sha(data) == digest, path
    return json.loads(data)

R145 = load(WAVE/'r145-linked-proof/R145_APT_LINKED.json', R145_RECEIPT_SHA)
manifest = load(WAVE/'r146-batch/MANIFEST.json', R146_MANIFEST_SHA)
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
    git=['git','--no-optional-locks','-C',seal['source_root']]
    def g(*args): return subprocess.check_output([*git,*args],text=True).strip()
    assert g('rev-parse',args.expected_head+'^{tree}') == TREE
    assert g('rev-parse',args.expected_head+'^') == PARENT
    diff=g('diff','--name-only',PARENT,args.expected_head).splitlines()
    assert set(diff)=={r['path'] for r in manifest['final_paths']}
    assert set(objects)==set(R145['selected_objects'])
    prior_seal=load(WAVE/'r145-link/seal.json', R145['link_sha256']['seal'])
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
    receipt={'schema':'r146-apt-linked-v1','head':args.expected_head,'tree':TREE,'base':PARENT,
      'manifest_sha256':R146_MANIFEST_SHA,'r145_receipt_sha256':R145_RECEIPT_SHA,
      'link_sha256':hashes,'selected_objects':{stem:row for stem,(_,_,row) in objects.items()},
      'selected_source_blobs':blobs,'dispatch_slots':slots,'retained_paths':legacy,'class_paths':classes,
      'native_paths':native,'functions':proofs,'compiled_only':only,'runtime':'not run'}
    out=ROOT/'R146_APT_LINKED.json'
    with out.open('x',encoding='utf8',newline='\n') as f: json.dump(receipt,f,indent=2); f.write('\n')
    print(str(out),sha(out.read_bytes()))
if __name__=='__main__': main()
