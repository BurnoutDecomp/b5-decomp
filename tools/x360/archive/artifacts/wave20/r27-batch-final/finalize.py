import hashlib,json,os,shutil,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
old=out.parent/'r27-batch-repaired'
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='3792b7d0f752405cbf160c530832492df09da639'
path='vendor/dirtysdk/src/x360/dirtynet_io_x360.cpp'
before='324395c597622151467fe69c156224227a5dadb4'
after='ed20e187bbe66f498b207e7c9bb9053a75d4e458'
gate=out.parent.parent/'wave22/socket-io-terra/callback-abi/native1/compile-results.json'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(p.read_text())
def pin(p):return {'path':str(p.resolve()),'sha256':sha(p)}
def entry(rev):
    return git('ls-tree',rev,'--',path).decode().split('\t')[0]
def indexed(env):
    mode,blob,stage=git('ls-files','--stage','--',path,env=env).decode().split()[:3]
    assert stage=='0'
    return f'{mode} blob {blob}'
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
m=read(old/'MANIFEST.json')
r=read(old/'REVIEW.json')
assert r['verdict']=='fail' and r['manifest_sha256']==sha(old/'MANIFEST.json')=='e864403c22e2227a3475cff1a25400cbbfaa30c879456d0edf17e2fd507729c6'
assert r['patch_sha256']==sha(old/'r27.patch')=='78a64199b931551ca188bfa93522282ca2a741be8c4095edc8488ebe144ff64f'
assert len(r['blockers'])==1 and git('rev-parse',after+'^').decode().strip()==before
assert git('diff','--name-only',before,after).decode().splitlines()==[path]
g=read(gate)
assert g['requested']==g['passed']==1 and g['failed']==0 and g['results'][0]['machine']==0x1f2
index=out/'projection.index'
assert not index.exists()
shutil.copyfile(old/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==(old/'r27.patch').read_bytes()
assert indexed(env)==entry(before)
delta=out/'socket-callback-abi.patch'
delta.write_bytes(git('diff','--binary','--no-ext-diff',before,after))
git('apply','--cached','--check',str(delta),env=env)
git('apply','--cached',str(delta),env=env)
assert indexed(env)==entry(after)
final=[{**row,'new':entry(after)} if row['path']==path else row for row in m['final_paths']]
assert len(final)==34 and sum(a!=b for a,b in zip(final,m['final_paths']))==1
patch=out/'r27.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
group={'name':'socket-callback-abi','base':before,'head':after,'paths':[path],
       'patch':pin(delta),'receipts':[pin(gate),pin(Path(g['results'][0]['assembly']))]}
manifest={**m,'schema':'r27-native-intake-final-v1','previous_manifest':pin(old/'MANIFEST.json'),
          'previous_review':pin(old/'REVIEW.json'),'final_paths':final,'patch':pin(patch),
          'groups':m['groups']+[group]}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R27 FINAL',sha(out/'MANIFEST.json'),sha(patch),'PREVIOUS REVIEW',sha(old/'REVIEW.json'))
