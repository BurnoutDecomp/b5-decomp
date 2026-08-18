// ===========================================================================
// CgsSceneManager::VolumeManager -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//   Home: GameShared/GameClasses/SceneManager/CgsVolumeManager.cpp
//
// Bodies homed here (the console's own assert file for the mutators is
// CgsVolumeManager.cpp; for GetRwVolume it is CgsVolumeManager.h, i.e. that one is a
// header inline the compiler emitted out of line as a COMDAT -- see the header banner):
//   VolumeManager::Prepare              @ 0x828CFD38 (36)
//   VolumeManager::RemoveVolume         @ 0x828CFDC8 (71)   asserts CgsVolumeManager.cpp:236
//   VolumeManager::ReplaceDynamicVolume @ 0x828CFEE8 (48)   asserts :266 / :271
//   VolumeManager::AddDynamicVolume     @ 0x828D1708 (401)  asserts :144-:149, :154
//   VolumeManager::GetRwVolume          @ 0x828C5E68 (87)   asserts CgsVolumeManager.h:182/:183
//   VolumeManager::GetVolumeIndexByID   -- header inline, no export; its shape is pinned by
//                                          the two bodies that inlined it (see below)
//   VolumeManager::Construct            -- DWARF call list (CgsVolumeManager.cpp:53)
//
// ReplaceDynamicVolume and GetRwVolume are absent from progress/identity.json (export
// holes); their disassembly was recovered with headless IDA and is saved at
// scratchpad/waveQ5/q5_out.json / q5_out2.json.
//
// ---------------------------------------------------------------------------
// CONSOLE ANCHORS used below (offsets are X360 facts, quoted as documentation only --
// every access in the code is by member name):
//   this + 0        mVolumeStore        (passed as bare `this` into the VolumeSlot pool)
//   this + 608848   mVolumePool         `addis r,this,9 ; addi r,r,0x4A50`
//   this + 791216   maVolumeHashElement `addis r,this,0xC ; addi r,r,0x12B0`, stride 24
//   this + 912368   mVolumeIdToIndex    `addis r,this,0xE ; addi r,r,-0x1410`
//   record +0x00/+0x08/+0x0C/+0x14/+0x18  the five VolumeManagerVolume fields
//
// ASSERT TEXT is the FULL console string, read out of .rdata with headless IDA on a
// private .i64 copy (IDA's inline listing truncates at 39 chars):
//   0x820F627C "Bounding box with zero width for volume instance id "
//   0x820F6244 "Bounding box with zero height for volume instance id "
//   0x820F620C "Bounding box with zero depth for volume instance id "
//   0x820F61DC "Inside out bounding box for volume instance id "
//   0x820F5C34 "Exceeded Scene Manager Volume allocation"
//   0x820F6404 "Can't find volume "            (RemoveVolume)
//   0x820F6418 "Can't find volume"             (ReplaceDynamicVolume -- no trailing space)
//   0x820F642C "Can't replace volume. Volume is not dynamic"
//   0x820F51CC "liVolumeIndex >=0 && liVolumeIndex < KI_MAX_NUM_VOLUMES"
//   0x820F519C "mVolumePool.IsObjectAllocated( liVolumeIndex )"
// The console streams the offending VolumeId onto the four "... id " messages through
// StrStream; CGS_ASSERT in this tree takes a plain string (see CgsAssert.h), so the
// message stops at the string, exactly like every other reconstructed TU.
// ===========================================================================

#include "GameShared/GameClasses/SceneManager/CgsVolumeManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Physics/CgsCollisionMeshData.h"       // CgsPhysics::CollisionMeshData
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"     // CgsResource::ResourcePtr<T>
#include "SDKs/EATech/rwcollision/volume_debug_access.h"               // rw::collision::Volume (GetBBox)
#include "rw/math/fpu/scalar_operation.h"                              // rw::math::fpu::IsZero

