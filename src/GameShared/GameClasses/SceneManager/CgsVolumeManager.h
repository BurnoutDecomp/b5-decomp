#pragma once

// ===========================================================================
// CgsSceneManager::VolumeManager
//   Home: GameShared/GameClasses/SceneManager/CgsVolumeManager.{h,cpp}
//
// The scene's collision-VOLUME registry: it maps a public VolumeId to a pooled
// VolumeManagerVolume record, and (for dynamic volumes) owns the in-place copy of
// the serialised rw collision volume inside VolumeStore<4608>. Every scene
// VolumeInstance references a volume by the pool index AddDynamicVolume returns.
// Embedded BY VALUE in CgsSceneManager::SceneManagerModule (mVolumeManager,
// X360 +0x2C7800).
//
// Member set + signatures: DecFIGS DWARF (CgsVolumeManager.h), gated on the X360
// ARTIST ledger. The 915,104-byte `maPooledStorage` blob that stood here until
// wave Q5 is GONE -- it is the four members below, whose CONSOLE offsets are all
// measured from the ARTIST bodies (quoted per member). Host offsets are NOT the
// console's (ResourceHandle's two pointers widen, the hash-bin pointer widens);
// everything is reached by name, per the project's semantic-parity rule.
//
// X360 ARTIST functions homed in CgsVolumeManager.cpp:
//   VolumeManager::Prepare              @ 0x828CFD38 (36)
//   VolumeManager::RemoveVolume         @ 0x828CFDC8 (71)
//   VolumeManager::ReplaceDynamicVolume @ 0x828CFEE8 (48)   [export hole, IDA-recovered]
//   VolumeManager::AddDynamicVolume     @ 0x828D1708 (401)
//   VolumeManager::GetRwVolume          @ 0x828C5E68 (87)   [export hole, IDA-recovered]
// ⚠️ The banner that stood here before wave Q5 attributed Prepare to 0x828CFFA8.
// THAT WAS WRONG: 0x828CFFA8 is CgsSceneManager::SpatialPartitionManager::Prepare
// (progress/identity.json). VolumeManager::Prepare is 0x828CFD38, which is also what
// the WorldLinkStubs boot gate's own comment says. A committed address is a claim.
//
// INLINING NOTE: the console defines the small accessors INSIDE the header (their baked
// assert file is CgsVolumeManager.h -- GetRwVolume's two asserts are h:182/h:183), and the
// mutators inside CgsVolumeManager.cpp (assert file CgsVolumeManager.cpp, lines 144-154 /
// 236 / 266 / 271). GetRwVolume is emitted out of line at 0x828C5E68 because five separate
// TUs call it. It is DECLARED here and DEFINED in the .cpp: putting its body here would
// drag SDKs/EATech/rwcollision + CgsResourcePtr + CgsCollisionMeshData into every includer
// of CgsSceneManagerModule.h (AGENTS forward-declaration/cascade exception), and an
// out-of-line definition is link-equivalent for the five cross-TU callers.
// ===========================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsIndexedHashTable.h"      // IndexedHashTable(Element)
#include "GameShared/GameClasses/Containers/CgsObjectPool.h"            // ObjectPool<T,N,TIndex>
#include "GameShared/GameClasses/SceneManager/CgsVolumeId.h"            // VolumeId
#include "GameShared/GameClasses/SceneManager/CgsVolumeStore.h"         // VolumeStore<N>, VolumeSlot, VolRef::Volume
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h"   // CgsResource::ResourceHandle

namespace CgsSceneManager
{
    // The volume-pool capacity the X360 bakes into GetRwVolume's own assert text
    // ("liVolumeIndex >=0 && liVolumeIndex < KI_MAX_NUM_VOLUMES", `cmpwi r31,0x13B8`
    // @0x828C5E8C) and into the ObjectPool<VolumeManagerVolume,5048,int> bounds checks.
    const s32 KI_MAX_NUM_VOLUMES = 5048;

    // Bin count of the VolumeId -> pool-index table. Measured: VolumeManager::Prepare
    // @0x828CFD38 runs its bucket-init loop exactly 227 times (`li r9,0xE3`) at a 12-byte
    // stride and then stamps mbInitialised at +0xAA4 == 227*12. DWARF spells the
    // instantiation IndexedHashTable<VolumeId,uint32_t,227u>.
    const u32 KU_NUM_VOLUME_HASH_BINS = 227;

    // DWARF CgsVolumeManager.h:60 -- one pooled collision-volume record.
    // CONSOLE OFFSETS (all measured; 32-byte stride, pinned by ObjectPool<...>::operator[]
    // @0x828B7910 `slwi r11,r29,5` and by maObjectPool[5048] spanning [0,161536)):
    //   +0x00  mVolumeId           std r24,0(r11)      AddDynamicVolume @0x828D1D10
    //   +0x08  mxFlags             stb r17,8(r11)                       @0x828D1D1C
    //   +0x0C  mVolumeHandle       addi r4,r3,0xC      GetRwVolume      @0x828C5F50
    //   +0x14  miVolumeStoreIndex  stw r29,0x14(r11)   AddDynamicVolume @0x828D1D18
    //   +0x18  mbStaticVolume      stb r30,0x18(r11)                    @0x828D1D14
    // On the x64 host the record is 40 bytes (ResourceHandle's two pointers widen); the
    // pool indexes it by sizeof(T), so nothing depends on the console stride.
    struct VolumeManagerVolume
    {
        // DWARF CgsSceneManagerTypes.h:44, nested typedef spelled
        // VolumeManagerVolume::VolumeTypeFlags in the DWARF signatures below.
        typedef u8 VolumeTypeFlags;

