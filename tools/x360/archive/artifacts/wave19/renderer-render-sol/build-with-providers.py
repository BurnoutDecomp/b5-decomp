import importlib.util
import hashlib
import json
from pathlib import Path
import subprocess
import sys

dest = Path('D:/Reverse/BP-X360-Experimental-Artifacts/wave19/renderer-render-sol')
overlay = dest / 'provider-headers'
providers = {
    'SDKs/RenderEngineClub/MAIN/components/src/device.h': '53be630e26fb291f3c69cd97e0d6270c135912f1',
    'GameShared/GameClasses/Graphics/Dispatch/shadowingdevice.h': '62a858b61',
    'GameShared/GameClasses/Graphics/Dispatch/CgsDispatcher.h': '701c766f1',
    'GameShared/GameClasses/Graphics/CgsShaderConstants.h': '701c766f1',
}
for header, commit in providers.items():
    path = overlay / header
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(subprocess.check_output(['git', 'show', f'{commit}:src/{header}']))
if sys.argv[1] == 'render-final':
    particle_path = Path('C:/Users/adri1/.codex/worktrees/bp360/particles_native/src/GameSource/Effects/Particles/ParticleModule.h')
    particle = (dest / 'particle-owner-header.h')
    if not particle.exists():
        particle.write_bytes(particle_path.read_bytes())
    expected = '20a73b6802c58c5bbf9a46d3e02249fa461e63a0d0a858fdef9550b4fd29954a'
    assert hashlib.sha256(particle.read_bytes()).hexdigest() == expected
    relative = 'GameSource/Effects/Particles/ParticleModule.h'
    output = overlay / relative
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_bytes(particle.read_bytes())
    providers[relative] = {'owner_path': str(particle_path), 'sha256': expected}
(dest / 'provider-headers.json').write_text(json.dumps(providers, indent=2))
if sys.argv[1].startswith('render'):
    current = Path.cwd() / 'src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp'
    owner_base = dest / 'dispatcher-owner-base.cpp'
    owner_version = dest / 'dispatcher-owner-version.cpp'
    merged = dest / 'dispatcher-provider-combined.cpp'
    for path, revision in ((owner_base, '701c766f1^'), (owner_version, '701c766f1')):
        path.write_bytes(subprocess.check_output(['git', 'show', f'{revision}:src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp']))
    normalized = dest / 'dispatcher-statistics.cpp'
    normalized.write_bytes(current.read_bytes().replace(b'\r\n', b'\n'))
    result = subprocess.check_output(['git', 'merge-file', '-p', str(normalized), str(owner_base), str(owner_version)])
    merged.write_bytes(result)
    real_run = subprocess.run
    def compile_merged(command, *args, **kwargs):
        if isinstance(command, list) and command and Path(command[0]).name.lower() == 'cl.exe':
            if Path(command[-1]).name == 'CgsDispatcherCommands.cpp':
                command[-1] = str(merged)
        return real_run(command, *args, **kwargs)
    subprocess.run = compile_merged

spec = importlib.util.spec_from_file_location('build_x360', Path.cwd() / 'tools/build_x360.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)
build.FLAGS.append('/I' + str(overlay))
sys.argv = json.loads((dest / (sys.argv[1] + '-command.json')).read_text(encoding='utf-8-sig'))
sys.exit(build.main())
