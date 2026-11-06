import hashlib,json,os,re,subprocess
from pathlib import Path
out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ai-route-lanes-terra'); root=Path('D:/Reverse/BP-X360-Experimental-Worktrees/ai_route_lanes_terra');prev=Path('D:/Reverse/BP-X360-Experimental-Artifacts/ai-route-lanes-terra/input-commands.json');rows=json.loads(prev.read_text());env=os.environ.copy();tmp=out/'tmp';tmp.mkdir(exist_ok=True);env['TEMP']=env['TMP']=str(tmp)
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def key(p):return os.path.normcase(os.path.normpath(str(p)))
old='D:\\Reverse\\BP-X360-Experimental-Worktrees\\world_ai_lanes_terra';new=str(root).replace('/','\\')
cmds=[];roots=[]
for row in rows:
 src=row['source'];name=Path(src).stem;cmd=[x.replace(old,new) for x in row['command']];cmd=[('/Fo'+str(out/(name+'.obj'))) if x.startswith('/Fo') else ('/Fa'+str(out/(name+'.asm'))) if x.startswith('/Fa') else x for x in cmd];cmd.insert(1,'/showIncludes');cmds.append((src,name,cmd))
for x in cmds[0][2]:
 if x.startswith('/I') and len(x)>2:
  path=Path(x[2:]);
  if path.is_dir():roots.append(path)
# Snapshot all possible header and extensionless SDK include files before compile.
pre={}
for r in roots:
 for p in r.rglob('*'):
  if p.is_file() and (not p.suffix or p.suffix.lower() in ('.h','.hpp','.hxx','.hh','.inc','.inl','.ipp')):
   k=key(p)
   if k not in pre:pre[k]=sha(p)
sources={src:sha(root/src) for src,_,_ in cmds};results=[];allok=True
for src,name,cmd in cmds:
 r=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(out/(name+'.log')).write_text(r.stdout,encoding='utf8');includes=[]
 for line in r.stdout.splitlines():
  m=re.search(r'including file:\s*(.*)$',line,re.I)
  if m:
   p=Path(m.group(1).strip());k=key(p)
   if not p.is_file():raise RuntimeError(('missing include',str(p)))
   before=pre.get(k);after=sha(p);includes.append({'path':str(p),'sha256_before':before,'sha256_after':after,'stable':before==after})
 unique={key(x['path']):x for x in includes};source_after=sha(root/src);stable=sources[src]==source_after and all(x['stable'] for x in unique.values());allok=allok and r.returncode==0 and stable and bool(unique)
 obj=out/(name+'.obj');result={'source':src,'source_sha256_before':sources[src],'source_sha256_after':source_after,'returncode':r.returncode,'object':str(obj),'object_sha256':sha(obj) if obj.exists() else None,'assembly_sha256':sha(out/(name+'.asm')) if (out/(name+'.asm')).exists() else None,'includes':list(unique.values()),'stable':stable,'command':cmd};results.append(result);print(src,'return',r.returncode,'includes',len(unique),'stable',stable,flush=True)
 report={'state':'pass' if allok and len(results)==len(cmds) else 'incomplete_or_failed','base':'ef64014612b510bffa6fa17bebe91461bd95a0ab','patches':['0d795d7cb85c13102bf688c868b6c75d30d2a4ca'],'private_head':'0d795d7cb85c13102bf688c868b6c75d30d2a4ca','snapshot_header_files':len(pre),'results':results}
(out/'binding-results.json').write_text(json.dumps(report,indent=2)+'\n')
