import hashlib,json,os,subprocess
from pathlib import Path
out=Path(__file__).resolve().parent
wave=out.parent; artifact=wave.parent
w=Path('C:/Users/adri1/.codex/worktrees/bp360/wave4-intake')
base='6c0bcba0af1e35b05da730b8fd7a24a592e95cf5'
combined='49ae375da93dafe66b3d165eeb614cecaaa3fa50'
combined_dir=wave/'r28-root-combined'
initial=wave/'r28-root-preflight'
fix='src/GameShared/GameClasses/Gui/CgsSaveLoadX360_wB_04.cpp'
def git(*args,env=None):return subprocess.check_output(['git','--no-optional-locks',*args],cwd=w,env=env)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def pin(p):return {'path':str(p.resolve()),'sha256':sha(p)}
def read(p):return json.loads(p.read_text())
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
completion=wave/'r27-intake-final/completion.json'
link=wave/'r27-link/result.json'
assert read(completion)['head']==base and read(completion)['passed']==3679
assert read(link)['all_post_checks_pass'] and sha(wave/'r27-link/link.log')==read(link)['log_sha256']
m=read(initial/'PROJECTION.json'); rr=read(combined_dir/'DELIVERY.json')
assert m['base']==rr['base']==base and rr['head']==combined and rr['compiled']==141
assert sha(combined_dir/'verified/compile-results.json')==rr['compile_sha256']
index=out/'projection.index';assert not index.exists()
env=dict(os.environ,GIT_INDEX_FILE=str(index))
git('read-tree',combined,env=env)
active=parse(git('ls-files','--stage',env=env),True); before=parse(git('ls-tree','-r',base))
original_paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
assert set(original_paths)=={r['path'] for r in m['final_paths']}|{fix}
assert all(before.get(r['path'])==r['old'] and active.get(r['path'])==r['new'] for r in m['final_paths'])
assert sha(combined_dir/'r28-combined-repaired.patch')==rr['patch_sha256']
assert git('diff','--cached','--binary','--no-ext-diff',base,env=env)==(combined_dir/'r28-combined-repaired.patch').read_bytes()
groups=m['groups']
# Re-pin the exact RealmC source tuple evidence independently of a changed prose receipt.
realm=[r for r in groups if r['name'] in ('realmc-native-interface','realmc-simple-tasks')]
rp=out/'REALMC_NATIVE_COMPOSITION.json'
legacy=[]
for g in realm:
    paths=g['paths']; pre=parse(git('ls-tree','-r',g['base'],'--',*paths)); post=parse(git('ls-tree','-r',g['head'],'--',*paths))
    g['exact_paths']=[{'path':p,'old':pre.get(p),'new':post.get(p)} for p in paths]
    kept=[]
    for item in g['receipts']:
        file=Path(item['path'])
        if sha(file)==item['sha256']:kept.append(item);continue
        assert file.name=='COMPOSITION_BASE_RECEIPT.json'
        legacy.append({'expected':item,'observed':pin(file),'reason':'Prose receipt was edited after initial preflight. Replaced with independent exact Git tuple proof.'})
    g['receipts']=kept
p='src/GameShared/GameClasses/Gui/CgsSaveLoadPS3.cpp'
assert b'Native Xenon SaveLoad implementation' in git('show',base+':'+p)
rp.write_text(json.dumps({'base':base,'combined':combined,'historical_filename_is_xbox_native':p,
    'groups':[{'name':g['name'],'base':g['base'],'head':g['head'],'paths':g['exact_paths']} for g in realm],
    'caller_repair':{'path':fix,'old':before.get(fix),'new':active.get(fix),'artist':'0x82852368 slot +0x30'},
    'legacy_receipts':legacy,'verified_combined_gate':pin(combined_dir/'DELIVERY.json')},indent=2)+'\n')
for g in realm:g['receipts'].append(pin(rp))
for g in groups:
    for a in [g['patch'],*g['receipts']]:assert sha(Path(a['path']))==a['sha256'],a
groups.append({'name':'realmc-autosave-caller','base':base,'head':combined,'paths':[fix],
               'patch':pin(combined_dir/'realmc-caller-repair.patch'),
               'receipts':[pin(rp),pin(combined_dir/'realmc-caller-repair/compile-results.json')]})
conn=artifact/'wave27/connapi-lifecycle-luna'
conn_fix=artifact/'wave27/connapi-lifecycle-luna-abi-successor'
ping=artifact/'wave22/protoping-native-terra'
for name,parent,head,receipts in (
    ('connapi-native-lifecycle','f3cb84e9f8ef4c9c04d91d9f1f0281aa5f9f1331','ce55acf0f72b3b4a91e7f7f2e6824d49eacf2eec',
     [conn/'R27_SEALED_MANIFEST.json',conn/'R27_RECEIPT.md',conn_fix/'R27_ABI_SELECTOR_RECEIPT.md',conn_fix/'R27_ABI_SELECTOR_GATE.json']),
    ('protoping-native',base,'48c557b813d3ca2f96412883dc152fbb1e6bfa2c',
     [ping/'evidence.txt',ping/'native1/compile-results.json']),
):
    assert subprocess.run(['git','merge-base','--is-ancestor',parent,head],cwd=w,check=False).returncode==0
    paths=git('diff','--name-only',parent,head).decode().splitlines()
    pre=parse(git('ls-tree','-r',parent,'--',*paths));post=parse(git('ls-tree','-r',head,'--',*paths))
    assert paths and all(active.get(p)==before.get(p)==pre.get(p) for p in paths),(name,paths)
    patch=out/(name+'.patch');patch.write_bytes(git('diff','--binary','--no-ext-diff',parent,head))
    git('apply','--cached','--check',str(patch),env=env);git('apply','--cached',str(patch),env=env)
    active=parse(git('ls-files','--stage',env=env),True)
    assert all(active.get(p)==post.get(p) for p in paths)
    groups.append({'name':name,'base':parent,'head':head,'paths':paths,'patch':pin(patch),'receipts':[pin(p) for p in receipts]})
paths=git('diff','--cached','--name-only',base,env=env).decode().splitlines()
final=[{'path':p,'old':before.get(p),'new':active.get(p)} for p in paths]
assert all(r in final for r in m['final_paths']) and len(final)==38 and len(groups)==9
new=[p for p in paths if p not in before]
assert sum(p.endswith('.cpp') for p in new)==7 and sum(p.endswith('.h') for p in new)==4
patch=out/'r28.patch';patch.write_bytes(git('diff','--cached','--binary','--no-ext-diff',base,env=env))
git('apply','--index','--check',str(patch))
assert git('rev-parse','HEAD').decode().strip()==base and not git('status','--porcelain=v1').strip()
manifest={'schema':'r28-native-intake-v1','base':base,'source_root':str(w),'groups':groups,
          'patch':pin(patch),'final_paths':final,'new_files':new,'base_compiled':3679,'expected_total_native':3686,
          'base_compile':pin(completion),'base_link':pin(link),'initial_preflight':pin(initial/'PROJECTION.json'),
          'initial_combined_gate':pin(combined_dir/'DELIVERY.json'),'realmc_composition':pin(rp)}
(out/'MANIFEST.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('R28 MANIFEST',sha(out/'MANIFEST.json'),'PATCH',sha(patch))
