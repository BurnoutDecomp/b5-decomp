"""Compile the actual producer latch block; optional source captures baseline."""
from pathlib import Path
import os,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from run_render_part_interpolation import REPO,WORKFLOW,settings

def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else REPO/'src/GameSource/World/BrnWorldModule.cpp'
    source=path.read_text(encoding='utf-8-sig')
    start=source.index('// [FLAG PC bring-up] The console latches the frame camera')
    end=source.index('// ---- the projection scalars',start)
    with tempfile.TemporaryDirectory(prefix='brn_world_camera_latch_') as directory:
        out=Path(directory)
        (out/'world_camera_latch_body.inc').write_text(source[start:end],encoding='utf-8')
        includes=' '.join('/I"'+str(WORKFLOW/p)+'"' for p in settings('msvc_includes.txt'))
        command='cl '+' '.join(settings('msvc_flags.txt'))+' '+includes+' /I"'+str(out)+'" "'+str(Path(__file__).with_name('PlaytestWorldCameraLatch.cpp'))+'" /Fe:regression.exe /link /OPT:REF'
        script=out/'run.cmd';script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+command+'\nif errorlevel 1 exit /b 1\nregression.exe\n',encoding='utf-8',newline='\r\n')
        env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
        result=subprocess.run(['cmd','/c',str(script)],cwd=out,env=env)
        return result.returncode
if __name__=='__main__':raise SystemExit(main())
