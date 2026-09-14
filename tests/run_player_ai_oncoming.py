"""Regression coverage for player camera handoffs and road-tag oncoming state."""
import sys
sys.dont_write_bytecode = True
from pathlib import Path
import subprocess, tempfile
from run_rival_impacts import WORKFLOW, REPO, settings
from run_rival_recovery_camera import definition


def main():
    base = REPO / 'src/GameSource/World/EntityModules/RaceCarEntityModule'
    tag = (base / 'BrnRaceCarEntityModule_ResetOnTrack.cpp').read_text(encoding='utf8')
    method = definition(tag, 'void RaceCarEntityModule::UpdateRaceCarCollisionTagging(')
    # Strip diagnostic-only blocks; keep every production branch and store verbatim.
    a = method.index('    // ⭐⭐ [DIAG collision-tag]')
    b = method.index('    // The console\'s own first gate:', a)
    method = method[:a] + method[b:]
    a = method.index('        // FLAG PC-platform leaf: opt-in observation')
    b = method.index('\n    }', a)
    method = method[:a] + method[b:]
    actions = (base / 'BrnRaceCarEntityModule.cpp').read_text(encoding='utf8')
    case = definition(actions, 'case BrnGameState::GameStateModuleIO::E_ACTION_SET_PLAYER_CAR_DRIVER:')
    case = 'void RaceCarEntityModule::HandleDriverAction(const CgsModule::Event* lpEvent, Output* lpOutput) { switch (7) {\n' + case + '\n} }'
    boost = (base / 'Boost/BrnBoostStrategy.cpp').read_text(encoding='utf8')
    methods = [definition(tag, 'inline u16 TagAISectionIndex'), method, case,
               definition(boost, 'void BoostStrategy::SetOncomingState'),
               definition(boost, 'void BoostStrategy::SetForceBoost')]
    with tempfile.TemporaryDirectory(prefix='brn_player_ai_') as directory:
        output = Path(directory)
        (output / 'player_ai_methods.inc').write_text('\n'.join(methods), encoding='utf8')
        includes = ' '.join(f'/I"{WORKFLOW / path}"' for path in settings('msvc_includes.txt'))
        command = ('cl ' + ' '.join(settings('msvc_flags.txt')) + ' ' + includes
                   + f' /I"{output}" "{Path(__file__).with_name("PlayerAIOncoming.cpp")}"'
                   + f' "{REPO / "src/SharedClasses/World/BrnCollisionTag.cpp"}"'
                   + f' "{REPO / "src/SharedClasses/Trigger/BrnRegion.cpp"}"'
                   + f' "{REPO / "src/GameShared/GameClasses/Development/CgsStrStream.cpp"}"'
                   + f' "{REPO / "src/GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.cpp"}"'
                   + ' /Fe:regression.exe /link /OPT:REF' )
        script = output / 'run.cmd'
        script.write_text('@echo off\ncall "' + str(WORKFLOW / 'tools/build/msvc_env.bat')
            + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
            + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n', encoding='utf8', newline='\r\n')
        subprocess.run(['cmd', '/c', str(script)], cwd=output, check=True)

if __name__ == '__main__': main()
