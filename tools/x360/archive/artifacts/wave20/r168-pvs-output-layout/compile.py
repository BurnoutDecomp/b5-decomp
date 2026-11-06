import json,subprocess,pathlib,hashlib
out=pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r168-pvs-output-layout'); root=pathlib.Path(r'C:\Users\adri1\.codex\worktrees\bp360\resource_sentinel_terra'); old=r'C:\Users\adri1\.codex\worktrees\bp360\debug_format_luna'; data=json.loads(pathlib.Path(r'D:\Reverse\BP-X360-Experimental-Artifacts\wave20\r157-native-preliminary\wheel-phase-preflight\compile-results.json').read_text()); base=data['results'][0]['command']; sources=['src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.cpp','src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/EventQueue_GetZoneResponse_8.cpp','src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModuleIO.cpp','src/GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.cpp','src/GameSource/World/EntityModules/WorldEntityModule/BrnWorldEntityModule.cpp','src/GameSource/World/EntityModules/WorldEntityModule/Bridges/WorldEntityBridgePVSToOutput.cpp','src/GameSource/World/BrnWorldModule.cpp']; result=[]
for ix,src in enumerate(sources):
 name=pathlib.Path(src).stem; obj=out/(name+'.obj'); asm=out/(name+'.asm'); log=out/(name+'.log'); cmd=[]
 for arg in base:
  if arg.startswith('/Fo'): cmd.append('/Fo'+str(obj))
  elif arg.startswith('/Fa'): cmd.append('/Fa'+str(asm))
  elif arg.replace(chr(92), '/')==data['results'][0]['source'].replace(chr(92), '/'):  cmd.append(src)
  else: cmd.append(arg.replace(old,str(root)))
 p=subprocess.run(cmd,cwd=root,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); log.write_bytes(p.stdout); result.append({'source':src,'source_sha256':hashlib.sha256((root/src).read_bytes()).hexdigest(),'exit_code':p.returncode,'object_sha256':hashlib.sha256(obj.read_bytes()).hexdigest() if obj.exists() else None,'command':cmd}); print(name,p.returncode,flush=True)
(out/'compile-results.json').write_text(json.dumps(result,indent=2))

