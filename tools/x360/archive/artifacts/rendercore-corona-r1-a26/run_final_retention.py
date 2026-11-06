from pathlib import Path
import subprocess,sys,json,time
out=Path(r'D:/Reverse/BP-X360-Experimental-Artifacts/rendercore-corona-r1-a26');results=[]
for name in ['validate_core_final.py','validate_submission_final.py']:
 print('START',name,flush=True);started=time.time();r=subprocess.run([sys.executable,'-X','utf8',str(out/name)],cwd=r'D:/Reverse/BP-X360-Experimental-Worktrees/rendercore-corona-r1-a26');results.append({'script':name,'exit_code':r.returncode,'seconds':time.time()-started});print('END',name,r.returncode,flush=True)
 if r.returncode:break
p=out/'final-retention-queue.json';assert not p.exists();p.write_text(json.dumps(results,indent=2)+'\n');raise SystemExit(any(r['exit_code'] for r in results))
