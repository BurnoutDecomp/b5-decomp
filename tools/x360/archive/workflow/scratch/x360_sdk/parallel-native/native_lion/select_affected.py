from pathlib import Path
import json,re,subprocess,hashlib,os
r=Path(r'C:/Users/adri1/.codex/worktrees/bp360/native_lion')
files=[r/p for p in subprocess.check_output(['git','ls-files'],cwd=r,text=True).splitlines() if Path(p).suffix.lower() in ('.h','.hpp','.cpp','.c','.inl') and (r/p).is_file()]
known={p:p.relative_to(r).as_posix() for p in files}
roots=[r/'src',r/'vendor/renderware/include',r/'src/SDKs/EATech',r/'src/GameShared/GameClasses/Development']
rev={}
for p in files:
 s=p.read_text(encoding='utf-8-sig',errors='replace')
 for inc in re.findall(r'^\s*#\s*include\s*["<]([^">]+)[">]',s,re.M):
  for root in [p.parent]+roots:
   q=Path(os.path.normpath(root/inc.replace('\\','/')))
   if q in known:
    rev.setdefault(q,set()).add(p);break
changed=subprocess.check_output(['git','diff','--name-only'],cwd=r,text=True).splitlines()
seen={ (r/p) for p in changed};queue=list(seen)
while queue:
 for p in rev.get(queue.pop(),set()):
  if p not in seen:seen.add(p);queue.append(p)
sources=sorted(known[p] for p in seen if p in known and p.suffix.lower() in ('.cpp','.c') and not any(x.lower()=='pc' for x in p.relative_to(r).parts))
output=Path(r'C:/Users/adri1/.codex/worktrees/fbaf/BP-Decomp_Workflow/scratch/x360_sdk/parallel-native/native_lion')
(output/'affected-sources.json').write_text(json.dumps({'changed':changed,'sources':sources},indent=2)+'\n')
print(json.dumps({'affected_count':len(sources),'sources':sources},indent=2))
