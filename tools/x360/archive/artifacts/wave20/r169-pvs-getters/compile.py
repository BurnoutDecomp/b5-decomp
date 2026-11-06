import json,subprocess,pathlib,hashlib
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r169-pvs-getters'); root=pathlib.Path(r'C:\Users\adri1\.codex\worktrees\bp360\resource_sentinel_terra'); previous=json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r168-pvs-output-layout\compile-results.json').read_text()); item=next(x for x in previous if x['source'].endswith('/BrnPVSModule.cpp')); src=item['source']; cmd=[]
for arg in item['command']:
 if arg.startswith('/Fo'): cmd.append('/Fo'+str(out/'BrnPVSModule.obj'))
 elif arg.startswith('/Fa'): cmd.append('/Fa'+str(out/'BrnPVSModule.asm'))
 else: cmd.append(arg)
r=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); (out/'compile.log').write_bytes(r.stdout); obj=out/'BrnPVSModule.obj'; proof={'source':src,'source_sha256':hashlib.sha256((root/src).read_bytes()).hexdigest(),'command':cmd,'exit_code':r.returncode,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest() if r.returncode==0 else None}; (out/'compile-results.json').write_text(json.dumps(proof,indent=2));print(r.stdout.decode(errors='replace')); print('exit',r.returncode); print('sha',proof['object_sha256'])
