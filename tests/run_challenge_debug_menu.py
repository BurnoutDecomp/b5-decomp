"""Exercise all five Challenge Manager menu entries against their production bodies."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
source = tree.read('src/GameSource/GameState/ModeManager/ChallengeManager/BrnChallengeManagerDebugComponent.cpp')
signatures = (
    'void ChallengeManagerDebugComponent::OnActivate()',
    'void ChallengeManagerDebugComponent::StartChallenge(void* lpContext)',
    'void ChallengeManagerDebugComponent::CompleteAllChallenges(void* lpContext)',
)
methods = 'namespace BrnGameState {\n'
missing = []
for signature in signatures:
    try:
        methods += definition(source, signature) + '\n'
    except (ValueError, IndexError):
        missing.append(signature.strip())
methods += '}\n'
result = None if missing else compile_and_run(
    Path(__file__).with_name('ChallengeDebugMenu.cpp'),
    'challenge_debug_menu.inc', methods, 'ChallengeDebugMenu')
raise SystemExit(report('run_challenge_debug_menu',
                        [(f'production body present: {name}', name not in missing)
                         for name in signatures], result, 15))
