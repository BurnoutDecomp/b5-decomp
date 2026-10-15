"""Test production spatial bodies, voice-culling loop and PC clock calibration."""
import os
from pathlib import Path
from fxgs_common import REPO, Tree, compile_and_run, definition
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
tree = Tree()
spatial = tree.read('src/GameShared/GameClasses/Sound/Logic/Cgs3dEffectControl.cpp')
mic = tree.read('src/GameShared/GameClasses/Sound/Logic/CgsMicrophone.cpp')
governor = tree.read('vendor/renderware/src/rw/audio/core/CpuLoadBalancer.cpp')
system = tree.read('vendor/renderware/src/rw/audio/core/System.cpp')
text = 'namespace CgsSound { namespace Logic {\n'
for sig in ['MicrophoneSystem::MicrophoneSystem()', 'MicrophoneSystem::Microphone* MicrophoneSystem::GetMicrophone(',
            'void MicrophoneSystem::Microphone::SetMicrophoneMatrix(',
            'const rw::math::vpu::Matrix44Affine& MicrophoneSystem::Microphone::GetMicrophoneMatrix() const']:
    text += definition(mic, '\n' + sig) + '\n'
for sig in ['void Cgs3dEffectControl::UpdateDoppler(', 'void Cgs3dEffectControl::Generate3DParams(']:
    text += definition(spatial, sig).replace('Cgs3dEffectControl::', 'FixtureControl::') + '\n'
text += '} }\nnamespace rw { namespace audio { namespace core {\n'
text += 'static const f32 KF_ZERO = 0.0f, KF_VOICE_COST_SCALE = 64.0f, KF_MAX_LOAD = 100.0f, KF_FLT_MAX = 3.4028235e38f;\n'
for sig in ['Voice *CpuLoadBalancer::FindLowestPriorityVoice(', 'void CpuLoadBalancer::CullVoices(']:
    text += definition(governor, sig) + '\n'
text += definition(system, 'u32 GetCpuCycle()') + '\n} } }\n'
result = compile_and_run(Path(__file__).with_name('AudioSpatialAndBudget.cpp'),
                         'audio_spatial_budget.inc', text, 'AudioSpatialAndBudget')
raise SystemExit(0 if result and result[1] == 0 else 1)
