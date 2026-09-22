"""Replay the production AI attribute reset with real generated records and observed registry.
Optional first argument is a pre-fix AIDriver_Update.cpp snapshot.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import re
sys.dont_write_bytecode = True
from run_showtime_impulse import definition, settings, REPO, WORKFLOW

def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else REPO/'src/GameSource/World/AI/BrnAIDriver_Update.cpp'
    source=path.read_text(encoding='utf-8-sig')
    method='\n'.join(definition(source, '    void AIDriver::'+name+'()').replace('AIDriver::','DriverFixture::') for name in ('ResetAttribSysValues','ResetPIDTuningState'))
    constants=''
    with tempfile.TemporaryDirectory(prefix='brn_ai_attrib_') as directory:
        output=Path(directory)
        (output/'ai_attrib_methods.inc').write_text(constants+'\n'+method,encoding='utf-8')
        includes=' '.join(f'/I"{WORKFLOW / path}"' for path in settings('msvc_includes.txt'))
        command=('cl '+' '.join(settings('msvc_flags.txt'))+' '+includes
                 +f' /I"{output}" "{Path(__file__).with_name("AIAttribReset.cpp")}"'
                 +f' "{REPO / "src/GameSource/World/AI/PID/BrnPIDController.cpp"}" "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}" /Fe:regression.exe /link /OPT:REF')
        script=output/'run.cmd'
        script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\nif errorlevel 1 exit /b 1\n'+command+'\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n',encoding='utf-8',newline='\r\n')
        subprocess.run(['cmd','/c',str(script)],cwd=output,check=True)

if __name__=='__main__': main()
