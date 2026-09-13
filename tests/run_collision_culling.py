"""Compile production culling bodies against real records and a minimal sound pool."""
from pathlib import Path
import subprocess
import tempfile

REPO = Path(__file__).resolve().parents[1]
WORKFLOW = REPO.parent

def settings(name):
    return [s for s in (WORKFLOW / 'tools/build' / name).read_text().splitlines()
            if s and not s.startswith('#')]

source = (REPO / 'src/GameSource/Sound/Collision/BrnCollisionStateManager.cpp').read_text(encoding='utf-8')
methods = []
for name in ('AddInputCollision', 'CullInputCollisions_RemoveDuplicates', 'CullAgainstPlaying', 'CullInputCollisions'):
    start = source.index('void CollisionStateManager::' + name + '(')
    end = source.index('\n}', start) + 2
    body = source[start:end].replace('CollisionStateManager::', 'FixtureManager::')
    body = body.replace('CgsSound::Logic::State', 'FixtureState').replace('CollisionState*', 'FixtureCollisionState*')
    methods.append(body)
fixture = Path(__file__).with_name('CollisionCulling.cpp').read_text(encoding='utf-8')
with tempfile.TemporaryDirectory(prefix='brn_collision_culling_') as directory:
    out = Path(directory)
    (out / 'fixture.cpp').write_text(fixture.replace('// INSERT_PRODUCTION_METHODS', '\n\n'.join(methods)), encoding='utf-8')
    includes = ' '.join(f'/I"{WORKFLOW / p}"' for p in settings('msvc_includes.txt'))
    command = 'cl ' + ' '.join(settings('msvc_flags.txt')) + ' ' + includes + ' fixture.cpp /Fe:regression.exe'
    script = out / 'run.cmd'
    script.write_text('@echo off\ncall "' + str(WORKFLOW / 'tools/build/msvc_env.bat')
                      + '" >nul 2>&1\nif errorlevel 1 exit /b 1\n' + command
                      + '\nif errorlevel 1 exit /b 1\nregression.exe\nexit /b %ERRORLEVEL%\n', newline='\r\n')
    subprocess.run(['cmd', '/c', str(script)], cwd=out, check=True)
