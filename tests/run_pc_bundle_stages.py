"""Production bundle stages/queues with bounded file-reader and compression boundaries."""
import argparse
from pathlib import Path
from fxgs_common import REPO, Tree, definition, compile_and_run, report, STRSTREAM_CPP

parser=argparse.ArgumentParser()
parser.add_argument('--old-header',action='store_true')
parser.add_argument('--reuse-live-update',action='store_true',
    help='negative control: restore resident reuse for live replacement requests')
parser.add_argument('--original-cache-lookup',action='store_true',
    help='negative control: restore the untagged ARTIST resident lookup')
args=parser.parse_args()
tree=Tree()
base='src/GameShared/GameClasses/'
resource=base+'System/Resource/'
stages=tree.read(resource+'CgsBundleLoaderModule.cpp')
if args.old_header:
    signature='bool BundleLoaderModule::StreamHeaderFunc()'
    stages=stages.replace(definition(stages,signature),
        definition(Tree('087d7dae').read(resource+'CgsBundleLoaderModule.cpp'),signature))
if args.reuse_live_update:
    guard='        if (lRequest.mbLiveUpdateReplace)\n            break;\n'
    assert stages.count(guard)==1
    stages=stages.replace(guard,'')
if args.original_cache_lookup:
    lookup=definition(stages,'bool BundleLoaderModule::CheckForLoads(')
    marker='\n                    | 0x8000000000000000ull'
    assert lookup.count(marker)==1
    stages=stages.replace(lookup,lookup.replace(marker,''))
driver=tree.read(resource+'CgsResourceBundleLoaderModule.cpp')
adapter='namespace CgsResource {\n'+definition(driver,'void BundleLoaderModule::Construct()')+'\n'
adapter+=definition(driver,'bool BundleLoaderModule::Update(')+'\n}\n'
hash_code='#include "GameShared/GameClasses/System/Resource/CgsResourceID.h"\nnamespace CgsResource {\n'
hash_code='#include "GameShared/GameClasses/Core/CgsAssert.h"\n'+hash_code
hash_code+=definition(tree.read(resource+'CgsResourceID.cpp'),'s32 ID::HashString(')+'\n}\n'
# Module base lifecycle and compression are outside this uncompressed protocol
# test. Compression aborts, so accidentally selecting it cannot fake a pass.
shadow={
 'src/GameShared/GameClasses/Module/CgsModuleSingleBuffered.h':
    '#pragma once\nnamespace CgsModule { struct ModuleSingleBuffered { bool mbIsNewModule=false; void Construct(){} }; }\n',
 'src/GameShared/Jobs/DecompressionJob/DecompressionJobInterface.h':
    '#pragma once\n#include <cstdlib>\n#include "types.hpp"\nnamespace CgsResource { struct DecompressionJobInterface {\n'
    'void BeginStream(){std::abort();} void CreateEntry(void*,u32){std::abort();}\n}; }\n'
}
numeric=compile_and_run(Path(__file__).with_name('PCBundleStages.cpp'),
    'pc_bundle_stages.inc',adapter,'PCBundleStages',shadow=shadow,
    extra_files={'pc_bundle_stages.cpp':stages,'pc_bundle_hash.cpp':hash_code},
    extra_sources=['pc_bundle_stages.cpp','pc_bundle_hash.cpp',STRSTREAM_CPP,
        *[REPO/base/'Module'/name for name in ['CgsIOBuffer.cpp','CgsBaseEventReceiverQueue.cpp']],
        REPO/base/'Containers/CgsPriorityQueue.cpp',
        *[REPO/resource/name for name in ['CgsResourceBundle2.cpp','CgsResourceIOEvents.cpp',
         'CgsBundleLoaderModuleIO_InputBuffer.cpp','CgsBundleLoaderModuleIO_InputBuffer_Update.cpp',
         'CgsBundleLoaderModuleIO_InputBuffer_Record.cpp','CgsBundleLoaderModuleIO_OutputBuffer.cpp']]])
raise SystemExit(report('run_pc_bundle_stages',[],numeric,31))
