"""Production lookahead decoder against sorted records and a guarded key boundary."""
from pathlib import Path
import argparse
import os
from fxgs_common import Tree, definition, compile_and_run, report

os.environ.pop('NoDefaultCurrentDirectoryInExePath', None)
parser = argparse.ArgumentParser()
parser.add_argument('--read-past-end', action='store_true')
parser.add_argument('--truncate-pointer', action='store_true')
args = parser.parse_args()
source = Tree().read('src/GameShared/GameClasses/Graphics/Dispatch/CgsDispatcherCommands.cpp')
signatures = ['inline void* ReadCommandPointer(', 'inline u32 CommandIdOf(',
              'inline u32* PacketFromRecord(', 'inline void PrefetchDispatchSpanPC(',
              'inline void PrefetchMeshCommandsPC(']
body = '\n'.join(definition(source, signature) for signature in signatures)
if args.read_past_end:
    assert body.count('luEnd - luIndex <= 2u') == 1
    body = body.replace('luEnd - luIndex <= 2u', 'luEnd - luIndex <= 1u')
if args.truncate_pointer:
    assert body.count('static_cast<uintptr_t>(lu64)') == 1
    body = body.replace('static_cast<uintptr_t>(lu64)', 'static_cast<u32>(lu64)')
result = compile_and_run(Path(__file__).with_name('PCMeshPrefetch.cpp'),
                         'pc_mesh_prefetch.inc', 'namespace CgsGraphics {\n' + body + '\n}',
                         'PCMeshPrefetch')
raise SystemExit(report('run_pc_mesh_prefetch', [], result, 9))
