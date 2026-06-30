// Per-instantiation .cpp for the BYTE-INDEXED IndexedPool family (IndexType=s8). The IDA-
// truncated ledger key "char>" is the pool's second template tail -- the free-list / count
// index type is `char` (signed byte: the X360 reads the count fields with `lbz` and sign-extends
// with `extsb`, and the free-index list is a byte array). Two distinct using-TUs each emit one
// out-of-line copy of this generic, with two distinct element types T (and thus two sizeof
// strides), but the SAME byte index type -- so this single ledger key covers both:
//
//   Pool A -- CgsResource::ResourceModule (element stride 0x0C = 12 bytes):
//     IndexedPool<ResourcePoolRecord,N,s8>::Clear @ 0x828EB2B8  (ResourceModule::Construct)
//     IndexedPool<ResourcePoolRecord,N,s8>::Get   @ 0x828DFD38  (ProcessPendingFileSystemResponses;
//                                                    fills outBuffer[i]=&maElements[i] @ stride 12,
//                                                    nulls free slots, compacts, returns live count)
//   Pool B -- CgsFileSystem::PhysicalX360Device (element stride 0x110 = 272 bytes):
//     IndexedPool<FileHandleRecord,N,s8>::GetObjectIndex @ 0x828EAE38  (Close/CloseDirectory;
//                                                    (lpObject - maElements) -- divw by 272)
//     IndexedPool<FileHandleRecord,N,s8>::Push           @ 0x828EB138  (ProcessPendingFileSystemResponses)
//     IndexedPool<FileHandleRecord,N,s8>::PushIndex      @ 0x828EAEF8  (Close/CloseDirectory)
//
// The element record types are DEFERRED (reconstructed with their owning modules). They are
// modeled here as honest fixed-size layout placeholders pinned to the exact X360 strides the asm
// divides/indexes by (12 and 272), so the sizeof-dependent bodies (Get's &maElements[i], the
// GetObjectIndex pointer subtraction) compile byte-faithfully. GROW these into the real records
// when ResourceModule / PhysicalX360Device land; do NOT fork the pool. N is a runtime field
// (msCapacity); the literal extents below are placeholders (the bodies are N-independent).
#include "GameShared/GameClasses/Containers/CgsIndexedPool.h"

namespace CgsResource
{
    // 12-byte ResourceModule pool record (X360 Get stride 0x0C). Deferred; honest layout placeholder.
    struct ResourcePoolRecord
    {
        u8 maPad00[12];
    };
}

// The 272-byte PhysicalX360Device handle record (X360 GetObjectIndex stride 0x110) is now
// homed with named fields by its owning module — see Devices/CgsDevicePhysicalX360.h. Reuse it
// BY NAME so this pool instantiation keeps the exact 0x110 stride the GetObjectIndex asm divides
// by (no fork; the include pins the same sizeof).
#include "GameShared/GameClasses/System/FileSystem/Devices/CgsDevicePhysicalX360.h"

// Pool A (12-byte element, byte index): Clear + Get.
template void CgsContainers::IndexedPool<CgsResource::ResourcePoolRecord, 256, s8>::Clear();
template s32  CgsContainers::IndexedPool<CgsResource::ResourcePoolRecord, 256, s8>::Get(
                  CgsResource::ResourcePoolRecord**, s32) const;

// Pool B (272-byte element, byte index): GetObjectIndex + Push + PushIndex.
template s8   CgsContainers::IndexedPool<CgsFileSystem::FileHandleRecord, 256, s8>::GetObjectIndex(
                  const CgsFileSystem::FileHandleRecord*) const;
template void CgsContainers::IndexedPool<CgsFileSystem::FileHandleRecord, 256, s8>::Push(s8);
template void CgsContainers::IndexedPool<CgsFileSystem::FileHandleRecord, 256, s8>::PushIndex(s8);
