"""Exercise the production effect bank/gates and opaque debris pass admission."""
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
tree = Tree()
particles = tree.read('src/pc/gcm/renderengine/reflections/SceneParticles.cpp')
shadows = tree.read('src/pc/gcm/renderengine/shadows/SceneRender.cpp')
renderer = tree.read('src/GameSource/Graphics/BrnRendererModule.cpp')
code = 'namespace CgsPC::Reflections {\n' + definition(particles, 'bool ParticleCapture::HasDebrisShadow(') + '\n}\n'
code += 'namespace CgsPC::Shadows {\n' + definition(shadows, 'bool HasDebris(') + '\n}\n'
policy = re.search(r'    const auto\* lpSceneParticleDataPC =.*?;', renderer, re.S).group()
result = compile_and_run(Path(__file__).with_name('PCParticleShadow.cpp'), 'pc_particle_shadow.inc', code,
    'PCParticleShadow', extra_files={'pc_particle_policy.inc': policy + '\nreturn lpSceneParticleDataPC;\n'})
raise SystemExit(report('run_pc_particle_shadow', [], result, 1))
