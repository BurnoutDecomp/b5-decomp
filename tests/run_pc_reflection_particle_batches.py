"""Verify actual capture dispatch ranges without launching or reproducing gameplay."""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
source = Tree(args.rev).read('src/pc/gcm/renderengine/reflections/SceneParticles.cpp')
code = 'namespace CgsPC::Reflections {\n' + definition(source, 'u32 ParticleCapture::Render(') + '\n}\n'
result = compile_and_run(Path(__file__).with_name('PCReflectionParticleBatches.cpp'),
    'pc_reflection_particle_batches.inc', code, 'PCReflectionParticleBatches')
raise SystemExit(report('run_pc_reflection_particle_batches', [], result, 1))
