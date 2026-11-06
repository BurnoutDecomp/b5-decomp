import hashlib,json,os,pathlib,shutil,subprocess

OUT=pathlib.Path(__file__).resolve().parent
OLD=OUT.parent/'r26-batch'
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331'
SOCKET='88ede2a673efaa9b51d55df5f69dc3edc3c1a66b'
STREAM='931f0276c42d7c6af2a530374033131faad6cea8'
SOCKET_PATHS={'vendor/dirtysdk/src/x360/dirtynet_'+name+'_x360.cpp' for name in ('addr','lookup')}
STREAM_PATHS={'src/GameShared/GameClasses/System/FileSystem/CgsStreamDeviceDiskRead.'+ext for ext in ('cpp','h')}

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()
def pin(path):
    return {'path':str(path.resolve()),'sha256':sha(path)}
def read(path):
    return json.loads(path.read_text(encoding='utf-8-sig'))
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=W,env=env)
def tree_entry(rev,path):
    return git('ls-tree',rev,'--',path).decode().split('\t')[0].strip() or None
def idx_entry(env,path):
    stage=git('ls-files','--stage','--',path,env=env).decode().split()
    if not stage:
        return None
    assert stage[2]=='0'
    return f'{stage[0]} blob {stage[1]}'
def gate_paths(path,count):
    gate=read(path)
    assert gate['requested']==gate['passed']==count and gate['failed']==0
    assert all(row['machine']==0x01f2 for row in gate['results'])
    return [path,*[pathlib.Path(row['assembly']) for row in gate['results']]]

assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
old_manifest=OLD/'MANIFEST.json'
old_patch=OLD/'r26.patch'
review_path=OLD/'REVIEW.json'
assert sha(old_manifest)=='39260420200bda95216090f50363b9d706af375d4888fb5b301ecb5b1f8dd552'
assert sha(old_patch)=='58adf48c65b480a9b38480541050c5825b7af4a4384ed456577fadc28e26c95d'
assert sha(review_path)=='0d070df4ca1310054366ba0c327aa2747a80942ae74646d98e0a735b2583aafe'
review=read(review_path)
assert review['verdict']=='fail' and review['manifest_sha256']==sha(old_manifest) and review['patch_sha256']==sha(old_patch)
m=read(old_manifest)
index=OUT/'projection.index'
assert not index.exists()
shutil.copyfile(OUT.parent/'r26-integrated-preflight/projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
for row in m['final_paths']:
    assert idx_entry(env,row['path'])==row['new']
assert git('diff','--cached','--binary','--no-ext-diff',BASE,env=env)==old_patch.read_bytes()
socket_gate=OUT.parent.parent/'wave21/socket-alignment-terra/native2/compile-results.json'
stream_delivery=OUT.parent/'r26-stream-lock-root/DELIVERY.json'
stream=read(stream_delivery)
assert stream['commit']==STREAM and set(stream['paths'])==STREAM_PATHS
groups=m['groups'][:]
for name,commit,parent,expected,receipts in (
    ('socket-alignment',SOCKET,'0788e4c4a376707b8792a79fd6d2b6b8e7213436',SOCKET_PATHS,gate_paths(socket_gate,2)),
    ('stream-lock',STREAM,'8a41fda10605a9e780610d6331a6cabd8c6ab000',STREAM_PATHS,
     [stream_delivery,*gate_paths(pathlib.Path(stream['native_gate']['path']),4)])):
    assert git('rev-parse',commit+'^').decode().strip()==parent
    paths=git('diff','--name-only',parent,commit).decode().splitlines()
    assert set(paths)==expected,(name,paths)
    entries=[{'path':p,'old':tree_entry(parent,p),'new':tree_entry(commit,p)} for p in paths]
    for row in entries:
        assert idx_entry(env,row['path'])==row['old'],(name,row)
    patch=OUT/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,commit))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    for row in entries:
        assert idx_entry(env,row['path'])==row['new'],(name,row)
    groups.append({'name':name,'base':parent,'commit':commit,'patch':pin(patch),'paths':entries,
                   'receipts':[pin(receipt) for receipt in receipts]})
paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
final=[{'path':p,'old':tree_entry(BASE,p),'new':idx_entry(env,p)} for p in paths]
assert len(final)==58 and len(groups)==12
new_files=[r['path'] for r in final if r['old'] is None]
assert new_files==m['new_files']
assert {r['path'] for r in m['final_paths'] if r not in final}==SOCKET_PATHS
assert {r['path'] for r in final}-{r['path'] for r in m['final_paths']}==STREAM_PATHS
out_patch=OUT/'r26.patch'
assert not out_patch.exists()
out_patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',BASE,env=env))
git('apply','--index','--check',str(out_patch))
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
manifest={**m,'schema':'r26-native-intake-amended-v1','original_manifest':pin(old_manifest),
          'original_review':pin(review_path),'groups':groups,'final_paths':final,'patch':pin(out_patch)}
out=OUT/'MANIFEST.json'
out.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('MANIFEST',sha(out),'PATCH',sha(out_patch))
