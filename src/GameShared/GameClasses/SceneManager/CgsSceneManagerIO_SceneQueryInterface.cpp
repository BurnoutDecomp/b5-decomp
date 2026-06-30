#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                              // CgsModule::BaseEventQueue<T>::AddEvent (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"    // CgsSceneManager::SceneManagerIO::SceneQueryInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTest.h"          // InEventLineTestFine element (committed, 0x40 stride) + EExclusionMode
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventVolumeTestDeepest.h" // InEventVolumeTestDeepest element (224B opaque)

#include <cstring>   // std::memcpy (volume/transform opaque staging, models the Xbox block-copy)

// =============================================================================
// CgsSceneManager::SceneManagerIO::SceneQueryInterface -- two of the producer methods
// (LineTestFine @ 0x82216EF0, VolumeTestDeepest @ 0x822170B0). The sibling LineTestNearest
// @ 0x82216FD0 lives in CgsSceneManagerIO_SceneQueryInterface_LineTestNearest.cpp; HasData
// @ 0x82204E48 lives in CgsSceneManagerModuleIO.cpp.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
// =============================================================================

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine  @ 0x82216EF0
//
// Packs the caller's line-test arguments into a stack-local InEventLineTestFine record and
// appends it to this->mpFineLineTestQueue (member +0x00). The line start/end Vector3 lanes
// arrive in the two leading vector-register args (v1/v2); the GPR args carry the scalar fields.
//
// X360 ARG -> store mapping (authoritative; register order == declared param order):
//   r3 = this
//   r4 -> stw +0x20  mQueryId             (a2)
//   r5 -> stw +0x24  mx32EntityTypeFlags  (a3)
//   r6 -> stb +0x30  mxVolumeTypeFlags    (a4, byte)
//   r7 -> stw +0x28  mExcludeEntityId     (a5)
//   r8 -> stw +0x2C  meExclusionMode      (a6) -- the value the exclusion-mode assert gates on
//   v1 -> stvx128 +0x00  mLineStart
//   v2 -> stvx128 +0x10  mLineEnd
//
// The leExclusionMode assert fires when a6 is neither E_EXCLUDE_ENTITY_ONLY nor
// E_EXCLUDE_ALL_CHILD_PARTS; the mpFineLineTestQueue assert fires when this+0x00 == 0. Both are
// non-gating tripwires (CGS_ASSERT collapses Begin/Fire/EndAssert); the AddEvent always runs.
// -----------------------------------------------------------------------------
bool CgsSceneManager::SceneManagerIO::SceneQueryInterface::LineTestFine(
    const Vector3&  lLineStart,
    const Vector3&  lLineEnd,
    SceneQueryId    lQueryId,
    u32             lx32EntityTypeFlags,
    u8              lxVolumeTypeFlags,
    EntityId        lExcludeEntityId,
    EExclusionMode  leExclusionMode)
{
    CGS_ASSERT(leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY,
               "leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY");
    CGS_ASSERT(mpFineLineTestQueue, "mpFineLineTestQueue");

    InEventLineTestFine lEvent;
    lEvent.mLineStart          = lLineStart;
    lEvent.mLineEnd            = lLineEnd;
    lEvent.mQueryId            = lQueryId;
    lEvent.mx32EntityTypeFlags = lx32EntityTypeFlags;
    lEvent.mExcludeEntityId    = lExcludeEntityId;
    lEvent.meExclusionMode     = leExclusionMode;
    lEvent.mxVolumeTypeFlags   = lxVolumeTypeFlags;

    return mpFineLineTestQueue->AddEvent(lEvent);
}

// -----------------------------------------------------------------------------
// CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest  @ 0x822170B0
//
// Stages a deepest-volume-test query event on the stack and appends it to
// mpFineVolumeTestDeepestQueue (member +0x10). The 224-byte InEventVolumeTestDeepest element is
// an OPAQUE byte span (no field-level DWARF; see CgsSceneManagerIO_EventVolumeTestDeepest.h), so
// the producer's stack staging is reproduced by writing the asm-attested BYTE OFFSETS into the
// opaque payload rather than naming interior fields:
//   +0x00 transform  : 64-byte block copied from a6 (lpTransform, four SIMD lanes)
//   +0x40 query id    : a2
//   +0x44 entity-type-flags : a3
//   +0x48 exclude-entity-id : a7
//   +0x4C exclusion-mode    : a8 (the range-asserted value)
//   +0x50 volume data : 128-byte opaque rw-collision-volume image copied from a5 (lpVolumeData)
//   +0xD0 volume-type-flags : a4 (byte, stored last)
// a6 (transform) and a5 (volume) are TWO DISTINCT source pointers. Both asserts are non-gating
// tripwires; the AddEvent always runs.
// -----------------------------------------------------------------------------
int CgsSceneManager::SceneManagerIO::SceneQueryInterface::VolumeTestDeepest(
    u32            lQueryId,            // a2 -> payload +0x40
    u32            lx32EntityTypeFlags, // a3 -> payload +0x44
    u8             lxVolumeTypeFlags,   // a4 -> payload +0xD0 (byte, stored last)
    const void*    lpVolumeData,        // a5 -> 128-byte blob source (payload +0x50)
    const void*    lpTransform,         // a6 -> 64-byte transform-lane source (payload +0x00..+0x40)
    u32            lExcludeEntityId,    // a7 -> payload +0x48
    EExclusionMode leExclusionMode)     // a8 -> payload +0x4C (asserted in-range)
{
    CGS_ASSERT(leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY,
               "leExclusionMode == E_EXCLUDE_ALL_CHILD_PARTS || leExclusionMode == E_EXCLUDE_ENTITY_ONLY");
    CGS_ASSERT(mpFineVolumeTestDeepestQueue, "mpFineVolumeTestDeepestQueue");

    InEventVolumeTestDeepest lEvent;
    u8* lpPayload = lEvent.macOpaquePayload;

    // +0x00 transform (64-byte block from a6); +0x50 volume image (128-byte block from a5).
    std::memcpy(lpPayload + 0x00, lpTransform,  64);
    std::memcpy(lpPayload + 0x50, lpVolumeData, 128);

    // Scalar fields staged at their asm-attested byte offsets.
    *reinterpret_cast<u32*>(lpPayload + 0x40) = lQueryId;
    *reinterpret_cast<u32*>(lpPayload + 0x44) = lx32EntityTypeFlags;
    *reinterpret_cast<u32*>(lpPayload + 0x48) = lExcludeEntityId;
    *reinterpret_cast<u32*>(lpPayload + 0x4C) = static_cast<u32>(leExclusionMode);
    lpPayload[0xD0]                           = lxVolumeTypeFlags;  // stored last

    return mpFineVolumeTestDeepestQueue->AddEvent(lEvent) ? 1 : 0;
}
