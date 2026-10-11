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
submix = tree.read('src/GameSource/Sound/Module/LogicModule/BrnSubmixesEffect.cpp')
combined = tree.read('src/GameShared/GameClasses/Development/Log/CgsLogCombined.cpp')
output = tree.read('src/GameShared/GameClasses/Development/Log/CgsLogOutput.cpp')
stats = tree.read('src/GameSource/Sound/Debug/BrnSoundDebugStatistics.cpp')
missing = []
code = 'namespace BrnSound { namespace Debug {\n'
for signature in ('DebugComponent::DebugComponent()', 'void DebugComponent::Construct(',
                  'void ConsoleChangeCallback(', 'void DebugComponent::InstallConsole(',
                  'void DebugComponent::OnActivate(', 'Statistics::Statistics()',
                  'void Statistics::Construct('):
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
raise SystemExit(report('run_sound_debug_menu', [], result, 1))
