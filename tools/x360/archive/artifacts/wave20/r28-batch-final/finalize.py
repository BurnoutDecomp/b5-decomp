import hashlib,json,os,shutil,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
old=out.parent/'r28-batch-repaired'
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
path='src/SDKs/Realmc/RealmcXenonUtil.cpp'
before='da8020c3ad575c2bc488192028945fbc6435bde8'
after='c69edc410f6e2e1dc6ba6efd707f9136019d8aec'
receipt=out.parent/'r28-realmc-notify-root/DELIVERY.json'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def pin(p):return {'path':str(p.resolve()),'sha256':sha(p)}
def entry(rev):return git('ls-tree',rev,'--',path).decode().split('\t')[0]
def indexed(env):
    mode,blob,stage=git('ls-files','--stage','--',path,env=env).decode().split()[:3]
    assert stage=='0'
    return f'{mode} blob {blob}'
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
m=read(old/'MANIFEST.json'); r=read(old/'REVIEW.json')
assert r['verdict']=='fail' and r['manifest_sha256']==sha(old/'MANIFEST.json')=='8727647bf4eb94462f1886b977beccab2952e091f32b5a94d5ca1ad5b4780c6b'
assert r['patch_sha256']==sha(old/'r28.patch')=='fc789f29f569e7c3e06b93fba35ea3c3637208b95a3704a1e9ad45b7e23c4715'
assert sha(old/'REVIEW.json')=='2252956711014c4a5923d9a9871ce5114fc9fe19b0ce9f1b6586b93f31a01bb5'
assert len(r['blockers'])==1 and git('rev-parse',after+'^').decode().strip()==before
assert git('diff','--name-only',before,after).decode().splitlines()==[path]
delivery=read(receipt)
assert delivery['head']==after and delivery['notification_mask']==1
for a in ('gate','assembly'):assert sha(Path(delivery[a]['path']))==delivery[a]['sha256']
index=out/'projection.index'
assert not index.exists()
shutil.copyfile(old/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==(old/'r28.patch').read_bytes()
assert indexed(env)==entry(before)
delta=out/'realmc-notification-mask.patch'
delta.write_bytes(git('diff','--binary','--no-ext-diff',before,after))
git('apply','--cached','--check',str(delta),env=env);git('apply','--cached',str(delta),env=env)
assert indexed(env)==entry(after)
final=[{**row,'new':entry(after)} if row['path']==path else row for row in m['final_paths']]
assert len(final)==38 and sum(a!=b for a,b in zip(final,m['final_paths']))==1
patch=out/'r28.patch';patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
group={'name':'realmc-notification-mask','base':before,'head':after,'paths':[path],
       'patch':pin(delta),'receipts':[pin(receipt)]}
manifest={**m,'schema':'r28-native-intake-final-v1','previous_manifest':pin(old/'MANIFEST.json'),
          'previous_review':pin(old/'REVIEW.json'),'final_paths':final,'patch':pin(patch),
          'groups':m['groups']+[group]}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R28 FINAL',sha(out/'MANIFEST.json'),sha(patch))
