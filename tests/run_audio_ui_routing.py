"""Production GUI sound routing and host sound/camera scheduling regression.
Use --rev HEAD for the negative control before the UI/listener fix.
"""
import argparse
import os
from pathlib import Path
from fxgs_common import Tree, code_only, compile_and_run, definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
hud = tree.read('src/GameSource/Sound/Global/BrnHUDEffect.cpp')
text = 'namespace BrnSound { namespace Logic {\n'
text += definition(hud, '\nvoid HUDEffect::Notify(') + '\n} }\n'
result = compile_and_run(Path(__file__).with_name('AudioUiRouting.cpp'),
                         'audio_ui_routing.inc', text, 'AudioUiRouting')
game = code_only(definition(tree.read('src/GameSource/Game/BrnGameModule.cpp'),
                            '\n    bool BrnGameModule::GameMain()'))
flow = code_only(definition(tree.read('src/GameSource/GameFlowController/TopLevel/BrnGameMainFlowInGameState.cpp'),
                            '\nvoid MainGameFlowStateInGame::Update()'))
source = tree.read('src/GameSource/Game/BrnGameModule.cpp')
helper = 'FinishInGameSoundFramePC()' in game
post_call = 'FinishInGameSoundFramePC()' if helper else 'DoUpdate_Sound('
finish = code_only(definition(source, '\n    void BrnGameModule::FinishInGameSoundFramePC()')) if helper else game
begin = code_only(definition(source, '\n    BrnSound::Module::Io::RootPreUpdateOutputBuffer* BrnGameModule::BeginInGameSoundFramePC()')) if helper else flow
pre_call = 'BeginInGameSoundFramePC()' if helper else 'DoPreUpdate_Sound('
order = [game.find(call) for call in ['DoUpdate_Director(true)', 'DoUpdate_Effects(',
                                     post_call, 'DoUpdate_GameStatePostWorld(']]
checks = [
    ('sound consumes the published director camera after effects', min(order) >= 0 and order == sorted(order)),
    ('one sound update per simulation step', game.count(post_call) == 1 and
     finish.count('DoUpdate_Sound(') == 1 and 'DoUpdate_Sound(' not in flow),
    ('pre-update still precedes the world', 'DoPreUpdate_Sound(' in begin and
     0 <= flow.find(pre_call) < flow.find('DriveInGameWorldUpdate(')),
    ('sound resources are forwarded before its output is destroyed',
     'GetResourceRequestInterface()->mRequestQueue);' in finish and
     finish.find('GetResourceRequestInterface()->mRequestQueue);') < finish.find('DestroyIOBuffer')),
]
for name, ok in checks:
    if not ok: print('FAIL:', name)
failed = sum(not ok for _, ok in checks)
print(f'AudioSoundScheduling: {len(checks)} checks, {failed} failures')
raise SystemExit(0 if result and result[1] == 0 and failed == 0 else 1)
