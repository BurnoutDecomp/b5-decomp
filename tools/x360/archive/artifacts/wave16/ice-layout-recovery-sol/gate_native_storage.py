from pathlib import Path
import sys,os,json,subprocess,hashlib,datetime
root=Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/cxx03_five_luna_04ed')
a=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave16/ice-layout-recovery-sol')
prior=json.loads(Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave14/game_director_sol/attempt07-final-typed-context/compile-results.json').read_text(encoding='utf8'))
default_sources=[r['source'] for r in prior['results']]+[str(p.relative_to(root)).replace(chr(92),'/') for p in (root/'src/SDKs/Packages/ICE').glob('ICEController*.cpp')]
attempt=a/sys.argv[1];attempt.mkdir(exist_ok=False);temp=attempt/'temp';temp.mkdir()
sources=default_sources if len(sys.argv)==2 else sys.argv[2:]
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
pre={str(p.relative_to(root)):sha(p) for p in root.joinpath('src').rglob('*') if p.is_file() and p.suffix.lower() in ['.h','.hpp','.inl']}
pre.update({p:sha(root/p) for p in sources})
(attempt/'context-before.json').write_text(json.dumps(pre,indent=2))
cmd=[sys.executable,str(root/'tools/build_x360.py'),'--sdk',r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/build/x360/sdk-6534.4/portable/XDK','--out',str(attempt),'--jobs','1','--timeout','180','--quiet','--compiler-arg=/showIncludes','--compiler-arg=/d1reportSingleClassLayoutBrnGameModule','--compiler-arg=/d1reportSingleClassLayoutDirectorModule','--compiler-arg=/d1reportSingleClassLayoutReplayDirector','--compiler-arg=/d1reportSingleClassLayoutICEController','--compiler-arg=/d1reportSingleClassLayoutICEManager','--compiler-arg=/d1reportSingleClassLayoutICEWrapper','--compiler-arg=/d1reportSingleClassLayoutMainDirector']
for p in sources:cmd+=['--source',p]
vendor=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/b5-decomp/vendor')
for p in ['EABase/include/Common','EASTL/include','EAThread/include','renderware/include','PPMalloc/include','coreallocator/include','dirtysdk/include','zlib/include','lua/src']:cmd+=['--compiler-arg=/I'+str(vendor/p)]
(attempt/'launcher-command.json').write_text(json.dumps(cmd,indent=2))
env=dict(os.environ);env['TEMP']=env['TMP']=str(temp)
r=subprocess.run(cmd,cwd=root,env=env,capture_output=True,text=True,errors='replace')
(attempt/'launcher.log').write_text(r.stdout+r.stderr,encoding='utf8')
print('Compiler launcher exit',r.returncode)
report=json.loads((attempt/'compile-results.json').read_text());bindings={}
for row in report['results']:
 for l in Path(row['log']).read_text(encoding='utf8').splitlines():
  if l.startswith('Note: including file:'):
   p=Path(l.split('Note: including file:',1)[1].strip());bindings[str(p)]={'sha256':sha(p),'private_pre_sha256':pre.get(str(p.relative_to(root))) if p.is_relative_to(root) else None}
(attempt/'showIncludes-bindings.json').write_text(json.dumps(bindings,indent=2))
post={p:sha(root/p) for p in pre};changes=[p for p in pre if pre[p]!=post[p]]
(attempt/'context-check.json').write_text(json.dumps({'unchanged':not changes,'changed_paths':changes,'post':post},indent=2))
print('Context unchanged:',not changes)
for row in report['results']:
 print(row['source'],row['passed'])
 if not row['passed']:
  print('\n'.join(l for l in Path(row['log']).read_text(encoding='utf8').splitlines() if 'error C' in l))
sys.exit(r.returncode)
