import hashlib,json,os,pathlib,subprocess
D=pathlib.Path(__file__).resolve().parent
W=pathlib.Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
BASE='f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331'
P=D.parent/'r26-integrated-preflight'
I=D.parent/'r25-intake-repaired'
L=D.parent/'r25-link'
def git(*a,env=None):
    return subprocess.check_output(['git','--no-optional-locks',*a],cwd=W,env=env)
def read(p):
    return json.loads(p.read_text(encoding='utf-8-sig'))
def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):
    return {'path':str(p.resolve()),'sha256':sha(p)}
def parsed(data,index=False):
    result={}
    for line in data.decode().splitlines():
        info,path=line.split('\t',1)
        if index:
            mode,oid,stage=info.split()
            assert stage=='0'
            info=f'{mode} blob {oid}'
        result[path]=info
    return result
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
complete=read(I/'completion.json')
assert complete['head']==BASE and complete['status']=='SEALED_NO_REGRESSIONS' and complete['passed']==3651
link=read(L/'result.json')
diag=read(L/'diagnostic-parsed.json')
assert link['all_post_checks_pass'] and sha(L/'link.log')==link['log_sha256']
m=read(P/'PROJECTION.json')
assert m['base']==BASE and len(m['groups'])==10
for g in m['groups']:
    for r in [g['patch'],*g['receipts']]:
        assert sha(pathlib.Path(r['path']))==r['sha256']
env=dict(os.environ,GIT_INDEX_FILE=str(P/'projection.index'))
before=parsed(git('ls-tree','-r',BASE))
after=parsed(git('ls-files','--stage',env=env),True)
paths=git('diff','--cached','--name-only',BASE,env=env).decode().splitlines()
final=[{'path':p,'old':before.get(p),'new':after.get(p)} for p in paths]
assert final==m['final_paths'] and len(final)==56
new=[r['path'] for r in final if r['old'] is None]
assert sum(p.endswith('.cpp') for p in new)==21 and sum(p.endswith('.h') for p in new)==4
patch=D/'r26.patch'
assert not patch.exists()
patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',BASE,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==BASE and not git('status','--porcelain=v1').strip()
out=D/'MANIFEST.json'
manifest={'schema':'r26-native-intake-v1','base':BASE,'source_root':str(W),
          'projection_proof':pin(P/'PROJECTION.json'),'groups':m['groups'],'final_paths':final,'patch':pin(patch),
          'new_files':new,'base_compiled':3651,'expected_total_native':3672,
          'base_compile':pin(I/'completion.json'),'base_link':pin(L/'result.json'),
          'base_link_unresolved':diag['unique_symbols'],'base_link_duplicates':len(diag['duplicate_messages'])}
out.write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
print('MANIFEST',sha(out),'PATCH',sha(patch))
