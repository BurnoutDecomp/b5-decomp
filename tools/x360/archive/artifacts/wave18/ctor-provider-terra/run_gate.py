import hashlib, json, os, pathlib, subprocess, sys
root = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Worktrees\shared_lanes_terra')
out = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave18\ctor-provider-terra')
report = json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r21-game-cxx03-repair-20261103\composite-active-native.json').read_text())
rsp = pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave17\r21-repaired-full-native-link-diagnostic\r21-repaired-strict-diagnostic.rsp').read_text()
rows = [r for r in report['results'] if '"'+r['object']+'"' in rsp and 'brnguiviewmodule.h' in pathlib.Path(r['log']).read_text(encoding='utf8',errors='replace').lower()]
sources = [r['source'] for r in rows] + ['src/GameSource/Gui/View/BrnViewModule.cpp']
assert len(sources)==41 and len(set(sources))==41
files = subprocess.check_output(['git','ls-files','src','vendor'], cwd=root, text=True).splitlines()
def sha(p): return hashlib.sha256(p.read_bytes()).hexdigest()
def freeze():
 return {p:sha(root/p) for p in files if (root/p).is_file() and pathlib.Path(p).suffix.lower() in ('.h','.hpp','.cpp','.c','.inl')}
before=freeze(); (out/'source-before.json').write_text(json.dumps(before,indent=2))
base = json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave8\repair-r21-game-cxx03-repair-20261103\native1-command.json').read_text())['argv']
args=[sys.executable,'-X','utf8','-B',str(root/'tools/build_x360.py'),'--sdk',base[2],'--out',str(out/'native41'),'--jobs','1','--timeout','180','--quiet']
args += [a for a in base if a.startswith('--compiler-arg=')]
for s in sources: args += ['--source',s]
(out/'invocation.json').write_text(json.dumps({'sources':sources,'argv':args},indent=2))
tmp=out/'temp';tmp.mkdir(exist_ok=True)
env=dict(os.environ,TEMP=str(tmp),TMP=str(tmp))
with (out/'driver.log').open('w',encoding='utf8') as f: result=subprocess.run(args,cwd=root,env=env,stdout=f,stderr=subprocess.STDOUT)
after=freeze(); (out/'source-after.json').write_text(json.dumps(after,indent=2))
(out/'exit.json').write_text(json.dumps({'exit':result.returncode,'source_unchanged':before==after},indent=2))
sys.exit(result.returncode or (0 if before==after else 1))
