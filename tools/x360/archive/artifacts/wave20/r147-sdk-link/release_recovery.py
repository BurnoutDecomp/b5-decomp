import argparse, hashlib, json, pathlib, subprocess, sys
P=pathlib.Path
HERE=P(__file__).resolve().parent
ORIGINAL=HERE.parent/'r147-link'
PROOF=HERE.parent/'r147-xmedia-provider-proof/PROVIDER_SELECTION.json'
ORIGINAL_SHA='db48703b7b1fae57ff3d4addb72bff3e97eeca5bb0da081a3b2dd402c332b801'
HEAD='5509461903bd56c25d92208d84da7a37171a4fc4'
def sha(p):
 with P(p).open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def read(p):return json.loads(P(p).read_text(encoding='utf-8-sig'))
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--proof-sha256',required=True);ap.add_argument('--review',required=True);ap.add_argument('--review-sha256',required=True);args=ap.parse_args()
 assert len(args.proof_sha256)==len(args.review_sha256)==64 and all(c in '0123456789abcdef' for c in args.proof_sha256+args.review_sha256)
 assert sha(PROOF)==args.proof_sha256 and sha(args.review)==args.review_sha256 and sha(ORIGINAL/'seal.json')==ORIGINAL_SHA
 proof=read(PROOF);review=read(args.review);assert proof['head']==review['head']==HEAD and proof['original_seal_sha256']==review['original_seal_sha256']==ORIGINAL_SHA
 assert str(review['verdict']).lower()=='pass' and review['provider_selection_sha256']==args.proof_sha256
 scripts={name:sha(HERE/name) for name in ('prepare_seal.py','run_once.py','release_recovery.py')};assert review['scripts_sha256']==scripts
 assert not any((HERE/n).exists() for n in ('seal.json','ROOT_RELEASE.json','invocation.json','result.json'))
 subprocess.run([sys.executable,'-X','utf8','-B',str(HERE/'prepare_seal.py'),'--proof-sha256',args.proof_sha256,'--review',str(P(args.review).resolve()),'--review-sha256',args.review_sha256],check=True)
 seal=HERE/'seal.json';seal_sha=sha(seal);data=read(seal);assert data['head']==HEAD and data['provider_selection']['sha256']==args.proof_sha256 and data['review']['sha256']==args.review_sha256
 assert {name:sha(HERE/name) for name in scripts}==scripts and sha(PROOF)==args.proof_sha256 and sha(args.review)==args.review_sha256
 release=HERE/'ROOT_RELEASE.json'
 with release.open('x',encoding='utf-8') as f:json.dump({'action':'one strict R147 SDK-provider recovery link','sealed_sha256':seal_sha},f,indent=2);f.write('\n')
 subprocess.run([sys.executable,'-X','utf8','-B',str(HERE/'run_once.py'),'--sealed',str(seal),'--sealed-sha256',seal_sha,'--release',str(release)],check=True)
 result=read(HERE/'result.json');diag=read(HERE/'diagnostic-parsed.json');assert result['exit_code']==0 and result['all_post_checks_pass'] and not diag['unique_symbols'] and not diag['duplicate_messages']
 image=HERE/'native-game.exe';assert image.exists() and image.read_bytes()[:4]==b'XEX2' and result['outputs'][str(image)]['sha256']==sha(image)
 print('RECOVERY_LINK_SUCCESS',HEAD,sha(image),flush=True)
if __name__=='__main__':main()
