from pathlib import Path
import argparse,os,json,hashlib,subprocess,re
OUT=Path(__file__).resolve().parent;ROOT=Path('D:/Reverse/BP-X360-Experimental-Worktrees/world-module-native-r1')
p=argparse.ArgumentParser();p.add_argument('--source',required=True);p.add_argument('--name',required=True);a=p.parse_args();source=Path(a.source);temp=OUT/(a.name+'-temp');temp.mkdir();env=os.environ.copy();env['TEMP']=env['TMP']=str(temp)
r=json.loads((OUT/'shading-candidate-1/native/compile-results.json').read_text())['results'][0];cmd=[x for x in r['command']if not x.startswith(('/Fo','/Fa'))];cmd[-1]=str(source);obj=OUT/(a.name+'.obj');asm=OUT/(a.name+'.asm');log=OUT/(a.name+'.log');cmd[-1:-1]=['/Fo'+str(obj),'/Fa'+str(asm)];env['PATH']=str(Path(cmd[0]).parent)+';'+env['PATH']
result=subprocess.run(cmd,cwd=ROOT,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180);text=result.stdout.decode('utf-8',errors='replace');log.write_text(text);includes={}
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
for line in text.splitlines():
 m=re.match(r'\s*Note: including file:\s*(.+?)\s*$',line)
 if m:includes[m[1]]=sha(m[1])
report={'command':cmd,'returncode':result.returncode,'passed':result.returncode==0,'source':str(source),'source_sha256':sha(source),'compiler_sha256':sha(cmd[0]),'private_temp':str(temp),'includes':includes,'artifacts':{str(p):sha(p)for p in[obj,asm,log]if p.exists()}};(OUT/(a.name+'-compile.json')).write_text(json.dumps(report,indent=2)+'\n');print('PASS'if report['passed']else'FAIL',a.name)
if not report['passed']:print('\n'.join(line for line in text.splitlines()if'including file:'not in line))
raise SystemExit(result.returncode)
