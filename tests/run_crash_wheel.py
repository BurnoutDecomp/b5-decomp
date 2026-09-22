"""Replay the production Road Rage wheel fallback with real wheels and RNG.
Optional first argument supplies a pre-fix DeformableObject_Update.cpp snapshot.
"""
from pathlib import Path
import sys, subprocess, tempfile
sys.dont_write_bytecode=True
from run_showtime_impulse import settings, REPO, WORKFLOW

def main():
    path=Path(sys.argv[1]) if len(sys.argv)>1 else REPO/'src/GameSource/Physics/DeformationManager/DeformationPhysics/BrnDeformableObject_Update.cpp'
    source=path.read_text(encoding='utf-8-sig')
    if '// ARTIST82649400..826495C8:' in source:
        start=source.index('        // ARTIST82649400..826495C8:')
        body=source[start:source.index('        (void)lpModuleInput;',start)]
        body=body.replace('SimpleVehiclePhysics::','') # fixture provides the base accessor
    else:
        body='' # pre-fix tail only logged; it had no wheel/RNG writes
    with tempfile.TemporaryDirectory(prefix='brn_crash_wheel_') as directory:
        out=Path(directory);(out/'crash_wheel_methods.inc').write_text('void Fixture::Update(s32 liGameMode,CgsNumeric::Random& lrRandom){ auto* lpVehicle=&vehicle;\n'+body+'\n}',encoding='utf-8')
        includes=' '.join(f'/I"{WORKFLOW/p}"' for p in settings('msvc_includes.txt'))
        sources=[Path(__file__).with_name('CrashWheel.cpp'),REPO/'src/GameShared/GameClasses/Numeric/CgsRandom.cpp',REPO/'src/GameShared/GameClasses/Development/CgsStrStream.cpp']
        command='cl '+' '.join(settings('msvc_flags.txt'))+' '+includes+f' /I"{out}" '+' '.join('"'+str(p)+'"' for p in sources)+' /Fe:test.exe /link /OPT:REF'
        script=out/'run.cmd';script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\nif errorlevel 1 exit /b 1\n'+command+'\nif errorlevel 1 exit /b 1\ntest.exe\nexit /b %ERRORLEVEL%\n',encoding='utf-8',newline='\r\n')
        subprocess.run(['cmd','/c',str(script)],cwd=out,check=True)
if __name__=='__main__':main()
