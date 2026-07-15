#pragma once

#include "BrnCommonTypes.h"                                          // Matrix44, Vector4
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"     // CgsModule::VariableEventQueue<>, Event
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"     // CgsSceneManager::SceneQueryId

// ===========================================================================
// CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<N> and its coarse-query
// event payloads.
//   Home: GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.{h,cpp}
//
// This is the full reconstruction of the coarse-query input queue, whose minimal
// sized-blob slice lives in CgsSceneManagerIO_CoarseQuery.h
// (CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue). The queue is a
// VariableEventQueue<N,16> subclass that adds the typed Sphere/Frustum/Volume
// test enqueue helpers; it carries NO extra data members (DWARF
// CgsSceneManagerIO_CoarseQuery.h:90), so sizeof matches the base queue.
//
// EVENT-TYPE IDS (the queue's record ids) -- the X360 FrustumTestVp emitter passes
// type 4; the others follow the DWARF event-class ordering.
// ===========================================================================

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // EntityTypeFlags is a 32-bit set of entity-category bits (DWARF
    // CgsSceneManagerIO_FineQuery.h: `typedef uint32_t EntityTypeFlags;`).
    typedef u32 EntityTypeFlags;

    // Coarse-query event record ids (CgsSceneManagerIO_CoarseQuery.h enum).
    enum ECoarseQueryEvent
    {
        E_IN_EVENT_SPHERE_TEST     = 1,
        E_IN_EVENT_FRUSTUM_TEST    = 2,
        E_IN_EVENT_FRUSTUM_TEST_VP = 4,   // X360 0x82746770: `li r5, 4`
        E_IN_EVENT_VOLUME_TEST     = 8,
    };

    // The view-projection frustum-test event. X360 layout (asm @ 0x82746770; the
    // stack image is built store-for-store and pushed via AddEvent<...>):
    //   +0x00  mViewProjection   (Matrix44, 64 bytes; 4 lvx128 rows from r7)
    //   +0x40  maFrustumPlanes[8] (128 bytes; the 16-doubleword copy from r6)
    //   +0xC0  mQueryId          (SceneQueryId; r4)
    //   +0xC4  mx32EntityTypeFlags (u32; r5)
    //   +0xC8  mxQueryFlags      (u32; r8)
    //
    // mFrustum is the 128-byte (8 x Vector4) swizzled-plane frustum image the X360
    // copies wholesale; it is modelled here as a named Vector4 plane array (matching
    // the CgsGeometric::Frustum swizzled-planes layout the 128-byte copy attests)
    // rather than the committed 96-byte rw::collision::Frustum, so the named fields
    // land at the X360-observed offsets without re-sizing a committed type.
    struct alignas(16) InEventFrustumTestVp : public CgsModule::Event
    {
        Matrix44        mViewProjection;     // +0x00
        Vector4         maFrustumPlanes[8];  // +0x40  (mFrustum, 128 bytes)
        SceneQueryId    mQueryId;            // +0xC0
        EntityTypeFlags mx32EntityTypeFlags; // +0xC4
        u32             mxQueryFlags;        // +0xC8
    };

    // The coarse sphere-test event record (E_IN_EVENT_SPHERE_TEST). The X360 typed
    // AddEvent<InEventSphereTest> @0x8273F9D8 bakes `li r6, 0x20`, so its queued byte
    // image is 32 bytes. No field-level DWARF is recovered in any decompiled TU's scope
    // (the emitter builds the stack image and the queue block-copies it whole), so the
    // payload is modelled as an OPAQUE byte span at the X360-attested size, mirroring the
    // opaque-blob homes (CgsSceneManagerIO_EventSphereTest.h / EventAddDynamicVolume).
    // FLAG: opaque interior -- field names/types NOT fabricated (HARD RULE 3); only the
    // X360-attested sizeof(==32) is load-bearing (the liSize the typed AddEvent passes).
    struct alignas(16) InEventSphereTest : public CgsModule::Event
    {
        u8 macOpaquePayload[32];   // +0x00  opaque (X360-attested 32-byte AddEvent liSize)
    };
    static_assert(sizeof(InEventSphereTest) == 32, "InEventSphereTest byte image must match X360 AddEvent liSize (0x20)");

    // Coarse-query input queue. Adds the typed enqueue helpers over the variable
    // event queue base; carries no extra data members (DWARF
    // CgsSceneManagerIO_CoarseQuery.h:90).
    template <s32 SizeBytes>
    class InCoarseQueryQueue : public CgsModule::VariableEventQueue<SizeBytes, 16>
    {
    public:
        // @ X360 0x82746770 (SizeBytes == 16384). Build the InEventFrustumTestVp
        // record (view-projection matrix + the frustum planes + the query id /
        // entity-type / query flags) and push it onto the queue.
        void FrustumTestVp(SceneQueryId lQueryId,
                           EntityTypeFlags lx32EntityTypeFlags,
                           const Vector4* lpFrustumPlanes,
                           const Matrix44& lViewProjection,
                           u32 lxQueryFlags);
    };

    // -------- FrustumTestVp @ X360 0x82746770 --------
    template <s32 SizeBytes>
    void InCoarseQueryQueue<SizeBytes>::FrustumTestVp(
        SceneQueryId lQueryId,
        EntityTypeFlags lx32EntityTypeFlags,
        const Vector4* lpFrustumPlanes,
        const Matrix44& lViewProjection,
        u32 lxQueryFlags)
    {
        InEventFrustumTestVp lEvent;
        lEvent.mViewProjection     = lViewProjection;
        for (s32 li = 0; li < 8; ++li)                 // the 16-doubleword (8 x Vec4) copy
            lEvent.maFrustumPlanes[li] = lpFrustumPlanes[li];
        lEvent.mQueryId            = lQueryId;
        lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;
        lEvent.mxQueryFlags        = lxQueryFlags;

        this->template AddEvent<InEventFrustumTestVp>(&lEvent, E_IN_EVENT_FRUSTUM_TEST_VP);
    }
}
}
