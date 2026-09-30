"""Real pool moves/planners/jobs; negative controls restore the original faulty bodies."""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, definition, compile_and_run, report, STRSTREAM_CPP

parser=argparse.ArgumentParser()
parser.add_argument('--old-rebase',action='store_true')
parser.add_argument('--old-planner',action='store_true')
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
       'GetResource','GetAllowDefragmentation','GenerateLinearHeap','ExecuteBatchAllocation',
       'ExecuteBatchAddressedAllocation','BeginEmergencyDefragmentation',
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
planner=Tree('9878a9e5' if args.old_planner else None).read(resource+'PoolModuleStates/CgsBaseDefragPoolModuleState.cpp')
jobs=tree.read('src/SDKs/EATech/eajobs/job.cpp')
job_code='#include "SDKs/EATech/eajobs/job.h"\n#include <cstring>\nnamespace EA { namespace Jobs {\n'
for signature in ['static u64 PackHandleQword(', 'static EntryPoint MakeDefaultEntryPoint(',
                  'void Job::Clear(', 'Job::Job(const char* lpcName)', 'Job::~Job(', 'void Job::SetData(',
                  'bool Job::IsDone(', 'void Job::WaitOn(', 'void Job::SetCode(', 'void Job::SetName(']:
    job_code+=definition(jobs,signature)+'\n'
# Retain the synchronous fixture's allocator boundary.
# Real dependency buckets remain empty in this direct-dispatch integration.
leaves=tree.read('src/SDKs/EATech/AptRenderLinkStubs.cpp')
job_code+='\n} namespace Allocator {\n'
job_code+=definition(leaves,'ICoreAllocator* ICoreAllocator::GetDefaultAllocator()')+'\n} }\n'
numeric=compile_and_run(Path(__file__).with_name('PCResourceRelocation.cpp'),
    'pc_resource_relocation.inc',code,'PCResourceRelocation',extra_flags='/Gy',
    extra_files={'pc_resource_heap.inc':tree.read(resource+'CgsResourceHeap.cpp'),
                 'pc_resource_type_base.cpp':types,'pc_defrag_base.cpp':planner,'pc_relocator_jobs.cpp':job_code},
    extra_sources=['pc_resource_type_base.cpp','pc_defrag_base.cpp','pc_relocator_jobs.cpp', STRSTREAM_CPP,
        *[REPO/resource/'PoolModuleStates'/name for name in ['CgsIntelliFragPoolModuleState.cpp',
             'CgsEmergencyFragPoolModuleState.cpp']],
        *[REPO/'src/GameShared/Jobs/Relocator'/name for name in ['CgsRelocator.cpp','Relocator.cpp','RelocatorJob.cpp']],
        *[REPO/'src/SDKs/EATech/eajobs'/name for name in ['entrypoint.cpp','bucket_list_node.cpp','event.cpp','jobs.cpp']],
        *[REPO/resource/name for name in ['CgsResourceScratchPool.cpp','CgsResourceImportHashTable.cpp',
             'CgsSmallResource.cpp','CgsBaseResourcePtr.cpp','CgsResourcePtr.cpp']],
        *[REPO/base/'Memory'/name for name in ['CgsLinearMalloc.cpp','CgsDistributionStream.cpp',
             'CgsGatherStream.cpp','CgsScatterStream.cpp']],
        REPO/'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_resource_relocation',[],numeric,30))
