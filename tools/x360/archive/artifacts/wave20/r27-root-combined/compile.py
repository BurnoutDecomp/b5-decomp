from pathlib import Path
import hashlib,json,os,subprocess
d=Path(__file__).resolve().parent
w=Path('D:/Reverse/BP-X360-Experimental-Worktrees/r1_vecfloat_repair_terra')
pre=d.parent/'r27-root-preflight'
base='3792b7d0f752405cbf160c530832492df09da639'
prior_head='f7591afcc13f4a79c084ca3a041118d58ce8f00b'
branch='codex/experimental-x360-r27-combined-preflight'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def git(*a):return subprocess.check_output(['git',*a],cwd=w).decode().rstrip()
m=json.loads((pre/'PROJECTION.json').read_text())
p=json.loads((d/'projection.json').read_text())
patch=pre/'r27-preflight.patch'
assert p['manifest_sha256']==sha(pre/'PROJECTION.json')=='7bfbae3717e49d247c0df9f2628aa8c64edefe3077ff2f31039211cc85914964'
assert sha(patch)==m['patch']['sha256'] and p['base']==m['base']==base
assert p['native_count']==3677 and len(p['selected_sources'])==133
if git('rev-parse','HEAD')==prior_head:
    assert not git('status','--porcelain=v1')
    git('switch','-c',branch,base)
    git('apply','--index','--check',str(patch))
    git('apply','--index',str(patch))
assert git('rev-parse','HEAD')==base and git('branch','--show-current')==branch
assert not git('diff','--name-only')
def worktree_has_blob(path,blob):
    return blob in (git('hash-object','--no-filters','--',path),git('hash-object','--path='+path,'--',path))
expected={r['path']:r['new'].split()[-1] for r in m['final_paths']}
assert set(git('diff','--cached','--name-only').splitlines())==set(expected)
for path,blob in expected.items():
    assert git('rev-parse',':'+path)==blob
    assert worktree_has_blob(path,blob)
prior=json.loads((d.parent/'r26-stream-lock-root/command.json').read_text())
args=prior[:prior.index('--source')]
args[args.index('--out')+1]=str(d/'native')
args[args.index('--jobs')+1]='4'
args+=['--timeout','180','--quiet']
for src in p['selected_sources']:args+=['--source',src]
(d/'command.json').write_text(json.dumps(args,indent=2)+'\n')
tmp=d/'temp';tmp.mkdir()
subprocess.run(args,cwd=w,env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp)),check=True)
report=d/'native/compile-results.json'
r=json.loads(report.read_text())
assert r['passed']==r['requested']==133 and r['failed']==0
assert all(row['machine']==0x1f2 for row in r['results'])
assert not git('diff','--name-only')
for path,blob in expected.items():
    assert git('rev-parse',':'+path)==blob and worktree_has_blob(path,blob)
msg=d/'commit-message.txt'
msg.write_text('Compose initial R27 Xbox 360 candidates for native verification\n\nCo-Authored-By: GPT-6 <noreply@openai.com>\n')
git('commit','--file',str(msg))
head=git('rev-parse','HEAD')
assert not git('status','--porcelain=v1') and git('rev-parse','HEAD^')==base
(d/'DELIVERY.json').write_text(json.dumps({'base':base,'head':head,'branch':branch,'projection_sha256':sha(d/'projection.json'),'patch_sha256':sha(patch),'compile_sha256':sha(report),'compiled':133,'scope':'Fresh native compile of affected source and prior include closure only. Full R27 integration, link, and launch pending.'},indent=2)+'\n')
print('R27_COMBINED_PASS',head,sha(report),flush=True)
