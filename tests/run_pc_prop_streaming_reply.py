"""Exercise the production prop completion loop and its existing retry producer.

--rev <pre-fix revision> runs the same cases against the unpatched body.
The fixture uses the real native ResourcePtr, handle, entry and prop-zone record.
"""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, STRSTREAM_CPP, definition, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--rev')
args = parser.parse_args()
tree = Tree(args.rev)
base = 'src/GameSource/World/EntityModules/PropEntityModule/'
source = tree.read(base + 'BrnPropEntityModule_PreScene.cpp')
marker = '        {\n            const CgsModule::Event* lpEventData = 0;'
start = source.index(marker, source.index('// [11]'))
loop = definition(source[start:], marker)
code = 'void PropEntityModule::Drain(PropEntityIO::OutputBuffer_PreScene* lpOutput) {\n'
code += loop + '\nmReceiverQueue.Clear();\n}\n'
code += definition(tree.read(base + 'BrnPropEntityModule_Streaming.cpp'),
                   'bool RequestPropInstancesForZone(')
resource = 'src/GameShared/GameClasses/System/Resource/'
numeric = compile_and_run(Path(__file__).with_name('PCPropStreamingReply.cpp'),
    'pc_prop_streaming_reply.inc', code, 'PCPropStreamingReply', extra_flags='/EHsc',
    extra_sources=[STRSTREAM_CPP, REPO / 'src/GameShared/GameClasses/Core/CgsID.cpp',
        REPO / 'src/GameShared/GameClasses/Core/CgsStringUtils.cpp',
        REPO / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp',
        *[REPO / resource / name for name in [
            'CgsBaseResourcePtr.cpp', 'CgsResourcePtr.cpp', 'CgsResourceHandle.cpp']]])
raise SystemExit(report('run_pc_prop_streaming_reply', [], numeric, 14))
