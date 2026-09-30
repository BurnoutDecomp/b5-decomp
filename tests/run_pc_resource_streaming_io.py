"""Native streaming queues and production allocation-request decoding.

--old-driver runs the pre-fix 32-bit-offset decoder against the same native records.
"""
import argparse
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report, STRSTREAM_CPP

parser = argparse.ArgumentParser()
parser.add_argument('--old-driver', action='store_true')
args = parser.parse_args()
base = 'src/GameShared/GameClasses/System/Resource/'
tree = Tree('15ec7dc1' if args.old_driver else None)
body = definition(tree.read(base + 'CgsPoolModule.cpp'), 'void PoolModule::DoAllocateResourceListRequest(')
body = body.replace('PoolModule::DoAllocateResourceListRequest', 'RequestDecoder::DoAllocateResourceListRequest')
numeric = compile_and_run(Path(__file__).with_name('PCResourceStreamingIO.cpp'),
    'pc_resource_streaming_io.inc', body, 'PCResourceStreamingIO',
    extra_sources=[STRSTREAM_CPP, REPO / 'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',
        *[REPO / base / f'CgsBundleLoaderModuleIO_{name}.cpp' for name in
          ['InputBuffer', 'InputBuffer_Update', 'InputBuffer_Record', 'OutputBuffer']]])
raise SystemExit(report('run_pc_resource_streaming_io', [], numeric, 14))
