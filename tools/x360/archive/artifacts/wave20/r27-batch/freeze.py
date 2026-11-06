import hashlib,json,os,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
wave=out.parent
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='3792b7d0f752405cbf160c530832492df09da639'
combined='8a39520b92539c98e42693e0c3345d31f0cf584a'
initial=wave/'r27-root-preflight'
compile_receipt=wave/'r27-root-combined/DELIVERY.json'
def git(*args,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):
    return {'path':str(p.resolve()),'sha256':sha(p)}
def read(p):
    return json.loads(p.read_text())
def parse(data,staged=False):
    rows={}
    for line in data.decode().splitlines():
        info,path=line.split('\t',1)
        if staged:
            mode,blob,stage=info.split()
            assert stage=='0'
            info=f'{mode} blob {blob}'
        rows[path]=info
    return rows
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
completion=wave/'r26-intake-repaired/completion.json'
link=wave/'r26-link/result.json'
assert read(completion)['head']==base and read(completion)['passed']==3672
assert read(link)['all_post_checks_pass'] and sha(wave/'r26-link/link.log')==read(link)['log_sha256']
m=read(initial/'PROJECTION.json')
r=read(compile_receipt)
assert m['base']==r['base']==base and r['head']==combined and r['compiled']==133
assert sha(initial/'r27-preflight.patch')==r['patch_sha256'] and sha(wave/'r27-root-combined/native/compile-results.json')==r['compile_sha256']
index=out/'projection.index'
assert not index.exists()
env=dict(os.environ,GIT_INDEX_FILE=str(index))
git('read-tree',combined,env=env)
active=parse(git('ls-files','--stage',env=env),True)
before=parse(git('ls-tree','-r',base))
initial_paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
assert [{'path':p,'old':before.get(p),'new':active.get(p)} for p in initial_paths]==m['final_paths']
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==(initial/'r27-preflight.patch').read_bytes()
groups=m['groups'][:]
for name,head,receipts in (
    ('fft-allocation','47a2fdfb0b42793db10deef6af549870c822307b',[wave/'fft-lifetime-root/DELIVERY.json']),
    ('native-immediate-state','e08884c6696c796d116e4d33a115e28fe42caace',
     [wave/'r26-imstate-terra/evidence.json',wave/'r26-imstate-terra/final.patch']),
):
    assert git('rev-parse',head+'^').decode().strip()==base
    paths=git('diff','--name-only',base,head).decode().splitlines()
    end=parse(git('ls-tree','-r',head,'--',*paths))
    assert paths and all(active.get(p)==before.get(p) for p in paths),(name,paths)
    patch=out/(name+'.patch')
    patch.write_bytes(git('diff','--binary','--no-ext-diff',base,head))
    git('apply','--cached','--check',str(patch),env=env)
    git('apply','--cached',str(patch),env=env)
    active=parse(git('ls-files','--stage',env=env),True)
    assert all(active.get(p)==end.get(p) for p in paths)
    groups.append({'name':name,'base':base,'head':head,'paths':paths,'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
final=[{'path':p,'old':before.get(p),'new':active.get(p)} for p in paths]
assert all(row in final for row in m['final_paths']) and len(final)==28 and len(groups)==6
new=[p for p in paths if p not in before]
assert sum(p.endswith('.cpp') for p in new)==6 and sum(p.endswith('.h') for p in new)==4
patch=out/'r27.patch'
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
manifest={'schema':'r27-native-intake-v1','base':base,'source_root':str(w),'groups':groups,
          'patch':pin(patch),'final_paths':final,'new_files':new,'base_compiled':3672,'expected_total_native':3678,
          'base_compile':pin(completion),'base_link':pin(link),'initial_preflight':pin(initial/'PROJECTION.json'),
          'initial_combined_gate':pin(compile_receipt)}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R27 MANIFEST',sha(out/'MANIFEST.json'),'PATCH',sha(patch))
