"""Real ARTIST 82652F48 replay output lifecycle; CPU only.

--omit-status removes just the original flags reset from temporary extracted code.
This is a targeted omission control, not an old-tree execution.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode = True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, compile_and_run, definition

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--omit-status', action='store_true')
args = parser.parse_args()
source = Tree().read('src/GameSource/Replays/BrnReplayModuleIO.cpp')
construct = definition(source, 'void OutputBuffer_PreSim::Construct()')
if args.omit_status:
    if construct.count('mStatusInterface.mxStatusFlags = 0;') != 1:
        raise SystemExit('control requires exactly one production status reset')
    construct = construct.replace('mStatusInterface.mxStatusFlags = 0;', '', 1)
bodies = ('namespace BrnReplays { namespace ReplayIO {\n' + construct + '\n'
          + definition(source, 'void OutputBuffer_PreSim::Destruct()') + '\n} }\n')
numeric = compile_and_run(Path(__file__).with_name('ReplayPreSimOutputIO.cpp'),
    'replay_presim_output_io.inc', bodies, 'ReplayPreSimOutputIO', extra_sources=[
        REPO/'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',
        REPO/'src/GameShared/GameClasses/Gui/CgsGuiEventQueue.cpp', STRSTREAM_CPP])
if numeric is None:
    raise SystemExit(1)
checks, failures = numeric
print(f'run_replay_presim_output_io: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
