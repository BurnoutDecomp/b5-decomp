#pragma once

// ===========================================================================
// CgsSceneManager::VolumeStore<N> + its element type VolumeSlot
//   Home: GameShared/GameClasses/SceneManager/CgsVolumeStore.h
//
// The scene manager's DYNAMIC collision-volume store: every dynamic volume the
// scene is told about is COPIED, in place, into a fixed 128-byte VolumeSlot
// (KI_VOLUME_SLOT_SIZE), and 4608 of those slots are pooled by
// ObjectPool<VolumeSlot,4608,int>. A store index is what
// VolumeManagerVolume::miVolumeStoreIndex holds.
//
// DWARF authority (references/DecFIGS/dwarfdump/.../CgsVolumeStore.h):
//   KI_VOLUME_SLOT_SIZE       = 128        (CgsVolumeStore.h:33)
//   KI_INVALID_VOLUME_INDEX   = 4294967295 (CgsVolumeStore.h:34, int32_t = 0xFFFFFFFF)
//   struct VolumeSlot { private: uint8_t macBuffer[128];
//                       void SetVolume(const VolRef::Volume*, int32_t);   (h:56)
//                       VolRef::Volume* GetVolume() const;   }            (h:59/64)
//   VolumeStore<4608> { private: ObjectPool<VolumeSlot,4608,int32_t> mVolumePool; (h:126)
//                       Construct(h:172) Destruct(h:190) Prepare(h:208) Clear(h:227)
//                       Release(h:246) AddVolume(h:267) RemoveVolume(h:295)
//                       ReplaceVolume(h:318) GetVolumeResourceDescriptor(h:350)
//                       IsPrimitiveVolume(h:386) GetVolume(h:421) }
//
// X360 ARTIST bodies homed in CgsVolumeStore.cpp:
//   VolumeStore<4608>::RemoveVolume                @ 0x828C4108 (36)
//   VolumeStore<4608>::GetVolumeResourceDescriptor @ 0x828C5A98
//   VolumeStore<4608>::ReplaceVolume               @ 0x828CC1D8 (74)
//   VolumeStore<4608>::{Construct,Prepare,Clear,AddVolume,GetVolume}
//        -- header-inline on the console (no out-of-line export); their exact shape is
//           recovered from the sites that inlined them, see CgsVolumeStore.cpp.
//   ObjectPool<VolumeSlot,4608,int>::Clear             @ 0x828B9218
//                                    ::FreeObject      @ 0x828B9030
//                                    ::IsObjectAllocated @0x828B9540
//                                    ::operator[] const  @0x828B9280 (asserts h:228/229)
//                                    ::operator[]        @0x828B93E0 (asserts h:218/219)
//                                    ::AllocateObject    @0x828B8EA0 (export hole)
//
// ---------------------------------------------------------------------------
// ⭐ 16-BYTE ALIGNMENT OF VolumeSlot -- DERIVED, AND IT IS LOAD-BEARING.
// The slot holds a *serialised rw::collision::Volume image in place*, and the store's own
// GetVolumeResourceDescriptor hands that image out with alignment 0x10 (see the .cpp).
// The console layout independently forces the same conclusion, twice:
//   * VolumeManager embeds { VolumeStore<4608> mVolumeStore; ObjectPool<VolumeManagerVolume,
//     5048,int> mVolumePool; ... } and the X360 puts mVolumePool at +608848
//     (GetRwVolume @0x828C5E68: `addis r30,r28,9 ; addi r30,r30,0x4A50`), while the pool
//     members attested inside VolumeStore end at 608840 (maiObjectFreeQueue @+589824,
//     miNumObjectsFree @+608256, BitArray<4608> @+608264 + 576 bytes). The 8-byte tail gap
//     is exactly a round-up of 608840 to a multiple of 16.
//   * sizeof(VolumeManager) == 915104 on the console (SceneManagerModule mVolumeManager
//     @+0x2C7800, mCullingGroupManager @+0x3A6EA0), while its last attested member ends at
//     915093 -> 915096 under 8-byte alignment. 915104 is 16*57194; 915096 is not.
// Both facts fall out of `alignas(16)` on VolumeSlot and nothing else, so it is declared
// here and PINNED by the static_asserts below (this whole sub-tree is pointer-free, so the
// console byte sizes survive the x64 compile exactly -- the rare case where a console
// literal IS a host value, and it is asserted rather than assumed).
// ---------------------------------------------------------------------------
//
// TYPE NOTE -- `VolRef::Volume`. The DWARF spells the collision-volume record
// `VolRef::Volume` (DecFIGS volume.h:39 is `typedef rw::collision::Volume Volume`), i.e. it
// IS rw::collision::Volume, the 96-byte serialised record whose real home in this tree is
// SDKs/EATech/rwcollision/volume_debug_access.h. It is kept as an opaque forward
// declaration HERE (pointer-only use, AGENTS forward-declaration exception (b)) because
// this header is pulled all the way up into CgsSceneManagerModule.h -> BrnWorldModule.h,
// and volume_debug_access.h's transitive rw-math vocabulary cannot be dragged along that
// path. The .cpp that actually needs the complete type includes the real home.
// ⚠️ REPORTED FORK (not introduced here, not resolvable from this file): the tree carries
// THREE separately-scoped opaque declarations of this one type --
// CgsSceneManager::VolRef::Volume (here and in CgsOverlapCullingModule.h:57) and the
// GLOBAL ::VolRef::Volume (CgsCollisionMeshData.h:16). They are distinct C++ types, so a
// value crossing between them needs a cast (CgsVolumeManager.cpp does exactly one, at the
// CollisionMeshData seam, and flags it). Unifying them is a cross-owner edit.
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"          // CgsContainers::ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/System/Resource/CgsResourceType.h"   // CgsResource::ResourceDescriptor (rw::BaseResourceDescriptors<5>)

