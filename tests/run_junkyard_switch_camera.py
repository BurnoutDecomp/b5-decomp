"""Vehicle-switch camera direction, ARTIST ArbStateCarSelect::Update @8226F5D0.

Compile the four COMPLETE production switch arms (ACTIVE, ROTATE, WAIT_DROP,
IDLE) with recording camera handles. The console toggles mbIsLeft then picks
this+298 when true, this+294 when false at 822701F4/1FC, 82270514/51C,
822707BC/7C4 and 82270A34/A3C. Prepare @8226F088/F0A4 assigns ShotList[1]
to +294 (LeftToRight) and [2] to +298 (RightToLeft). Tests also exercise
position notifications, modification/unfinished gates, respawn and timeout.

python b5-decomp/tests/run_junkyard_switch_camera.py [--rev REV | --source CPP]
Unset NoDefaultCurrentDirectoryInExePath for the MSVC runner.
"""
from pathlib import Path
import argparse
import re
import sys

sys.dont_write_bytecode = True
from fxgs_common import Tree, compile_and_run, definition, report

SOURCE = 'src/GameSource/Director/Arbitrator/States/BrnArbStateCarSelect.cpp'


def main():
    parser = argparse.ArgumentParser()
    group = parser.add_mutually_exclusive_group()
    group.add_argument('--rev')
    group.add_argument('--source', type=Path)
    args = parser.parse_args()
    source = args.source.read_text(encoding='utf-8-sig') if args.source else Tree(args.rev).read(SOURCE)
    update = definition(source, 'void ArbStateCarSelect::Update(ArbStateSharedInfo& lrSharedInfo)')
    # Preserve each arm intact, including all conditions and side effects; only
    # the untested surrounding states and Update's common prologue are omitted.
    arm = re.search(r'case E_STATE_ACTIVE:.*?(?=case E_STATE_WAIT_FOR_AUDIO:)', update, re.S)
    if not arm:
        raise ValueError('missing four contiguous production camera arms')
    constants = []
    for name in sorted(set(re.findall(r'\bK[A-Z_0-9]+\b', arm[0]))):
        declaration = re.search(r'^\s*(?:void\* const|const (?:f32|s32|bool)|const char\* const)\s+' + name + r'\s*=.*?;', source, re.M)
        if not declaration:
            raise ValueError('missing production constant ' + name)
        constants.append(declaration[0])
    inc = '\n'.join(constants) + '\nvoid State::Step() {\n'
    inc += 'auto& lrGameState = mGame; auto& lrSharedInfo = mShared; auto& lrResources = mResources; auto& lrManager = mManager;\n'
    inc += 'switch (meState) {\n' + arm[0] + '\ndefault: break;\n}}\n'
    numeric = compile_and_run(Path(__file__).with_name('JunkyardSwitchCamera.cpp'),
                              'junkyard_switch_camera.inc', inc, 'JunkyardSwitchCamera')
    return report('run_junkyard_switch_camera', [], numeric, 62)


if __name__ == '__main__':
    sys.exit(main())
