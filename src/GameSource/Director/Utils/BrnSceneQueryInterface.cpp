#include "GameSource/Director/Utils/BrnSceneQueryInterface.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventLineTestNearest.h"   // ENearestExclusionMode

// =============================================================================
// BrnDirector::SceneQueryInterface -- the director's per-frame scene-query handle.
// Reconstructed from BURNOUT_X360_ARTIST.XEX (semantic parity, not byte match).
//   Clear             @ 0x8221CD38
//   LineTestFine      @ 0x82232F68
//   LineTestNearest   @ 0x82233048  (the producer TU CgsSceneManagerIO_SceneQueryInterface_LineTestNearest.cpp is mounted)
//   VolumeTestDeepest @ 0x82233128
// ⭐ TYPED 2026-09-25 (FX-DIRECTOR2): see the header banner.
// =============================================================================

namespace BrnDirector
{
    // -------------------------------------------------------------------------
    // Clear @ 0x8221CD38
    //
    // Each post office that is present forgets the boxes it handed ids to last frame. The fine
    // office's Clear is the specialised one (the X360 calls it out of line, sub_8221CC98 -- it
    // empties every box that already GOT its pointer package first); the other five are inlined as
    // a single `stw 0` to the office's length word (+0xA0 / +0x28 / +0x28 / +0x04 / +0x28). The
    // producer (slot 0) is not touched.
    // -------------------------------------------------------------------------
    void SceneQueryInterface::Clear()
    {
        if (mpLineTestFinePostOffice)            mpLineTestFinePostOffice->Clear();
        if (mpLineTestNearestPostOffice)         mpLineTestNearestPostOffice->Clear();
        if (mpLineTestFastDoubleSidedPostOffice) mpLineTestFastDoubleSidedPostOffice->Clear();
        if (mpSphereTestFastPostOffice)          mpSphereTestFastPostOffice->Clear();
        if (mpVolumeTestFinePostOffice)          mpVolumeTestFinePostOffice->Clear();
        if (mpVolumeTestDeepestPostOffice)       mpVolumeTestDeepestPostOffice->Clear();
    }

    // -------------------------------------------------------------------------
    // LineTestFine @ 0x82232F68
    //   assert mpSceneQueryInterface != NULL          (h:188, 0xBC)
    //   assert mpLineTestFinePostOffice != NULL       (h:189, 0xBD)
    //   id = mpLineTestFinePostOffice->AddPostBox(lrPostBox)   (0x8222D158)
    //   producer->LineTestFine((id & 0xFFFF) | 0x10000, flags, vflags, exclude, mode; v1 start, v2 end)
    // The asserts are non-gating tripwires (CGS_ASSERT); the forward always runs.
    // -------------------------------------------------------------------------
    void SceneQueryInterface::LineTestFine(LineTestFinePostBox&                             lrPostBox,
                                           u32                                              lx32EntityTypeFlags,
                                           u8                                               lxVolumeTypeFlags,
                                           const Vector3&                                   lLineStart,
                                           const Vector3&                                   lLineEnd,
                                           CgsSceneManager::EntityId                        lExcludeEntityId,
                                           CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode) const
    {
        CGS_ASSERT(mpSceneQueryInterface != 0,    "mpSceneQueryInterface != NULL");      // h:188
        CGS_ASSERT(mpLineTestFinePostOffice != 0, "mpLineTestFinePostOffice != NULL");   // h:189

        CgsSceneManager::SceneQueryId lQueryId;
        lQueryId.Set(KU_DIRECTOR_QUERY_OWNER, mpLineTestFinePostOffice->AddPostBox(lrPostBox));

        mpSceneQueryInterface->LineTestFine(lLineStart, lLineEnd, lQueryId, lx32EntityTypeFlags,
                                            lxVolumeTypeFlags, lExcludeEntityId, leExclusionMode);
    }

