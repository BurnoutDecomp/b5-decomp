#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <cstddef>   // offsetof

// =============================================================================
// BrnDirector::SceneQueryInterface -- the Director-side wrapper over the SceneManager
// scene-query producer. Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity,
// not byte match).
//   Clear             @ 0x8221CD38
//   VolumeTestDeepest @ 0x82233128
// The interface holds the producer at slot 0 and a table of "post office" hand-off
// pointers (slots 1..6). Post-office element types are not recovered; slots are modelled
// as raw void* (see BrnSceneQueryInterface.h).
// =============================================================================

namespace BrnDirector
{
    // -------------------------------------------------------------------------
    // Clear @ 0x8221CD38
    //
    // Resets each held post office that is non-null:
    //   slot 1: hand off to sub_8221CC98 (the slot's own Reset/Clear) and forward its
    //           result as the return value (null otherwise).
    //   slots 2..6: zero one owned field at the noted byte offset.
    // sub_8221CC98 is not yet reconstructed (no recovered name); resolved at link time.
    // -------------------------------------------------------------------------
    u32* SceneQueryInterface::Clear()
    {
        void** lppSlots = reinterpret_cast<void**>(this);

        u32* lpResult = nullptr;
        if (lppSlots[1])
            lpResult = sub_8221CC98(lppSlots[1]);

        if (lppSlots[2]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[2]) + 160) = 0;
        if (lppSlots[3]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[3]) +  40) = 0;
        if (lppSlots[4]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[4]) +  40) = 0;
        if (lppSlots[5]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[5]) +   4) = 0;
        if (lppSlots[6]) *reinterpret_cast<u32*>(static_cast<u8*>(lppSlots[6]) +  40) = 0;

        return lpResult;
    }

    // -------------------------------------------------------------------------
    // VolumeTestDeepest @ 0x82233128
    //
    // Mints a 16-bit deepest-volume-test SceneQueryId from mpVolumeTestDeepestPostOffice
    // (OutEventVolumeTestDeepest), ORs in the 0x10000 "fine" bit, then forwards the query to
    // the producer. Both slot pointers are asserted non-null (non-gating tripwires;
    // CGS_ASSERT collapses Begin/Fire/EndAssert -- the forward always runs).
    // (asm: clrlwi r11,r11,16 -> `& 0xFFFF`; oris r4,r11,1 -> `| 0x10000`.)
    // -------------------------------------------------------------------------
    int SceneQueryInterface::VolumeTestDeepest(
            void*                                           lpQueryParams,
            u32                                             lx32EntityTypeFlags,
            u8                                              lxVolumeTypeFlags,
            const void*                                     lpVolumeData,
            const void*                                     lpTransform,
            u32                                             lExcludeEntityId,
            CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode)
    {
        CGS_ASSERT(mpSceneQueryInterface != nullptr, "mpSceneQueryInterface != NULL");
        CGS_ASSERT(mpVolumeTestDeepestPostOffice != nullptr, "mpVolumeTestDeepestPostOffice != NULL");

        u32 lx32RawQueryId = CgsSceneManager::SceneManagerIO::OutEventVolumeTestDeepest(
                                 mpVolumeTestDeepestPostOffice, lpQueryParams);
        u32 lx32QueryId = (lx32RawQueryId & 0xFFFFu) | 0x10000u;

        return mpSceneQueryInterface->VolumeTestDeepest(
                   lx32QueryId,
                   lx32EntityTypeFlags,
                   lxVolumeTypeFlags,
                   lpVolumeData,
                   lpTransform,
                   lExcludeEntityId,
                   leExclusionMode);
    }

    // Pointer-invariant layout facts only (LLP64 gate): the two assert-named members sit at
    // their X360-attested word offsets, and the deepest-volume-test post office is Clear's
    // slot 6.
    void SceneQueryInterface::_AssertLayout()
    {
        // Slot indices are the pointer-invariant fact (X360 word slots == void* slots here);
        // raw byte offsets scale with pointer width across the LLP64 gate.
        static_assert(offsetof(SceneQueryInterface, mpSceneQueryInterface) == 0 * sizeof(void*),
                      "mpSceneQueryInterface = slot 0 (X360 +0x00)");
        static_assert(offsetof(SceneQueryInterface, mpVolumeTestDeepestPostOffice) == 6 * sizeof(void*),
                      "mpVolumeTestDeepestPostOffice = Clear slot 6 (X360 +0x18)");
    }
}
