"""ARTIST review counterexamples against extracted production bodies.

Owner, particle, curve and draw boundaries are typed fixtures; shared vector and
ATan headers come from the selected revision. No native or hardware-bit verdict
is inferred. --rev 28d96636 is the frozen pre-repair negative control.
"""
import argparse
import os
from pathlib import Path
import re
import fxgs_common as fx

p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--rev')
p.add_argument('--source-root', type=Path)
p.add_argument('--workflow-root', type=Path)
a = p.parse_args()
if a.source_root:
    fx.REPO = a.source_root.resolve()
fx.WORKFLOW = a.workflow_root.resolve() if a.workflow_root else next(
    x for x in fx.REPO.parents if (x/'tools/build/msvc_flags.txt').is_file())
original_settings = fx.settings


def settings(name):
    values = original_settings(name)
    if name != 'msvc_includes.txt':
        return values
    return [str(fx.REPO.joinpath(*Path(x).parts[1:]))
            if Path(x).parts[:2] == ('b5-decomp', 'src')
            else str(fx.WORKFLOW/x) for x in values]


fx.settings = settings
t = fx.Tree(a.rev)
debug = t.read('src/GameSource/Effects/BrnEffectsDebugComponent.cpp')
module = t.read('src/GameSource/Effects/EffectsModule.cpp')
jump = t.read('src/GameSource/Effects/Jump/JumpStateMachine.cpp')
boost = t.read('src/GameSource/Effects/Boost/BoostStateMachine.cpp')
state = t.read('src/GameSource/Effects/EffectsStateMachine.cpp')
director = t.read('src/GameSource/Director/BrnMainDirector.cpp')
printer = t.read('src/GameSource/Director/DirectorModule/BrnDirectorModuleDebugPrinter.cpp')
corona = t.read('src/GameSource/Graphics/BrnCoronaManager.cpp')


def definitions(source, signatures):
    return '\n'.join(fx.definition(source, s) for s in signatures)


helpers = '\n'.join(fx.definition(debug, s) for s in
                    ['f32 JunkyardSinCosLane(', 'f32 JunkyardHeadingRadians(']
                    if s in debug)
constants = '\n'.join(re.findall(
    r'^\s*const f32 KF_(?:VAPOUR_(?:MIN_SPEED|DIR_LOW|DIR_MID)|EXHAUST_MAX_VEHICLE_SPEED|'
    r'EXHAUST_SMOKE_SPEED_SCALE|BOOST_TRANSITION_(?:MIN|MAX)_DEFAULT)[^;]*;',
    jump+'\n'+boost, re.M))
code = 'namespace BrnEffects {\n'+helpers+'\n'+constants+'\n'
code += definitions(debug, ['EffectsDebugJumping::EffectsDebugJumping()',
                           'EffectsDebugProps::EffectsDebugProps()',
                           'void EffectsDebugComponent::OnJunkyardEffectIndexChange(',
                           'void EffectsDebugComponent::OnJunkyardEffectValueChange('])
code += '\n'+fx.definition(module, 'Vector3 MaddSplat4(')
code += '\n'+fx.definition(module, 'void EffectsModule::HandleQADebugTests(')
code += '\nnamespace Curves {\n'+fx.definition(t.read('src/GameSource/Effects/Curves.cpp'),
    'f32 SmoothStep::Evaluate(const Vector3 &')+'\n}\n'
code += '\n'+fx.definition(jump, 'void JumpStateMachine::SetVapourBlend(')
code += '\n'+fx.definition(boost, 'EffectsState BoostStateMachine::OnDetermineNextState(')
code += '\n'+fx.definition(state, 'void EffectsStateMachine::Tick(')+'\n}\n'

# These two large producers are deliberately bounded to the exact changed
# blocks. The surrounding submission/query/backend paths are outside this test.
size_start = corona.index('        Vector2 lvSize;')
size_end = corona.index('        mBufferIterator.Write(', size_start)
code += '\nVector2 ScaleCorona(const CoronaParams& lCoronaTypeParams,f32 lfSizeScale) {\n'
code += corona[size_start:size_end]+'\nreturn lvSize; }\n'
world_map = fx.definition(director,
    '            if (maStateFlagTail[E_FLAG_TAIL_SHOW_WORLD_MAP_DEBUG] != 0 &&')
code += '\nvoid MainDirector::DrawWorldMap(IO* lpIO) {\n'+world_map+'\n}\n'
has_log = 'void DebugLog::Update(' in printer
code += f'\n#define DEBUG_LOG_BODY_PRESENT {int(has_log)}\n'
if has_log:
    code += '\nnamespace BrnDirector {\n'+fx.definition(printer, 'void DebugLog::Update(')+'\n}\n'

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
headers = ['src/pc/math/VmxFloat.h', 'src/SDKs/XboxMath/XMVectorATan.h']
shadow = {x: t.read(x) for x in headers if t.read(x)}
numeric = fx.compile_and_run(Path(__file__).with_name('DebugMenuEffectsDirector.cpp'),
    'debug_menu_effects_director.inc', code, 'DebugMenuEffectsDirector', shadow=shadow)
raise SystemExit(fx.report('run_debug_menu_effects_director', [], numeric, 1))
