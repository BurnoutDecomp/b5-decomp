"""Extract current diagnostic helper; optional source snapshot proves RED."""
from pathlib import Path
import os,re,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from fxdeformlat_common import definition,settings,REPO,WORKFLOW
def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else REPO/'src/GameSource/Director/BrnMainDirector.cpp'
    source=path.read_text(encoding='utf-8-sig')
    helper=definition(source,'bool BrnDiag_DirectorActionDiagOn(')
    constant=re.search(r'const s32 KI_DIRECTOR_ACTION_DIAG_MAX_LINES\s*=[^;]+;',source)[0]
    call='BrnDiag_DirectorActionDiagOn(action)' if '(s32 liActionType)' in helper else 'BrnDiag_DirectorActionDiagOn()'
    pieces=constant+'\n'+helper+'\nbool DiagCall(s32 action){return '+call+';}\n'
    with tempfile.TemporaryDirectory(prefix='brn_director_diag_') as directory:
        out=Path(directory);(out/'director_action_diag_methods.inc').write_text(pieces,encoding='utf-8')
        includes=' '.join('/I"'+str(WORKFLOW/p)+'"' for p in settings('msvc_includes.txt'))
        cmd='cl '+' '.join(settings('msvc_flags.txt'))+' '+includes+' /I"'+str(out)+'" "'+str(Path(__file__).with_name('PlaytestDirectorActionDiag.cpp'))+'" /Fe:probe.exe /link /OPT:REF'
        script=out/'build.cmd';script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+cmd+'\nexit /b %ERRORLEVEL%\n',encoding='utf-8',newline='\r\n')
        env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
        result=subprocess.run(['cmd','/c',str(script)],cwd=out,env=env,capture_output=True,text=True)
        if result.returncode:print(result.stdout,result.stderr);return result.returncode
        errors=0
        for mode in [0,1,2]:
            r=subprocess.run([str(out/'probe.exe'),str(mode)],cwd=out,env=env,capture_output=True,text=True)
            print(r.stdout.strip());errors+=int(r.returncode!=0)
        return 1 if errors else 0
if __name__=='__main__':raise SystemExit(main())
