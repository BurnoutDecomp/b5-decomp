"""Production allocation state + pool batches over the real heap allocator.

--old-free reinstates the old empty pointer-free body as a negative control.
--old-records reinstates the console-width native pool request/response views.
"""
import argparse
from pathlib import Path
from fxgs_common import Tree, REPO, STRSTREAM_CPP, definition, compile_and_run, report

parser = argparse.ArgumentParser()
parser.add_argument('--old-free', action='store_true')
parser.add_argument('--old-records', action='store_true')
parser.add_argument('--old-init', action='store_true', help='restore the missing idle initialization')
args = parser.parse_args()
base = 'src/GameShared/GameClasses/System/Resource/'
tree = Tree()
pool = tree.read(base + 'CgsResourcePool.cpp')
names = ['Construct', 'InitPool', 'InitManagementData',
         'GetId', 'GetName', 'GetHeapAlignment', 'GetNumEntriesInPurgatory',
         'GetEntryRefCount', 'SetEntryRefCount', 'IncEntryRefCount', 'DecEntryRefCount',
         'GetRefCountThreshold', 'IsDefragmenting', 'FixUpEntry', 'PostFixUpEntry', 'ResolveImportForEntry',
         'ResolveImportsForEntry', 'FixUpAndResolveResourceList',
         'SetEntryStatus', 'SetEntryImportCount', 'FindResourceIndex', 'FindResource',
         'FindResourceWithDependencies', 'FindResourceIndexWithDependencies',
         'AllocateResourceEntry', 'CreateEntry', 'CreateEntryInSlot', 'AllocateMemoryForResource',
         'FreeResourceEntry', 'FreeMemoryForResource', 'RemoveReference',
         'DeleteMemoryForEntry', 'ReAllocateMemoryForEntry', 'DeleteEntry',
         'CreateBatchEntrySlots', 'BuildAllocRequestForEntry', 'ExecuteBatchAllocation',
         'ExecuteBatchAllocations', 'MergeBatchAllocations']
code = 'namespace CgsResource {\n' + definition(pool, 'u32 GetManagementHashLength(') + '\n'
for name in names:
    signature = next(line.strip() for line in pool.splitlines()
                     if f'Pool::{name}(' in line and not line.strip().startswith('//'))
    source = Tree('64f9b755').read(base + 'CgsResourcePool.cpp') if args.old_init and name == 'InitPool' else pool
    code += definition(source, signature) + '\n'
code += definition(pool, 'void AllocListSet::ClearCountsAndResults(') + '\n'
code += definition(tree.read(base + 'CgsResourceID.cpp'), 's32 ID::HashString(') + '\n'
bundle = tree.read(base + 'CgsResourceBundle2.cpp')
for name in ['GetUncompressedSize', 'GetUncompresssedAlignment']:
    code += definition(bundle, f'u32 BundleV2::ResourceEntry::{name}(') + '\n'
state = tree.read(base + 'PoolModuleStates/CgsAllocatePoolModuleState.cpp')
for name in ['Construct', 'BeginAllocation', 'GenerateResponse', 'Update', 'DebugPrintAllocListSet',
             'CheckListDependencies', 'CheckEntryListDependency', 'CreateResourceList',
             'CreateEntryListResource', 'UndoEntryCreations']:
    signature = next(line.strip() for line in state.splitlines()
                     if f'AllocatePoolModuleState::{name}(' in line and not line.strip().startswith('//'))
    code += definition(state, signature) + '\n'
deallocate = tree.read(base + 'PoolModuleStates/CgsDeAllocatePoolModuleState.cpp')
for name in ['Construct', 'Begin', 'Update']:
    signature = next(line.strip() for line in deallocate.splitlines()
                     if f'DeAllocatePoolModuleState::{name}(' in line and not line.strip().startswith('//'))
    code += definition(deallocate, signature) + '\n'
driver = tree.read(base + 'CgsPoolModule.cpp')
for name in ['ConvertPoolRequestOptions', 'DoDeletePoolRequest',
             'DoFixUpAndResolveResourceListRequest', 'DoUnloadResourceListRequest']:
    source = Tree('afcb3588').read(base + 'CgsPoolModule.cpp') if args.old_records and name in [
        'ConvertPoolRequestOptions', 'DoDeletePoolRequest'] else driver
    code += definition(source, f'void PoolModule::{name}(') + '\n'
code += '}\n'
heap = tree.read(base + 'CgsResourceHeap.cpp')
if args.old_free:
    heap = heap.replace(definition(heap, 'void Heap::Free(void* lpPtr)'),
        definition(Tree('15ec7dc1').read(base + 'CgsResourceHeap.cpp'), 'void Heap::Free(void* /*lpPtr*/)'))
numeric = compile_and_run(Path(__file__).with_name('PCResourceBatch.cpp'), 'pc_resource_batch.inc', code,
    'PCResourceBatch', extra_files={'pc_resource_heap.inc':heap}, extra_sources=[STRSTREAM_CPP,
        REPO / 'src/GameShared/GameClasses/Memory/CgsLinearMalloc.cpp',
        REPO / base / 'CgsResourceTypeBase.cpp', REPO / base / 'CgsEntryListResource.cpp',
        REPO / base / 'CgsBaseResourcePtr.cpp', REPO / base / 'CgsResourcePtr.cpp',
        REPO / base / 'CgsPoolModuleIO_OutputBuffer.cpp',
        REPO / 'src/GameShared/GameClasses/Module/CgsIOBuffer.cpp',
        REPO / base / 'CgsSmallResource.cpp', REPO / 'vendor/renderware/src/rw/BaseResourceDescriptor.cpp'])
raise SystemExit(report('run_pc_resource_batch', [], numeric, 59))
