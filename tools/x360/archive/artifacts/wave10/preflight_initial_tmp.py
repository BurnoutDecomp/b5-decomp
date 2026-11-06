import json,pathlib,subprocess,hashlib,re,struct
R=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave10'); repo='C:/Users/adri1/.codex/worktrees/bp360/wave4-intake'; base='e572f7db4c533546832e42c6f3f73149071b5f82'; intake='b92f3e9aaf87cafc07bd80a9748064f66204c603'
names=['audio_cxx03_terra','scene_native_luna','d3d_native_terra','guide-native-root','world_native_luna']; out={}; allpaths=[]
def git(*a): return subprocess.check_output(['git','-C',repo,*a])
def sha(b):return hashlib.sha256(b).hexdigest()
def blob(c,p):return git('show',c+':'+p)
def match(b,h):return h in (sha(b),sha(b.replace(b'\r\n',b'\n')),sha(b.replace(b'\r\n',b'\n').replace(b'\n',b'\r\n')))
def filecheck(x,h=None):
 if isinstance(x,dict): h=x.get('sha256');x=x['path']
 p=pathlib.Path(x); return {'exists':p.exists(),'sha_ok':sha(p.read_bytes())==h if p.exists() and h else None}
for n in names:
 f=R/n/'DELIVERY.json';d=json.loads(f.read_text());c=d.get('commit',d.get('final_commit')); ps=git('diff-tree','--no-commit-id','--name-only','-r',c).decode().splitlines(); allpaths+=ps; rows=d.get('changed_paths',d.get('changed',[])); scene=d.get('nativeCompile',{}); rows=rows if n!='scene_native_luna' else scene['sources']; chk=[]
 for row in rows:
  p=row.get('path',row.get('source')); pre=row.get('pre_sha256',row.get('preGitBlobSha256'));post=row.get('post_sha256',row.get('postGitBlobSha256')); chk.append({'path':p,'base_receipt':match(blob(base,p),pre),'post_receipt':match(blob(c,p),post),'intake_preimage':blob(base,p)==blob(intake,p)})
 res=d.get('results') or scene.get('sources') or d.get('changed_paths'); gates=[]
 for x in res:
  cmd=x.get('command',x.get('compiler_command',[])); flags=[z for z in cmd if z.lower().startswith('/i') and '/vendor/' in z.lower().replace('\\','/')]; log=x.get('log'); obj=x.get('object'); asm=x.get('assembly',x.get('assembly_listing')); entry={'source':x.get('source',x.get('path')),'pass':x.get('passed',x.get('native_gate')=='pass'),'returncode':x.get('returncode'),'receipt_machine':x.get('machine'),'vendor_flags_count':len(flags)}
  for typ,v in [('object',obj),('assembly',asm),('log',log)]:
   if v:
    vv=v if isinstance(v,dict) else {'path':v,'sha256':x.get(typ+'_sha256')};entry[typ]=filecheck(vv)
    if typ=='object' and pathlib.Path(vv['path']).exists():entry['actual_machine']=struct.unpack('<H',pathlib.Path(vv['path']).read_bytes()[:2])[0]
    if typ=='log' and pathlib.Path(vv['path']).exists():entry['log_errors']=len(re.findall(r'\b(?:fatal\s+)?error\s+[A-Z]+\d+',pathlib.Path(vv['path']).read_text(errors='replace'),re.I))
  gates.append(entry)
 out[n]={'receipt_sha256':sha(f.read_bytes()),'commit':c,'parent':git('rev-parse',c+'^').decode().strip(),'paths':ps,'checks':chk,'gates':gates}
print(json.dumps({'duplicate_paths':len(allpaths)-len(set(allpaths)),'candidates':out},indent=2))
