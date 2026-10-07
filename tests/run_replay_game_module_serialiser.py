"""Real canonical game replay channel, including 24-byte SIM timer round-trip.

--wrong-id changes only the extracted leaf's channel argument as a targeted probe.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import REPO, STRSTREAM_CPP, Tree, definition, compile_and_run

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--wrong-id',action='store_true')
args=parser.parse_args()
source=Tree().read('src/GameSource/Replays/Serialisers/BrnReplayGameModuleSerialiser.cpp')
body=definition(source,'void GameModuleSerialiser::Construct()')
if args.wrong_id:
    if body.count('BaseSerialiser::Construct(5,')!=1:
        raise SystemExit('channel probe requires exactly one original id argument')
    body=body.replace('BaseSerialiser::Construct(5,','BaseSerialiser::Construct(4,',1)
numeric=compile_and_run(Path(__file__).with_name('ReplayGameModuleSerialiser.cpp'),
    'replay_game_serialiser.inc','namespace BrnReplays {\n'+body+'\n}',
    'ReplayGameModuleSerialiser',extra_sources=[
        REPO/'src/GameSource/Replays/BrnReplayBaseSerialiser.cpp',
        REPO/'src/GameShared/GameClasses/Core/CgsStringUtils.cpp',STRSTREAM_CPP])
if numeric is None:
    raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_game_module_serialiser: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
