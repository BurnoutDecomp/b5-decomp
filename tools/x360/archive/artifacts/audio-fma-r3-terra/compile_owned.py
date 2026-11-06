import os,json,subprocess,hashlib
from pathlib import Path
source=Path('D:/Reverse/BP-X360-Experimental-Worktrees/audio-fma-r3-terra');out=Path('D:/Reverse/BP-X360-Experimental-Artifacts/audio-fma-r3-terra/native');out.mkdir(exist_ok=True);(out/'temp').mkdir(exist_ok=True);env=os.environ.copy();env['TEMP']=env['TMP']=str(out/'temp');old=json.loads(Path('D:/Reverse/BP-X360-Experimental-Artifacts/native-static-sound-map-root/native-final/compile-results.json').read_text());template=old['results'][0]['command'];files=['vendor/renderware/src/rw/audio/core/Xas1Enc.cpp','vendor/renderware/src/rw/audio/core/XasEnc.cpp','vendor/renderware/src/rw/audio/core/Butterworth.cpp'];rows=[]
for file in files:
 stem=Path(file).stem;cmd=[v.replace('D:\\Reverse\\BP-X360-Experimental-Worktrees\\drive-thru-native-root',str(source).replace('/','\\')) for v in template];cmd[-1]=file;cmd=[('/Fo'+str(out/(stem+'.obj'))) if v.startswith('/Fo') else ('/Fa'+str(out/(stem+'.asm'))) if v.startswith('/Fa') else v for v in cmd];r=subprocess.run(cmd,cwd=source,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True);(out/(stem+'.log')).write_text(r.stdout,encoding='utf-8');headers=[]
 for line in r.stdout.splitlines():
  if 'including file:' in line:
   p=Path(line.split('including file:',1)[1].strip())
   if p.exists() and p.is_file():headers.append({'path':str(p),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 rows.append({'source':file,'sha256':hashlib.sha256((source/file).read_bytes()).hexdigest(),'command':cmd,'returncode':r.returncode,'object_sha256':hashlib.sha256((out/(stem+'.obj')).read_bytes()).hexdigest() if r.returncode==0 else None,'headers':headers});print(file,r.returncode,r.stdout[-800:] if r.returncode else '',flush=True)
(out/'compile-results.json').write_text(json.dumps(rows,indent=2),encoding='utf-8')