        VolumeId                    mVolumeId;           // h:62  console +0x00
        VolumeTypeFlags             mxFlags;             // h:63  console +0x08
        CgsResource::ResourceHandle mVolumeHandle;       // h:64  console +0x0C (static volumes only)
        s32                         miVolumeStoreIndex;  // h:65  console +0x14
        bool                        mbStaticVolume;      // h:66  console +0x18
    };

    class VolumeManager
    {
    public:
        // h:84 -- DWARF call list: { VolumeStore<4608>::Construct,
        // ObjectPool<VolumeManagerVolume,5048,int>::Construct }.
        void Construct();

        // h:87 / h:93 -- declaration-only. The console emits no out-of-line body, their
        // DWARF twins are ObjectPool::Destruct (which this tree's ObjectPool does not
        // declare), and nothing in the reconstructed tree calls either. Left undefined
        // rather than invented.
        void Destruct();
        bool Release();

        // h:90 @0x828CFD38 -- clear the store, the volume pool and the id->index table.
        // Always returns true (the X360 body has no failure path: `li r3,1`).
        bool Prepare();

        // h:100 @0x828D1708 -- register a DYNAMIC volume: bounding-box tripwires, take a
        // pool slot, copy the volume into the store, stamp the record and publish the id.
        // Returns the volume-pool index (the value scene volume instances hold), or -1 when
        // the pool is exhausted.
        s32 AddDynamicVolume(VolumeId lVolumeId,
                             VolumeManagerVolume::VolumeTypeFlags lxVolumeTypeFlags,
                             const VolRef::Volume* lpVolume);

        // h:106 @0x828CFEE8 -- swap the stored image of an already-registered dynamic volume.
        void ReplaceDynamicVolume(VolumeId lVolumeId, const VolRef::Volume* lpVolume);

        // h:111 @0x828CFDC8 -- retire a volume: release its store slot (dynamic only), free
        // its pool slot and drop its id->index entry.
        void RemoveVolume(VolumeId lVolumeId);

        // h:118 -- declaration-only in this slice. The console body is its own function
        // (DWARF CgsVolumeManager.cpp:185) and it is NOT in the X360 ledger / export set,
        // and no reconstructed caller exists yet; not invented here.
        s32 AddStaticVolume(VolumeId lVolumeId,
                            VolumeManagerVolume::VolumeTypeFlags lxVolumeTypeFlags,
                            const CgsResource::ResourceHandle& lrVolumeResourceHandle);

        // h:123 -- declaration-only (header-inline on the console, no export, no caller).
        VolumeId GetVolumeIdByIndex(s32 liVolumeIndex) const;

        // h:128 -- id -> pool index, or KI_INVALID_VOLUME_INDEX. Header-inline on the
        // console; its exact shape is pinned by the two bodies that inlined it
        // (RemoveVolume @0x828CFDE8.. and ReplaceDynamicVolume @0x828CFF0C..), and the
        // DWARF lists it in RemoveVolume's/ReplaceDynamicVolume's call graph.
        s32 GetVolumeIndexByID(VolumeId lVolumeId) const;

        // h:133 @0x828C5E68 -- the rw collision volume behind a pool index. Two arms:
        // STATIC volumes resolve mVolumeHandle through a
        // ResourcePtr<CgsPhysics::CollisionMeshData>; DYNAMIC volumes read the store slot.
        // This is the accessor the whole contact-generation middle calls
        // (SceneManagerModule::ProcessAddForCollisionEvent / UpdateCollisionBody,
        // OverlapCullingModule::ProcessOverlap x2 / IsInsideEscapeVolume /
        // DoInternalCollision, FineIntersectionTestModule::Compute*).
        const VolRef::Volume* GetRwVolume(s32 liVolumeIndex) const;

        // h:138 -- declaration-only (header-inline on the console, no export; no
        // reconstructed caller yet, and its assert set is not attested).
        VolumeManagerVolume::VolumeTypeFlags GetVolumeTypeFlags(s32 liVolumeIndex) const;

    private:
        // DWARF h:142-146, member order verbatim; console offsets measured.
        VolumeStore<4608>                                                  mVolumeStore;   // h:142  +0x00000
        CgsContainers::ObjectPool<VolumeManagerVolume, KI_MAX_NUM_VOLUMES, s32>
                                                                           mVolumePool;    // h:143  +608848
        CgsContainers::IndexedHashTableElement<VolumeId, u32>
                                        maVolumeHashElement[KI_MAX_NUM_VOLUMES];           // h:145  +791216
        CgsContainers::IndexedHashTable<VolumeId, u32, KU_NUM_VOLUME_HASH_BINS>
                                                                           mVolumeIdToIndex; // h:146 +912368
    };
}
