"""Exercise production Lion integration across main view and six cameras, without gameplay.

--rev selects the emitter kernels to test; the native cache comes from the working
tree so the preceding revision demonstrates repeated integration without its hooks.
"""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, code_only, compile_and_run, report, REPO

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read('src/SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleEmitter.cpp')
build = definition(source, 'cParticleEmitter::EParticleBuildResult cParticleEmitter::ParticleBuild(')
start = source.rfind('namespace', 0, source.index('s32 giBaseColourWithVarianceMonitor ='))
end = source.index(build) + len(build)
code = source[start:end] + '\n'
code += 'namespace {\n' + definition(source, '    void ApplyEmitterWeighting(') + '\n}\n'
for signature in ['void MatrixSimulationHelper::UpdateLocatorVelocity(',
                  'void VectorSimulationHelper::UpdateLocatorVelocity(',
                  'void LocalSimulationHelper::UpdateLocatorVelocity(']:
    code += definition(source, signature) + '\n'
code += 'template <class T>\n' + definition(source, 'u32 cParticleEmitter::SimulateParticlesInBucketGeneral(') + '\n'
code += '\n'.join(re.findall(r'template u32 cParticleEmitter::SimulateParticlesInBucketGeneral<[\s\S]*?;', source))
scene = code_only(tree.read('src/pc/gcm/renderengine/reflections/SceneParticles.cpp'))
wiring = [('main preparation and per-face rendering share the published simulation frame',
           scene.count('LionSimulation::Scope') == 2
           and 'lpData->muCurrentFrame' in scene and 'lrData.muCurrentFrame' in scene)]
result = compile_and_run(Path(__file__).with_name('PCLionSimulation.cpp'),
    'pc_lion_simulation.inc', code, 'PCLionSimulation', extra_sources=(
        REPO / 'src/pc/gcm/renderengine/reflections/LionSimulation.cpp',
        REPO / 'src/GameShared/GameClasses/Memory/PC/CgsLowMemoryPC.cpp'))
raise SystemExit(report('run_pc_lion_simulation', wiring, result, 1))
