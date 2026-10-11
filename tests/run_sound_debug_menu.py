"""Run recovered sound-menu bodies against instrumented log/UI/mixer endpoints."""
import argparse
import os
import re
from pathlib import Path
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
component = tree.read('src/GameSource/Sound/BrnDebugComponent.cpp')
collision_header = tree.read('src/GameSource/Sound/Collision/BrnCollisionStateManager.h')
submix = tree.read('src/GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.cpp')
combined = tree.read('src/GameShared/GameClasses/Development/Log/CgsLogCombined.cpp')
output = tree.read('src/GameShared/GameClasses/Development/Log/CgsLogOutput.cpp')
stats = tree.read('src/GameSource/Sound/Debug/BrnSoundDebugStatistics.cpp')
missing = []

# Define only the external cells needed by the extracted production methods. Their declared
# C++ types come from the production TU; ARTIST registration expectations are a separate
# checked-in table in SoundDebugMenu.cpp, never synthesized from OnActivate.
try:
    globals_start = component.index('extern s32 KI_DEBUG_PRINT_AUDIO_ALLOCATIONS;')
    globals_end = component.index('namespace BrnSound\n{\nnamespace Debug\n{\nCgsDev::Log::LogCombined', globals_start)
    code = '''namespace CgsSound { namespace Utils {
struct SlopeParams { f32 mfMinInput = 0, mfMaxInput = 0, mfMinOutput = 0, mfMaxOutput = 0; };
} }
'''
    code += component[globals_start:globals_end].replace('extern ', '') + '\n'
except ValueError:
    missing.append('component debug-global declarations')
    code = ''

# These are external owner cells that the production component includes by header rather than
# redeclaring in its own source. The collision declarations are copied from the owning header.
code += '''namespace BrnSound { namespace Vehicles { namespace Engines {
s32 KI_MONITOR_MAX_RPM = 0; f32 gfMaxRpmMonitor = 0.0f;
} } }
namespace BrnPhysics {
s32 KI_HOLD_CAR_IN_PLACE = 0; s32 KI_HOLD_CAR_SPINING_IN_AIR = 0;
f32 KF_SPINNING_SPEED_X = 0.0f; f32 KF_SPINNING_SPEED_Y = 0.0f; f32 KF_SPINNING_SPEED_Z = 0.0f;
}
namespace BrnSound { namespace Debug {
bool gbShowStatistics = false; bool gbResetStatistics = false;
bool gbStatisticsDrawDacGraph = false; s32 KI_SPEW_SPLICE_USAGES = 0;
} }
namespace CgsSound { namespace Logic {
void (*Cgs3dEffectControl::spDebugRenderingFunction)(const Cgs3dEffectControl*) = nullptr;
} }
'''
collision_scalars = []
for line in collision_header.splitlines():
    match = re.match(r'\s*extern\s+(volatile\s+)?(bool|s32|u32|f32)\s+(\w+)\s*;', line)
    if not match:
        continue
    qualifier, value_type, name = match.groups()
    initializer = '0.0f' if value_type == 'f32' else 'false' if value_type == 'bool' else '0'
    collision_scalars.append(f'{qualifier or ""}{value_type} {name} = {initializer};')
if not collision_scalars:
    missing.append('CollisionStateManager scalar declarations')
code += 'namespace BrnSound { namespace Logic { namespace Collision {\n'
code += '\n'.join(collision_scalars) + '\n} } }\n'

code += 'namespace BrnSound { namespace Debug {\n'
for signature in ('const char* const KA_COLLISION_MATERIAL_NAMES[25]',
                  'const char* const KA_COLLISION_ACTION_NAMES[6]'):
    try:
        code += definition(component, signature) + ';\n'
    except ValueError:
        missing.append(signature)
for signature in ('DebugComponent::DebugComponent()', 'void DebugComponent::Construct(',
                  'void ConsoleChangeCallback(', 'void DebugComponent::InstallConsole(',
                  'void DebugComponent::OnActivate(', 'void DebugComponent::BuildMaterialNames()',
                  'void DebugComponent::BuildActionNames()', 'void DebugComponent::BuildSplicerBankNames()',
                  'Statistics::Statistics()', 'void Statistics::Construct('):
    try:
        code += definition(stats if 'Statistics::' in signature else component, signature) + '\n'
    except ValueError:
        missing.append(signature)
code += '} }\nnamespace CgsDev { namespace Log {\n'
for source, signatures in ((combined, ('void LogCombined::AddStream(',
                                       'void LogCombined::RemoveStream(',
                                       'void LogCombined::RemoveAllStreams()',
                                       'void LogCombined::Append(')),
                           (output, ('int LogOutput::Append(',))):
    for signature in signatures:
        try:
            code += definition(source, signature) + '\n'
        except ValueError:
            missing.append(signature)
code += '} }\nnamespace BrnSound { namespace Logic {\n'
for line in submix.splitlines():
    if re.match(r'(?:static const )?f32 KF_(?:GLOBAL_|COLLISION_FILTER_)', line):
        code += line + '\n'
for signature in ('void SubmixesEffect::ProcessUpdate()',
                  'void SubmixesEffect::UpdateStaticPluginParameters('):
    try:
        code += definition(submix, signature) + '\n'
    except ValueError:
        missing.append(signature)
code += '} }\n'
if missing:
    print('Missing production bodies: ' + ', '.join(missing))
    raise SystemExit(1)
result = compile_and_run(Path(__file__).with_name('SoundDebugMenu.cpp'),
                         'sound_debug_menu.inc', code, 'SoundDebugMenu')
raise SystemExit(report('run_sound_debug_menu', [], result, 137))
