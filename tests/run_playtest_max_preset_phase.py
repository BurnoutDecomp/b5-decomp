"""Extract the production debug phase/diagnostic bridge/prefix/post delivery.

--late-phase uses the saved pre-relocation caller/manager regions as a negative
control. --render-delay restores the mistaken per-render delay advancement.
Only telemetry sinks and the intervening engine boundary are captured.
"""
from pathlib import Path
import os,re,subprocess,sys,tempfile
sys.dont_write_bytecode=True
from fxdeformlat_common import definition,settings,REPO,WORKFLOW

def region(text):
    body=definition(text,'bool BrnGameModule::GameMain(')
    a=body.index('mDebugManager.Update(')
    b=body.index('UpdateRequestDoStepFrame();',a)
    return body[a:b]

def delivery(text,post):
    body=definition(text,'void DeformationManager::Update(')
    r=re.search(r'if \(miPlayerModelIndex >= 0 && mModelsAdded\.IsBitSet\(static_cast<u32>\(miPlayerModelIndex\)\)\)\s*\n\s*mDebugComponent\.RunMaxPresetProbePC\(lvfTimeStep\.x, '+str(post).lower()+r'\);',body)
    return r.group(0) if r else ''

def main():
    late='--late-phase' in sys.argv
    path=REPO/'src/GameSource/Physics/DeformationManager'
    debug=(path/'BrnDeformationDebugComponent.cpp').read_text(encoding='utf-8-sig')
    manager=(path/'BrnDeformationManager.cpp').read_text(encoding='utf-8-sig')
    game=(REPO/'src/GameSource/Game/BrnGameModule.cpp').read_text(encoding='utf-8-sig')
    prefix=definition(debug,'void DeformationDebugComponent::RunMaxPresetProbePC(')
    prefix=prefix[:prefix.index('char lacLine[768];')]+ 'gSamplePhases.push_back(gPhase);if(lbPostUpdate)++siPostSamples;\n}\n'
    if '--render-delay' in sys.argv:
        prefix,n=re.subn(r'if \(lbPostUpdate\)\s*\{\s*sfElapsed \+= lfTimeStep;\s*return;\s*\}',
                         'if(lbPostUpdate)return;sfElapsed+=lfTimeStep;',prefix,count=1)
        assert n==1
    bridge=definition(manager,'void DeformationManager::RunMaxPresetProbePCDebugUpdate(')
    pre=delivery(manager,False);post=delivery(manager,True)
    assert not pre and post
    assert 'RunMaxPresetProbePCDebugUpdate' in region(game)
    game_region=region(game)
    if late:
        game_region=re.sub(r'\s*static const bool sbMaxPresetProbePC = .*?mGameTimer.GetRate\(\) \* mGameTimer.GetScaleCurrent\(\)\);','\n',game_region,count=1,flags=re.S)
        pre=Path(__file__).with_name('data').joinpath('PlaytestMaxPresetLatePhase.inc').read_text(encoding='utf-8')
        assert 'RunMaxPresetProbePCDebugUpdate(' not in game_region
    with tempfile.TemporaryDirectory(prefix='brn_max_phase_') as p:
        out=Path(p)
        for name,body in [('prefix',prefix),('bridge',bridge),('game',game_region),('before',pre),('post',post)]:
            (out/('max_phase_'+name+'.inc')).write_text(body,encoding='utf-8')
        incs=' '.join('/I"'+str(WORKFLOW/p)+'"' for p in settings('msvc_includes.txt'))
        cmd='cl '+' '.join(settings('msvc_flags.txt'))+' '+incs+' /I"'+str(out)+'" "'+str(Path(__file__).with_name('PlaytestMaxPresetPhase.cpp'))+'" /Fe:probe.exe /link /OPT:REF'
        script=out/'build.cmd'
        script.write_text('@echo off\ncall "'+str(WORKFLOW/'tools/build/msvc_env.bat')+'" >nul 2>&1\n'+cmd+'\nexit /b %ERRORLEVEL%\n',encoding='utf-8',newline='\r\n')
        env=dict(os.environ);env.pop('NoDefaultCurrentDirectoryInExePath',None)
        compiled=subprocess.run(['cmd','/c',str(script)],cwd=out,env=env,capture_output=True,text=True)
        if compiled.returncode:print(compiled.stdout,compiled.stderr);return 1
        failures=0
        cases=[(enabled,frozen,roaming,startup,renders)
               for renders in [1,5]
               for enabled,frozen,roaming,startup in [(0,0,1,0),(0,1,1,1),(1,0,1,0),(1,1,1,0),(1,0,1,1),(1,1,1,1),(1,0,0,0),(1,1,0,1)]]
        cases += [(enabled,frozen,roaming,startup,renders,1,awake)
                  for renders in [1,5]
                  for enabled,frozen,roaming,startup,awake in [(1,1,1,0,60),(1,1,1,1,60),(1,1,1,0,-1),(1,0,1,0,-1),(0,1,1,0,60),(1,1,0,0,60)]]
        for args in cases:
            r=subprocess.run([str(out/'probe.exe'),*map(str,args)],cwd=out,env=env,capture_output=True,text=True)
            print(r.stdout.strip());failures+=int(r.returncode!=0)
        print('Max-preset phase:',len(cases),'scenarios,',failures,'failed')
        return int(failures!=0)
if __name__=='__main__':raise SystemExit(main())
