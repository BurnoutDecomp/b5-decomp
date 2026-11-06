import argparse,hashlib,json,os,pathlib,shutil,subprocess

OUT=pathlib.Path(__file__).resolve().parent
OLD=OUT.parent/'r25-batch'
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='c7376f3fed4105792eabdb9622d3ffefd7535009'
GAME='3fa937e7fa94e4f47c712f6b82379cfe181c6893'
GAME_RECEIPT=OUT.parent.parent/'wave19/lobby-finish-luna/game-event-mask-fix/RECEIPT.json'
FRIENDS_PATHS={'vendor/dirtysdk/src/x360/hlbudapi.cpp','vendor/dirtysdk/src/x360/xnfriends.cpp',
               'vendor/dirtysdk/src/x360/xnfriends_x360.h'}

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

parser=argparse.ArgumentParser()
parser.add_argument('--friends',required=True)
parser.add_argument('--friends-receipt',required=True)
args=parser.parse_args()
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
old_manifest=OLD/'MANIFEST.json'
old_patch=OLD/'r25.patch'
assert sha(old_manifest)=='6ab917f8b65e10bec9884e408b5fbad86af42cc3fd4e166489ff02bc2b6d726b'
assert sha(old_patch)=='b4281d4d1d7939c352989b029c48c012e4ddd67de97ce1f47c061d057e49b7fd'
review=read(OLD/'REVIEW.json')
assert review['verdict']=='fail' and review['manifest_sha256']==sha(old_manifest)
m=read(old_manifest)
index=OUT/'projection.index'
assert not index.exists()
shutil.copyfile(OLD/'projection.index',index)
env=dict(os.environ,GIT_INDEX_FILE=str(index))
for row in m['final_paths']:
    assert idx_entry(env,row['path'])==row['new']
groups=m['groups'][:]
for name,commit,expected,receipt in (
    ('game-manager-last-play',GAME,{'vendor/dirtysdk/src/x360/gamemanager.cpp'},GAME_RECEIPT),
    ('xnfriends-hlb-contract',args.friends,FRIENDS_PATHS,pathlib.Path(args.friends_receipt))):
    end=git('rev-parse',commit).decode().strip()
    parent=git('rev-parse',end+'^').decode().strip()
    paths=git('diff','--name-only',parent,end).decode().splitlines()
    assert set(paths)==expected,(name,paths)
    entries=[{'path':p,'old':tree_entry(parent,p),'new':tree_entry(end,p)} for p in paths]
    for row in entries:
        assert idx_entry(env,row['path'])==row['old'],(name,row)
    patch=OUT/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,end))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    for row in entries:
        assert idx_entry(env,row['path'])==row['new'],(name,row)
    groups.append({'name':name,'base':parent,'commit':end,'patch':pin(patch),'paths':entries,'receipts':[pin(receipt)]})
paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
final=[{'path':p,'old':tree_entry(BASE,p),'new':idx_entry(env,p)} for p in paths]
assert len(final)==36 and len(groups)==15
new_files=[r['path'] for r in final if r['old'] is None]
assert sum(p.endswith('.cpp') for p in new_files)==4
assert sum(p.endswith('.h') for p in new_files)==2
differs={r['path'] for r in m['final_paths'] if r not in final}
assert differs=={'vendor/dirtysdk/src/x360/xnfriends.cpp','vendor/dirtysdk/src/x360/gamemanager.cpp'}
out_patch=OUT/'r25.patch'
assert not out_patch.exists()
out_patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',BASE,env=env))
git('apply','--index','--check',str(out_patch))
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
manifest={'schema':'r25-native-intake-amended-v1','base':BASE,'source_root':str(W),
          'original_manifest':pin(old_manifest),'original_review':pin(OLD/'REVIEW.json'),
          'groups':groups,'final_paths':final,'patch':pin(out_patch),'new_files':new_files,
          'base_compiled':3647,'expected_total_native':3651}
out=OUT/'MANIFEST.json'
out.write_text(json.dumps(manifest,indent=2)+'\n')
print('MANIFEST',sha(out),'PATCH',sha(out_patch))