namespace CgsSceneManager
{
    // The serialised rwcollision volume record. See the TYPE NOTE in the banner.
    namespace VolRef { struct Volume; }

    // DWARF CgsVolumeStore.h:33/34. KI_INVALID_VOLUME_INDEX is an int32_t whose bit
    // pattern is 0xFFFFFFFF (== -1); kept as the DWARF name and type. It is the "no
    // volume" sentinel for BOTH index spaces this subsystem uses (a VolumeStore slot
    // index and a VolumeManager pool index); the X360 compares against literal -1 in
    // both (VolumeManager::RemoveVolume @0x828CFDC8 `cmpwi r29,-1`).
    static const s32 KI_VOLUME_SLOT_SIZE     = 128;
    static const s32 KI_INVALID_VOLUME_INDEX = static_cast<s32>(0xFFFFFFFF);

    // One pooled collision-volume slot: a 128-byte buffer that holds a serialised
    // VolRef::Volume in place (DWARF CgsVolumeStore.h:51/64). The buffer sizes the
    // ObjectPool stride (589824 / 4608 == 128 on the X360). 16-byte aligned -- see the
    // alignment block in the banner.
    struct alignas(16) VolumeSlot
    {
        // CgsVolumeStore.h:56 -- copy a serialised volume of liSizeInBytes bytes into
        // macBuffer. Inlined by the console into VolumeStore<4608>::ReplaceVolume
        // @0x828CC1D8, which is where its size tripwire is baked; de-inlined back to its
        // DWARF-declared home here.
        void SetVolume(const VolRef::Volume* lpVolume, s32 liSizeInBytes);

        // CgsVolumeStore.h:59 -- view macBuffer as the volume it holds. The console's
        // inlined form is a bare `slot address` (the buffer is the slot's only member, at
        // offset 0): VolumeManager::GetRwVolume @0x828C5E68 tail-calls
        // ObjectPool<VolumeSlot,4608,int>::operator[] @0x828B9280 and returns its result
        // unchanged. DWARF declares it const-qualified but returning a NON-const Volume*.
        VolRef::Volume* GetVolume() const;

    private:
        u8 macBuffer[KI_VOLUME_SLOT_SIZE];   // CgsVolumeStore.h:64 (+0x00)
    };