    // -------------------------------------------------------------------------
    // LineTestNearest @ 0x82233048
    //   assert mpSceneQueryInterface != NULL          (h:216, 0xD8)
    //   assert mpLineTestNearestPostOffice != NULL    (h:217, 0xD9)
    //   id = mpLineTestNearestPostOffice->AddPostBox(lrPostBox)   (0x8222D288)
    //   producer->LineTestNearest((id & 0xFFFF) | 0x10000, ...)   (0x82216FD0)
    // The producer's exclusion-mode parameter is spelled ENearestExclusionMode on this host (the
    // tree keeps the nearest event's enum distinct to avoid a redefinition); its two values are the
    // EExclusionMode values (E_EXCLUDE_ENTITY_ONLY 0, E_EXCLUDE_ALL_CHILD_PARTS 1), so the console's
    // single register is passed through unchanged.
    // -------------------------------------------------------------------------
    void SceneQueryInterface::LineTestNearest(LineTestNearestPostBox&                          lrPostBox,
                                              u32                                              lx32EntityTypeFlags,
                                              u8                                               lxVolumeTypeFlags,
                                              const Vector3&                                   lLineStart,
                                              const Vector3&                                   lLineEnd,
                                              CgsSceneManager::EntityId                        lExcludeEntityId,
                                              CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode) const
    {
        CGS_ASSERT(mpSceneQueryInterface != 0,       "mpSceneQueryInterface != NULL");        // h:216
        CGS_ASSERT(mpLineTestNearestPostOffice != 0, "mpLineTestNearestPostOffice != NULL");  // h:217

        CgsSceneManager::SceneQueryId lQueryId;
        lQueryId.Set(KU_DIRECTOR_QUERY_OWNER, mpLineTestNearestPostOffice->AddPostBox(lrPostBox));

        mpSceneQueryInterface->LineTestNearest(
            lLineStart, lLineEnd, lQueryId, lx32EntityTypeFlags, lxVolumeTypeFlags, lExcludeEntityId,
            static_cast<CgsSceneManager::SceneManagerIO::ENearestExclusionMode>(leExclusionMode));
    }

    // -------------------------------------------------------------------------
    // VolumeTestDeepest @ 0x82233128
    //   assert mpSceneQueryInterface != NULL          (h:316, 0x13C)
    //   assert mpVolumeTestDeepestPostOffice != NULL  (h:317, 0x13D)
    //   id = mpVolumeTestDeepestPostOffice->AddPostBox(lrPostBox)   (0x8222D628)
    //   producer->VolumeTestDeepest((id & 0xFFFF) | 0x10000, flags, vflags, volume, transform,
    //                               exclude, mode)                    (0x822170B0)
    // -------------------------------------------------------------------------
    void SceneQueryInterface::VolumeTestDeepest(VolumeTestDeepestPostBox&                        lrPostBox,
                                                u32                                              lx32EntityTypeFlags,
                                                u8                                               lxVolumeTypeFlags,
                                                const void*                                      lpVolume,
                                                const Matrix44Affine&                            lrTransform,
                                                CgsSceneManager::EntityId                        lExcludeEntityId,
                                                CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode) const
    {
        CGS_ASSERT(mpSceneQueryInterface != 0,         "mpSceneQueryInterface != NULL");          // h:316
        CGS_ASSERT(mpVolumeTestDeepestPostOffice != 0, "mpVolumeTestDeepestPostOffice != NULL");  // h:317

        CgsSceneManager::SceneQueryId lQueryId;
        lQueryId.Set(KU_DIRECTOR_QUERY_OWNER, mpVolumeTestDeepestPostOffice->AddPostBox(lrPostBox));

        mpSceneQueryInterface->VolumeTestDeepest(lQueryId.mId, lx32EntityTypeFlags, lxVolumeTypeFlags,
                                                 lpVolume, &lrTransform,
                                                 static_cast<u32>(lExcludeEntityId), leExclusionMode);
    }
}
