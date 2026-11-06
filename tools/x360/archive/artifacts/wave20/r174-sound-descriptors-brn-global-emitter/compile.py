import json,subprocess,pathlib,hashlib
out=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r174-sound-descriptors-brn-global-emitter'); root=pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Worktrees/audio-fma-r3-terra'); data=json.loads(pathlib.Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r173-state-manager-vtable/compile-results.json').read_text()); chosen=['BrnStateManager.cpp','BrnGlobalStateManager.cpp','BrnEmitterStateManager.cpp']; results=[]
for item in data:
 if not any(item['source'].endswith('/'+x) for x in chosen): continue
 src=item['source']; name=pathlib.Path(src).stem; cmd=[]
 for arg in item['command']:
  if arg.startswith('/Fo'): cmd.append('/Fo'+str(out/(name+'.obj')))
  elif arg.startswith('/Fa'): cmd.append('/Fa'+str(out/(name+'.asm')))
  else: cmd.append(arg.replace(r'C:\Users\adri1\.codex\worktrees\bp360\native-replay-collision',str(root)))
 r=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); (out/(name+'.log')).write_bytes(r.stdout); results.append({'source':src,'source_sha256':hashlib.sha256((root/src).read_bytes()).hexdigest(),'exit_code':r.returncode,'object_sha256':hashlib.sha256((out/(name+'.obj')).read_bytes()).hexdigest() if r.returncode==0 else None,'command':cmd});print(name,r.returncode,flush=True)
(out/'compile-results.json').write_text(json.dumps(results,indent=2));assert len(results)==3 and all(x['exit_code']==0 for x in results)