// rw::collision::AABBox is only NAMED here (GetBBox's out-parameter). Its real home,
// vendor/renderware/collision/AABBox.hpp, pulls the SDK rw::math::vpu::Vector3 CLASS into
// a TU that already has the vendor 4-lane POD via BrnCommonTypes.h, and the two cannot
// coexist (measured; see the include note in
// GameSource/Physics/PropManager/PropManager_wQ4_03.cpp:289-301, which solves the exact
// same problem the exact same way). What this TU needs is only AABBox's byte image: two
// 16-byte float4 rows, mMin @+0x00 and mMax @+0x10. That image is PINNED by
// GameSource/Physics/PropManager/PropManager_wQ4_03_embed_check.cpp (a mounted TU that CAN
// include AABBox.hpp and static_asserts sizeof==32 / offsetof(mMin)==0 /
// offsetof(mMax)==16), so a change to AABBox breaks that gate rather than this reader.
namespace rw { namespace collision { class AABBox; } }

namespace CgsSceneManager
{

namespace
{
    // The byte image of rw::collision::AABBox over the vendor POD Vector3 this TU speaks.
    struct alignas(16) AABBoxRows
    {
        Vector3 mMin;   // AABBox +0x00
        Vector3 mMax;   // AABBox +0x10
    };
    static_assert(sizeof(AABBoxRows) == 32, "AABBox image is two 16-byte rows");
}

// ---------------------------------------------------------------------------
// CgsVolumeManager.cpp:53 -- DWARF call list is exactly
// { VolumeStore<4608>::Construct, ObjectPool<VolumeManagerVolume,5048,int>::Construct }.
// (The console emits no out-of-line body -- it is inlined into SceneManagerModule::
// Construct -- so the DWARF call list is the whole attestation.)
// ---------------------------------------------------------------------------
void VolumeManager::Construct()
{
    mVolumeStore.Construct();
    mVolumePool.Construct();
}

// ---------------------------------------------------------------------------
// @ 0x828CFD38 (36 insns, store-for-store)
//   bl VolumeSlot,4608,int>::Clear(this)             <- mVolumeStore.Prepare()
//   bl VolumeSlot,4608,int>::Clear(this)             <- mVolumeStore.Clear()
//   bl VolumeManagerVolume,5048,int>::Clear(this+608848)  <- mVolumePool.Clear()
//   inlined bucket loop: 227 x 12 bytes at this+912368, each bin
//        { miCount = 0 ; mpBase = this+791216 ; miFirst = 0xFFFF ; miLast = 0xFFFF }
//     then `stb 1, 0xAA4(bins)` (0xAA4 == 227*12) -> mbInitialised
//   li r3,1  -> return true (no failure path; the store's bool result is ignored)
// The DWARF spells the third call ObjectPool<...>::Prepare; the X360 emits the pool's
// Clear for it, and Clear is what this tree's ObjectPool declares -- same store sequence.
// ---------------------------------------------------------------------------
bool VolumeManager::Prepare()
{
    mVolumeStore.Prepare();
    mVolumeStore.Clear();
    mVolumePool.Clear();
    mVolumeIdToIndex.Clear(maVolumeHashElement);
    return true;
}

// ---------------------------------------------------------------------------
// Header inline (no export). Pinned by the two bodies that inlined it -- RemoveVolume
// @0x828CFDE4..0x828CFDFC and ReplaceDynamicVolume @0x828CFF04..0x828CFF20 -- which both
// emit `Get(&mVolumeIdToIndex, id)` followed by a null test AND a `== -1` test on the
// value it points at. That double test is what `liIndex = GetVolumeIndexByID(id); if
// (liIndex == KI_INVALID_VOLUME_INDEX)` compiles to when the accessor is inlined. The
// DWARF lists GetVolumeIndexByID in both callers' call graphs (CgsVolumeManager.cpp:220
// and :256), immediately followed by IndexedHashTable<VolumeId,uint32_t,227u>::Get.
//
// ⚠️ SHARED-HEADER DIVERGENCE (reported, not edited -- CgsIndexedHashTable.h is not this
// cluster's file): the DWARF declares IndexedHashTable::Get as returning `const TValue*`
// (CgsIndexedHashTable.h:281/:299), which is why the console reads the index at offset 0
// of the returned pointer (`lwz r29,0(r3)`). The committed shared header instead returns
// the owning `const Element*`. Reading it through Element::GetValue() -- as below -- is
// value-identical (Get returns non-null exactly when the key is present, and GetValue()
// is the very field the console's returned pointer aims at).
// ---------------------------------------------------------------------------
s32 VolumeManager::GetVolumeIndexByID(VolumeId lVolumeId) const
{
    const CgsContainers::IndexedHashTableElement<VolumeId, u32>* lpElement =
        mVolumeIdToIndex.Get(lVolumeId);
    if (lpElement == NULL)
    {
        return KI_INVALID_VOLUME_INDEX;
    }
    return static_cast<s32>(lpElement->GetValue());
}

// ---------------------------------------------------------------------------
// @ 0x828D1708 (401 insns, store-for-store)
//
// Register map: r18 = this, r24 = lVolumeId (ONE 64-bit GPR -- Xenon GPRs are 64-bit, so
// the u64 id does not straddle a register pair), r17 = lxVolumeTypeFlags (stb, a byte),
// r19 = lpVolume.
//
// 1. Build an identity Matrix44Affine on the stack from flt_82001C98 (== 1.0f) and
//    flt_82001CC0 (== 0.0f), four rows {1,0,0,0}/{0,1,0,0}/{0,0,1,0}/{0,0,0,0} -- exactly
//    Matrix44Affine::SetIdentity(), whose wAxis is the zero row. (DWARF names the local
//    lMatrix at CgsVolumeManager.cpp:139 and lists SetIdentity.)
// 2. `lwz r7,0x40(r19) ; lwz r11,4(r7) ; mtctr ; bctrl` with r3=volume, r4=&lMatrix,
//    r5=1, r6=&lAabb -- i.e. lpVolume->GetBBox(&lMatrix, /*tight*/1, lAabb) dispatched
//    through the rwcollision per-TYPE descriptor at volume+0x40, slot +0x04. This is NOT
//    C++ virtual dispatch; see SDKs/EATech/rwcollision/volume_debug_access.h. The RwBool
//    result is not tested.
// 3. Six bounding-box tripwires, in the console's own order and on the console's own
//    lanes (measured from the vspltw lane selectors):
//      x-extent (:144), y-extent (:145), z-extent (:146) -- each
//        `vsubfp max-min ; vandc <sign bit> ; vcmpgtfp against a splat of *0x820F25D0
//         lane0 == 1.1920928955078125e-07 == FLT_EPSILON`, i.e. !IsZero(extent). The
//        epsilon is bit-identical to rw::math::fpu::KF_IS_ZERO_TOLERANCE, which is the
//        spelling the sibling CgsIntervalList.cpp already uses for the very same three
//        messages. (The DWARF names the vector form rw::math::vpu::IsZero; the scalar
//        home is the one this tree has, and it is the same comparison on one lane.)
//      inside-out on lane 0 = X (:147), lane 2 = Z (:148), lane 1 = Y (:149) -- each
//        `vcmpgefp. max,min` + CR6 "all true", i.e. min <= max. The X/Z/Y order is not a
//        transcription slip: the DWARF's call list for this function is
//        operator<=<VectorAxisX>, operator<=<VectorAxisZ>, operator<=<VectorAxisY>.
// 4. liIndex = mVolumePool.AllocateObject(); tripwire :154 when it comes back -1.
//    ⚠️ FAITHFUL, AND IT IS A CONSOLE BUG: the assert does NOT gate. `bne cr6,
//    loc_828D1CB8` only skips the assert -- there is no early return, so an exhausted pool
//    falls through and indexes the pool with -1 (whose own operator[] bounds tripwire then
//    fires). Reproduced as-is; adding a return would be new behaviour.
// 5. liStoreIndex = mVolumeStore.AddVolume(lpVolume) (inlined on the console: the slot
//    pool's AllocateObject + ReplaceVolume, -1 on either failure).
// 6. Stamp the record -- store order verbatim: mVolumeId (std +0x00), mbStaticVolume
//    (stb 0 +0x18: a dynamic volume), miVolumeStoreIndex (stw +0x14), mxFlags (stb +0x08).
//    mVolumeHandle (+0x0C) is deliberately NOT written -- it belongs to the static arm.
// 7. Publish the id: element = &maVolumeHashElement[liIndex] (`liIndex*24`), element.Set(
//    id, liIndex) (std id @+0, stw liIndex @+8), then mVolumeIdToIndex.Insert(element).
// 8. return liIndex  (`mr r3,r31`).
//
// 🔴 LIVE PRE-CONDITION FOR STEP 2 -- NOT INTRODUCED HERE, AND IT WILL CRASH IF IGNORED.
// GetBBox dispatches through the descriptor pointer the loader wrote into volume+0x40, and
// on THIS PC build that pointer is NULL for every volume: BrnPhysics::Props::
// FixableVolume::FixUp (GameShared/GameClasses/RenderWare/FixableVolume.cpp:42) stores
// `rw::collision::gVolumeVTable[type]`, and gVolumeVTable is still all-zero because
// SceneManagerModule::Construct calls the STATIC rw::collision::Volume::InitializeVTable
// (declared CgsSceneManagerModule.cpp:48, called :191) while the real body in
// SDKs/EATech/rwcollision/volume.cpp:103 is emitted NON-static -- a different mangled
// symbol -- so the boot gate at WorldLinkStubs.cpp:2461-2474 (`return 0`) wins and the
// table is never filled. AND the `static` fix alone is NOT enough: the six descriptors
// volume.cpp:106-111 would install are the placeholder symbols
// `gVolumeHandler_82F91*`, each defined as a SINGLE ZERO BYTE at
// SDKs/EATech/AptRenderLinkStubs.cpp:608-613 -- so the table would go from all-null to
// pointers-into-a-zero-byte, which still null-calls. (Wave Q2's rwvol owner found the
// mangling half -- volume_debug_access.h:296-305; wave Q5's rwcollision owner re-verified
// it and added the descriptor half -- scratchpad/waveQ5/rwcoll.owner.md s4.2.)
// Until BOTH land, the FIRST prop volume that reaches AddDynamicVolume dies here.
// Reported by the wave Q5 VolumeManager owner; NOT worked around in this body, because a
// null guard the console does not have is new behaviour and would silently swallow a real
// breakage.
// ---------------------------------------------------------------------------
s32 VolumeManager::AddDynamicVolume(VolumeId lVolumeId,
                                    VolumeManagerVolume::VolumeTypeFlags lxVolumeTypeFlags,
                                    const VolRef::Volume* lpVolume)
{
    AABBoxRows     lAabb;      // DWARF lAabb   (CgsVolumeManager.cpp:138)
    Matrix44Affine lMatrix;    // DWARF lMatrix (CgsVolumeManager.cpp:139)
    lMatrix.SetIdentity();

    // The scene manager's opaque VolRef::Volume IS rw::collision::Volume (DecFIGS
    // volume.h:39 typedefs exactly that); see the TYPE NOTE in CgsVolumeStore.h. The cast
    // is between two spellings of one type, not a layout reinterpretation.
    const rw::collision::Volume* lpRwVolume =
        reinterpret_cast<const rw::collision::Volume*>(lpVolume);
    lpRwVolume->GetBBox(&lMatrix, 1, *reinterpret_cast<rw::collision::AABBox*>(&lAabb));

    CGS_ASSERT(!rw::math::fpu::IsZero(lAabb.mMax.x - lAabb.mMin.x),
               "Bounding box with zero width for volume instance id ");
    CGS_ASSERT(!rw::math::fpu::IsZero(lAabb.mMax.y - lAabb.mMin.y),
               "Bounding box with zero height for volume instance id ");
    CGS_ASSERT(!rw::math::fpu::IsZero(lAabb.mMax.z - lAabb.mMin.z),
               "Bounding box with zero depth for volume instance id ");
    CGS_ASSERT(lAabb.mMin.x <= lAabb.mMax.x, "Inside out bounding box for volume instance id ");
    CGS_ASSERT(lAabb.mMin.z <= lAabb.mMax.z, "Inside out bounding box for volume instance id ");
    CGS_ASSERT(lAabb.mMin.y <= lAabb.mMax.y, "Inside out bounding box for volume instance id ");

    const s32 liIndex = mVolumePool.AllocateObject();          // DWARF liIndex (:153)
    CGS_ASSERT(liIndex != KI_INVALID_VOLUME_INDEX, "Exceeded Scene Manager Volume allocation");

    const s32 liStoreIndex = mVolumeStore.AddVolume(lpVolume); // DWARF liStoreIndex (:156)

    VolumeManagerVolume& lrVMVolume = mVolumePool[liIndex];    // DWARF lpVMVolume (:157)
    lrVMVolume.mVolumeId          = lVolumeId;
    lrVMVolume.mbStaticVolume     = false;
    lrVMVolume.miVolumeStoreIndex = liStoreIndex;
    lrVMVolume.mxFlags            = lxVolumeTypeFlags;

    CgsContainers::IndexedHashTableElement<VolumeId, u32>& lrElement =
        maVolumeHashElement[liIndex];
    lrElement.Set(lVolumeId, static_cast<u32>(liIndex));
    mVolumeIdToIndex.Insert(&lrElement);

    return liIndex;
}

// ---------------------------------------------------------------------------
// @ 0x828CFEE8 (48 insns, store-for-store)
//   liIndex = GetVolumeIndexByID(lVolumeId)          (inlined Get + null/-1 tests)
//   if not found            -> assert :271 "Can't find volume"
//   lpVMVolume = mVolumePool[liIndex]                 (`this+608848`, operator[])
//   if (lpVMVolume->mbStaticVolume)                   (`lbz r10,0x18(r11)`)
//                           -> assert :266 "Can't replace volume. Volume is not dynamic"
//   else mVolumeStore.ReplaceVolume(lpVMVolume->miVolumeStoreIndex, lpVolume)
//                                                     (`lwz r4,0x14(r11)`, r3 = this)
// The store's bool result is tail-returned by the console but the DWARF declares this
// method void, so it is dropped.
// ---------------------------------------------------------------------------
void VolumeManager::ReplaceDynamicVolume(VolumeId lVolumeId, const VolRef::Volume* lpVolume)
{
    const s32 liIndex = GetVolumeIndexByID(lVolumeId);         // DWARF liIndex (:256)
    if (liIndex == KI_INVALID_VOLUME_INDEX)
    {
        CGS_ASSERT(false, "Can't find volume");
        return;
    }

    VolumeManagerVolume& lrVMVolume = mVolumePool[liIndex];    // DWARF lpVMVolume (:259)
    if (lrVMVolume.mbStaticVolume)
    {
        CGS_ASSERT(false, "Can't replace volume. Volume is not dynamic");
        return;
    }

    mVolumeStore.ReplaceVolume(lrVMVolume.miVolumeStoreIndex, lpVolume);
}

// ---------------------------------------------------------------------------
// @ 0x828CFDC8 (71 insns, store-for-store)
//   liIndex = GetVolumeIndexByID(lVolumeId)
//   if not found -> assert :236 "Can't find volume " (with the id streamed on)
//   lpVMVolume = mVolumePool[liIndex]
//   if (!lpVMVolume->mbStaticVolume)                       (`lbz r10,0x18 ; bne skip`)
//        mVolumeStore.RemoveVolume(lpVMVolume->miVolumeStoreIndex)
//   mVolumePool.FreeObject(liIndex)
//   mVolumeIdToIndex.Remove(lVolumeId)
// Note the static arm frees nothing in the store (a static volume never took a store
// slot -- its image lives in the resource, reached through mVolumeHandle).
// ---------------------------------------------------------------------------
void VolumeManager::RemoveVolume(VolumeId lVolumeId)
{
    const s32 liIndex = GetVolumeIndexByID(lVolumeId);         // DWARF liIndex (:220)
    if (liIndex == KI_INVALID_VOLUME_INDEX)
    {
        CGS_ASSERT(false, "Can't find volume ");
        return;
    }

    VolumeManagerVolume& lrVMVolume = mVolumePool[liIndex];    // DWARF lpVMVolume (:223)
    if (!lrVMVolume.mbStaticVolume)
    {
        mVolumeStore.RemoveVolume(lrVMVolume.miVolumeStoreIndex);
    }

    mVolumePool.FreeObject(liIndex);
    mVolumeIdToIndex.Remove(lVolumeId);
}

// ---------------------------------------------------------------------------
// @ 0x828C5E68 (87 insns, store-for-store) -- the accessor the whole contact-generation
// middle reads volumes through.
//
//   assert (liVolumeIndex >= 0 && liVolumeIndex < 0x13B8)   [h:182, signed compares]
//   assert (mVolumePool.IsObjectAllocated(liVolumeIndex))   [h:183]
//   lpVMVolume = mVolumePool[liVolumeIndex]                 (this+608848)
//   if (lpVMVolume->mbStaticVolume)          (`lbz r11,0x18(r3) ; bne loc_828C5F48`)
//   {
//       // the STATIC arm -- the one a PROP volume takes.
//       ResourcePtr<CgsPhysics::CollisionMeshData> ptr(lpVMVolume->mVolumeHandle);
//         (the console open-codes the ctor: zero the identity words, self-link the
//          intrusive alias list at +0x0C/+0x10/+0x14, clear +0x18, then
//          CgsResource::BaseResourcePtr::CreateFromHandle(&ptr, &lpVMVolume->mVolumeHandle)
//          with `addi r4,r3,0xC` naming mVolumeHandle @+0x0C)
//       return ptr->GetVolume();
//         (`bl CgsPhysics::CollisionMeshData>::op` @0x828B7B98 == ResourcePtr<
//          CollisionMeshData>::operator->(), then `lwz r3,0(r3)` == CollisionMeshData::
//          mpCollisionVolume, the class's first member -- i.e. GetVolume(). The two
//          conditional stores that follow are the inlined ~BaseResourcePtr unlink.)
//   }
//   // the DYNAMIC arm
//   return mVolumeStore.GetVolume(lpVMVolume->miVolumeStoreIndex);
//     (`lwz r31,0x14(r3)` then the inlined store lookup -- see VolumeStore<N>::GetVolume)
//
// ⚠️ TYPE-FORK CAST, REPORTED. CollisionMeshData::GetVolume() returns the GLOBAL
// ::VolRef::Volume* (CgsCollisionMeshData.h:16) while this subsystem's headers declare the
// CgsSceneManager-scoped VolRef::Volume (CgsVolumeStore.h, CgsOverlapCullingModule.h:57).
// They are two opaque forward declarations of ONE type (rw::collision::Volume, DecFIGS
// volume.h:39), so the cast reinterprets nothing -- but the fork is real and belongs to
// whoever owns CgsCollisionMeshData.h / CgsOverlapCullingModule.h; it is reported by the
// wave Q5 VolumeManager owner rather than unilaterally unified across three owners.
// ---------------------------------------------------------------------------
const VolRef::Volume* VolumeManager::GetRwVolume(s32 liVolumeIndex) const
{
    CGS_ASSERT(liVolumeIndex >= 0 && liVolumeIndex < KI_MAX_NUM_VOLUMES,
               "liVolumeIndex >=0 && liVolumeIndex < KI_MAX_NUM_VOLUMES");
    CGS_ASSERT(mVolumePool.IsObjectAllocated(liVolumeIndex),
               "mVolumePool.IsObjectAllocated( liVolumeIndex )");

    const VolumeManagerVolume& lrVMVolume = mVolumePool[liVolumeIndex];

    if (lrVMVolume.mbStaticVolume)
    {
        CgsResource::ResourcePtr<CgsPhysics::CollisionMeshData>
            lCollisionMesh(lrVMVolume.mVolumeHandle);
        return reinterpret_cast<const VolRef::Volume*>(lCollisionMesh->GetVolume());
    }

    return mVolumeStore.GetVolume(lrVMVolume.miVolumeStoreIndex);
}

}
