"""Real ARTIST replay reader and disk-range invalidation, CPU dependency oracles.

--truncate-frame and --wrong-end are targeted extracted-body controls, not old-tree runs.
"""
import argparse
import os
import sys
from pathlib import Path
sys.dont_write_bytecode=True
from fxgs_common import REPO,STRSTREAM_CPP,Tree,definition,compile_and_run

os.environ.pop('NoDefaultCurrentDirectoryInExePath',None)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--truncate-frame',action='store_true')
parser.add_argument('--wrong-end',action='store_true')
args=parser.parse_args()
tree=Tree()
source=tree.read('src/GameSource/Replays/Stream/BrnReplayReadStream.cpp')
disk_source=tree.read('src/GameSource/Replays/Stream/BrnReplayDiskReadStream.cpp')
source=('#include <windows.h>\nextern "C" long RtlInitializeCriticalSection(void*);\n'
        +source+'\nnamespace BrnReplays {\n'
        +definition(disk_source,'DiskReadStream::DiskReadStream()')+'\n}\n')
if args.truncate_frame:
    source=source.replace('return lrFirst.miFrameNumber;','return static_cast<s32>(lrFirst.miFrameNumber);',1)
if args.wrong_end:
    source=source.replace('mpStreamHeader->miFirstFrunk + mpStreamHeader->miNumFrunks)',
                          'mpStreamHeader->miFirstFrunk + mpStreamHeader->miNumFrunks - 1)',1)
writer_source=tree.read('src/GameSource/Replays/Stream/BrnReplayWriteStream.cpp')
writer='\n'.join(definition(writer_source,signature) for signature in (
    'void WriteStream::InvalidateFrunksAhead(', 'void WriteStream::ResetStartFrame('))
source+='\n#include "GameSource/Replays/Stream/BrnReplayWriteStream.h"\nnamespace BrnReplays {\n'+writer+'\n}\n'
numeric=compile_and_run(Path(__file__).with_name('ReplayReadStream.cpp'),
    'replay_read_stream.inc',source,'ReplayReadStream',extra_sources=[
        REPO/'src/GameSource/Replays/Stream/BrnReplayWriteStream_embed_check.cpp',STRSTREAM_CPP],
        extra_flags='ntdll.lib')
if numeric is None:
    raise SystemExit(1)
checks,failures=numeric
print(f'run_replay_read_stream: {checks-failures}/{checks} pass ({failures} fail)')
raise SystemExit(bool(failures))
