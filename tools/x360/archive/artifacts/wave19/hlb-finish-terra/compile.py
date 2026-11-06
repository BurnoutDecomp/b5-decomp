import json,pathlib,subprocess,os,hashlib,time
root=pathlib.Path('D:/Reverse/BP-X360-Experimental-Worktrees/ai_accessors_terra'); out=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave19/hlb-finish-terra'); tmp=out/'tmp'; tmp.mkdir(exist_ok=True)
p=pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/r22-intake/composite-active-native.json'); d=json.loads(p.read_text()); cache={}; matches=[]
for r in d['results']:
 a=pathlib.Path(r['origin_report']).parent.parent/'actual-includes.json'
 if a not in cache: cache[a]=json.loads(a.read_text())['rows']
 row=cache[a][r['source']]
 if any(x.lower().replace('/','\\').endswith('\\vendor\\dirtysdk\\include\\hlbudapi.h') for x in row): matches.append(r['source'])
sources=['vendor/dirtysdk/src/x360/hlbudapi.cpp']+matches
(out/'cohort.json').write_text(json.dumps(sources,indent=2))
template=json.loads(pathlib.Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave18/hlb-bounds-terra/gate/results.json').read_text())[0]['command']; base=[x for x in template[:-1] if not x.startswith(('/Fo','/Fa'))]
env=os.environ.copy(); env['TEMP']=str(tmp); env['TMP']=str(tmp); env['PATH']=str(pathlib.Path(base[0]).parent)+os.pathsep+env['PATH']
results=[]
for i,src in enumerate(sources):
 tag=f'{i:03d}-'+pathlib.Path(src).stem; cmd=base+['/Fo'+str(out/(tag+'.obj')),'/Fa'+str(out/(tag+'.asm')),src]; proc=subprocess.run(cmd,cwd=root,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT); log=proc.stdout.decode('utf-8',errors='replace'); (out/(tag+'.log')).write_text(log,encoding='utf-8'); results.append({'source':src,'returncode':proc.returncode,'header_seen':'hlbudapi.h' in log.lower(),'command':cmd}); (out/'results.json').write_text(json.dumps(results,indent=2)); print(i+1,len(sources),src,proc.returncode,flush=True)