    // VolumeStore<N> -- owns the pool of N collision-volume slots. Method bodies live in
    // CgsVolumeStore.cpp with an explicit instantiation for N == 4608 (the only one the
    // X360 build emits).
    template <s32 tiCapacity>
    class VolumeStore
    {
    public:
        // h:172 -- lifecycle-Construct: clear the pool's occupancy bits only (no free-queue
        // refill; Prepare does that). DWARF VolumeManager::Construct's call list is
        // {VolumeStore<4608>::Construct, ObjectPool<VolumeManagerVolume,5048,int>::Construct}.
        void Construct();
        // h:190 -- declaration-only: the console emits no body and nothing in the tree calls
        // it (its DWARF twin is ObjectPool::Destruct, which this tree's ObjectPool does not
        // declare). Left undefined deliberately rather than invented.
        void Destruct();
        // h:208 / h:227 -- both reduce to the pool's Clear() on the X360 (VolumeManager::
        // Prepare @0x828CFD38 emits ObjectPool<VolumeSlot,4608,int>::Clear TWICE, back to
        // back, for exactly this pair).
        bool Prepare();
        void Clear();
        // h:246 -- declaration-only (no X360 body, no caller). See Destruct.
        bool Release();

        // h:267 -- take a free slot and copy lpVolume into it; returns the slot index, or
        // KI_INVALID_VOLUME_INDEX when the pool is full or the copy is refused.
        s32  AddVolume(const VolRef::Volume* lpVolume);
        // h:295 @0x828C4108 -- free the pooled slot at liVolumeIndex.
        void RemoveVolume(s32 liVolumeIndex);
        // h:318 @0x828CC1D8 -- overwrite the pooled slot at liVolumeIndex with lpVolume.
        bool ReplaceVolume(s32 liVolumeIndex, const VolRef::Volume* lpVolume);
        // h:421 -- the volume held by the slot at liVolumeIndex, or NULL when the index is
        // out of range / the slot is not allocated.
        const VolRef::Volume* GetVolume(s32 liVolumeIndex) const;

    private:
        // CgsVolumeStore.h:350 -- size the serialised descriptor for a volume.
        // DWARF-attested NON-const (no trailing const in the dump).
        CgsResource::ResourceDescriptor GetVolumeResourceDescriptor(const VolRef::Volume* lpVolume);
        // CgsVolumeStore.h:386 -- true when lpVolume is one of the primitive types.
        bool IsPrimitiveVolume(const VolRef::Volume* lpVolume) const;

        CgsContainers::ObjectPool<VolumeSlot, tiCapacity, s32> mVolumePool;   // CgsVolumeStore.h:126
    };

    // ---- CONSOLE-SIZE PINS ----------------------------------------------------------
    // This whole sub-tree is POINTER-FREE (a byte buffer, an s32 free queue, an s32
    // counter and a u64 BitArray), so the console byte sizes survive the x64 compile
    // unchanged and can legitimately be asserted rather than merely commented:
    //   maObjectPool                    [0, 589824)      4608 * 128
    //   maiObjectFreeQueue              +589824          4608 * 4      -> 608256
    //   miNumObjectsFree                +608256          4             -> 608260
    //   mObjectsAllocated BitArray<4608> +608264         72 * 8        -> 608840
    //   tail pad to alignof(VolumeSlot) == 16                          -> 608848
    // and 608848 IS the measured offset of VolumeManager::mVolumePool.
    static_assert(sizeof(VolumeSlot) == 128,
                  "VolumeSlot is KI_VOLUME_SLOT_SIZE bytes (X360 pool stride 589824/4608)");
    static_assert(alignof(VolumeSlot) == 16,
                  "VolumeSlot holds a serialised rw::collision::Volume image (descriptor alignment 0x10)");
    static_assert(sizeof(CgsContainers::ObjectPool<VolumeSlot, 4608, s32>) == 608848,
                  "ObjectPool<VolumeSlot,4608,int> spans the X360 offsets 0/589824/608256/608264 + 16-byte tail pad");
    static_assert(sizeof(VolumeStore<4608>) == 608848,
                  "VolumeStore<4608> is its pool (X360: VolumeManager::mVolumePool sits at +608848)");
}
