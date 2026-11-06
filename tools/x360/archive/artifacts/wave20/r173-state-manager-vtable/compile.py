import json,subprocess,pathlib,hashlib
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r173-state-manager-vtable'); root=pathlib.Path(r'C:\Users\adri1\.codex\worktrees\bp360\native-replay-collision'); old=r'C:\Users\adri1\.codex\worktrees\bp360\resource_sentinel_terra'; data=json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r168-pvs-output-layout\compile-results.json').read_text()); base=data[0]['command']; sources=['src/GameShared/GameClasses/Sound/Logic/CgsStateManager.cpp','src/GameShared/GameClasses/Sound/Logic/CgsEnvironment.cpp','src/GameShared/GameClasses/Sound/Logic/CgsState.cpp','src/GameSource/Sound/Module/LogicModule/BrnStateManager.cpp','src/GameSource/Sound/Global/BrnGlobalStateManager.cpp','src/GameSource/Sound/Module/LogicModule/BrnEmitterStateManager.cpp','src/GameSource/Sound/Vehicles/BrnAIVehicleStateManager.cpp','src/GameSource/Sound/Module/LogicModule/BrnSoundLogicModule.cpp']; result=[]
for src in sources:
 name=pathlib.Path(src).stem; obj=out/(name+'.obj'); asm=out/(name+'.asm'); log=out/(name+'.log'); cmd=[]
 for arg in base:
  if arg.startswith('/Fo'): cmd.append('/Fo'+str(obj))
  elif arg.startswith('/Fa'): cmd.append('/Fa'+str(asm))
  elif arg.replace(chr(92),'/')==data[0]['source'].replace(chr(92),'/'): cmd.append(src)
  else: cmd.append(arg.replace(old,str(root)))
 p=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); log.write_bytes(p.stdout); result.append({'source':src,'source_sha256':hashlib.sha256((root/src).read_bytes()).hexdigest(),'exit_code':p.returncode,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest() if p.returncode==0 else None,'command':cmd}); print(name,p.returncode,flush=True)
(out/'compile-results.json').write_text(json.dumps(result,indent=2))
