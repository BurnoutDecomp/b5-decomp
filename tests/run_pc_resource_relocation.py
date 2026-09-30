"""Actual silent pool relocation and purgatory; --old-rebase restores the old empty callback."""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, definition, compile_and_run, report, STRSTREAM_CPP

parser=argparse.ArgumentParser()
parser.add_argument('--old-rebase',action='store_true')
args=parser.parse_args()
base='src/GameShared/GameClasses/'
resource=base+'System/Resource/'
tree=Tree()
pool=tree.read(resource+'CgsResourcePool.cpp')
code='namespace CgsResource { namespace {\nbool sabLoggedPoolFull[64] = {};\ns32 siDefragDebugFrame=0;\n'
for signature in ['u32 GetManagementHashLength(', 's32 GetRelocationRWMemoryType(',
                  'void RetireRelocatedResourcePC(', 'void WriteScratchImportPC(']:
    code+=definition(pool,signature)+'\n'
code+='}\n'
names=['Construct','InitPool','InitManagementData','GetId','GetName','GetHeapAlignment','IsDefragmenting',
       'GetEntryRefCount','SetEntryRefCount','GetNumEntriesInPurgatory','GetEntryStatus','SetEntryStatus',
       'FindResourceIndex','FindResource','FindResourceWithDependencies','FindResourceIndexWithDependencies',
       'AllocateResourceEntry','CreateEntry','CreateEntryInSlot','AllocateMemoryForResource',
       'FreeResourceEntry','FreeMemoryForResource','DeleteEntry','ResolveImportForEntry','ResolveImportsForEntry',
       'ResolveAllResources','BeginDefragmentation','Update','UpdateDefrag','UpdateEmergencyDefrag',
       'BeginDefragNextSetOfRelocations','AddResourceToScratchPool','AddResourcesToScratchPool',
       'RebaseResourceToScratchPool','RebaseResourceFromScratchPool','RebaseResourcesToScratchPool',
       'RebaseResourcesFromScratchPool','ResolveAllTempScratchResources','ResolveAllDestScratchResources']
for name in names:
    signature=next(line.strip() for line in pool.splitlines()
                   if f'Pool::{name}(' in line and not line.strip().startswith('//'))
    code+=definition(pool,signature)+'\n'
code+='}\n'
types=Tree('e2d854af' if args.old_rebase else None).read(resource+'CgsResourceTypeBase.cpp')
numeric=compile_and_run(Path(__file__).with_name('PCResourceRelocation.cpp'),
    'pc_resource_relocation.inc',code,'PCResourceRelocation',
    extra_files={'pc_resource_heap.inc':tree.read(resource+'CgsResourceHeap.cpp'),'pc_resource_type_base.cpp':types},
    extra_sources=['pc_resource_type_base.cpp', STRSTREAM_CPP,
        *[REPO/resource/name for name in ['CgsResourceScratchPool.cpp','CgsResourceImportHashTable.cpp',
             'CgsSmallResource.cpp','CgsBaseResourcePtr.cpp','CgsResourcePtr.cpp']],
        *[REPO/base/'Memory'/name for name in ['CgsLinearMalloc.cpp','CgsDistributionStream.cpp',
             'CgsGatherStream.cpp','CgsScatterStream.cpp']],
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_resource_relocation',[],numeric,17))
