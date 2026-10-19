"""GameState review counterexamples using complete production HUD bodies.

Typed timer/resource/route/render spies expose cache selection, gated acquisition
and arrow endpoints. --rev bbac56ac is the immutable pre-repair negative control.
No native object-layout, complete renderer or hardware last-bit verdict is claimed.
"""
import argparse
import os
from pathlib import Path
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
s = t.read('src/GameSource/GameState/BrnGameStateDebugComponent.cpp')
code = 'namespace BrnGameState {\n'
for name in ['    const CgsID KAA_DEBUG_ROAD_IDS[67] =',
             '    const char KAA_DEBUG_ROAD_NAMES[67][30] =']:
    code += fx.definition(s, name)+';\n'
for name in ['    s32 GameStateDebugComponent::FindRoadNameIndexFromId(',
             '    void GameStateDebugComponent::DrawUpcomingRoads(',
             '    void GameStateDebugComponent::RenderHUD(',
             '    void GameStateDebugComponent::RenderNextLandmarkInfo(']:
    code += fx.definition(s, name)+'\n'
code += '\n}\n'
headers = ['src/pc/math/VmxFloat.h', 'src/SDKs/XboxMath/XMVectorACos.h']
shadow = {x: t.read(x) for x in headers if t.read(x)}
os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
numeric = fx.compile_and_run(Path(__file__).with_name('DebugMenuGameState.cpp'),
    'debug_menu_gamestate.inc', code, 'DebugMenuGameState', shadow=shadow)
raise SystemExit(fx.report('run_debug_menu_gamestate', [], numeric, 1))
