"""Probe accepts a parked frozen rig, dispatches the original callbacks once.

Only scheduling/selection/dispatch through the diagnostic boundary is exercised.
--pre-fix restores the removed IsFrozen wait in that production prefix to falsify
the parked-car scenario; it is a test-hook comparison, not engine parity evidence.
"""
from pathlib import Path
import os,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from fxdeformlat_common import definition,settings,REPO,WORKFLOW

def main():
    src=(REPO/'src/GameSource/Physics/DeformationManager/BrnDeformationDebugComponent.cpp').read_text(encoding='utf-8-sig')
    body=definition(src,'void DeformationDebugComponent::RunMaxPresetProbePC(')
    prefix=body[:body.index('char lacLine[768];')]+'++gWitness;\n}\n'
    if '--pre-fix' in sys.argv:
        line='if (!BrnDirector::Harness::gbArbitratorInRoaming) return;'
        prefix=prefix.replace(line,line+'\nif (lpPlayer->GetVehiclePhysics()->IsFrozen()) return;')
    with tempfile.TemporaryDirectory(prefix='brn_max_preset_probe_') as path:
        out=Path(path);(out/'max_preset_probe_prefix.inc').write_text(prefix,encoding='utf-8')
        incs=' '.join('/I"'+str(WORKFLOW/p)+'"' for p in settings('msvc_includes.txt'))
        cmd='cl '+' '.join(settings('msvc_flags.txt'))+' '+incs+' /I"'+str(out)+'" "'+str(Path(__file__).with_name('PlaytestMaxPresetProbe.cpp'))+'" /Fe:probe.exe /link /OPT:REF'
        script=out/'build.cmd';script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+cmd+'\nexit /b %ERRORLEVEL%\n',encoding='utf-8',newline='\r\n')
        env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
        subprocess.run(['cmd','/c',str(script)],cwd=out,env=env,check=True,capture_output=True,text=True)
        errors=0
        for frozen,roaming in [(0,1),(1,1),(0,0),(1,0)]:
            r=subprocess.run([str(out/'probe.exe'),str(frozen),str(roaming)],cwd=out,env=env,capture_output=True,text=True)
            print(r.stdout.strip());errors+=int(r.returncode!=0)
        print('Max-preset callback liveness:',4,'cases,',errors,'failures')
        return 1 if errors else 0
if __name__=='__main__':raise SystemExit(main())
