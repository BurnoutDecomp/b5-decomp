"""ARTIST82652F08 selective replay input construction; CPU only.

--omit-timer removes only the timer Clear call in temporary extracted output.
This is an explicit omission probe, not an execution of an old source tree.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, compile_and_run, definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--omit-timer', action='store_true')
args = parser.parse_args()
tree = Tree()
source = tree.read('src/GameSource/Replays/BrnReplayModuleIO.cpp')
construct = definition(source, 'void InputBuffer_PreSim::Construct()')
if args.omit_timer:
    if construct.count('mTimerStatusInterface.Clear();') != 1:
        raise SystemExit('omission probe requires exactly one production timer Clear')
    construct = construct.replace('mTimerStatusInterface.Clear();', '', 1)
timer = definition(tree.read('src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.cpp'),
                   'CgsSystem::TimerStatusInterface::Clear()')
bodies = ('void\n' + timer + '\nnamespace BrnReplays { namespace ReplayIO {\n' + construct + '\n'
          + definition(source, 'const CgsSystem::TimerStatusInterface* InputBuffer_PreSim::GetTimerStatusInterface() const')
          + '\n} }\n')
numeric = compile_and_run(Path(__file__).with_name('ReplayPreSimIO.cpp'),
    'replay_presim_io.inc', bodies, 'ReplayPreSimIO', extra_sources=[
        REPO/'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp', STRSTREAM_CPP])
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
print(f'run_replay_presim_io: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
