import json, subprocess, os, pathlib, hashlib
out=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r145-apt-cih-flags')
prev=json.loads(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave20/r143-corrected-preflight/compile-results.json').read_text())
template=prev['results'][0]['command']; root=pathlib.Path.cwd(); files=['AptAnimationTarget.cpp','AptCIH.cpp','AptCIHBehaviour.cpp','AptDisplayList.cpp','AptGC.cpp','AptKey.cpp','AptLinker.cpp']; results=[]
for name in files:
 src='src/SDKs/EATech/include/Apt/'+name; stem=name[:-4]; obj=out/(stem+'.obj'); asm=out/(stem+'.asm'); log=out/(stem+'.log')
 cmd=[x for x in template if not (x.startswith('/Fo') or x.startswith('/Fa') or x.lower().endswith('.cpp'))]+['/Fo'+str(obj),'/Fa'+str(asm),src]
 p=subprocess.run(cmd,stdout=subprocess.PIPE,stderr=subprocess.STDOUT);log.write_bytes(p.stdout); machine=int.from_bytes(obj.read_bytes()[:2],'little') if obj.exists() else None
 results.append(dict(source=src,passed=p.returncode==0 and machine==498,returncode=p.returncode,machine=machine,command=cmd,log=str(log),object=str(obj),assembly=str(asm)))
 print(name,p.returncode,machine,flush=True)
(out/'compile-results.json').write_text(json.dumps(dict(stage='compile-only',target='xbox360-ppc-be',results=results,passed=sum(r['passed'] for r in results),failed=sum(not r['passed'] for r in results)),indent=2))
