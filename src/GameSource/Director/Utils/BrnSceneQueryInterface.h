#pragma once

// ============================================================================
// GameSource/Director/Utils/BrnSceneQueryInterface.h
//
// BrnDirector::SceneQueryInterface (DWARF BrnSceneQueryInterface.h:54) -- the director's per-frame
// scene-query handle. It pairs the SceneManager PRODUCER (the director's own
// CgsSceneManager::SceneManagerIO::SceneQueryInterface, published by DirectorIO::
// SceneQueryOutputBuffer) with the module's six POST OFFICES. A camera asks for a test through one
// of the three test methods below: the matching post office mints a 16-bit id for the caller's post
// box (AddPostBox, which also raises the box to WAITING_FOR_PACKAGE), the id is tagged with the
// director's owner byte, and the test is forwarded to the producer. DirectorModule::
// ProcessSceneQueryResults @0x82239278 later hands each result back through the same office.
//
// ⭐ TYPED 2026-09-25 (FX-DIRECTOR2, the camera scene-query closure). The raw-void* slots
// (mpPostOffice04..14) and the two free-function stand-ins (OutEventVolumeTestDeepest /
// sub_8221CC98, stubbed in DirectorLinkStubs.cpp) are retired: the slots are the DWARF's typed
// post-office pointers and the id minting is PostOffice<T,N>::AddPostBox.
//
// MEMBER ORDER (DWARF :162..:169, console offsets pinned by Clear @0x8221CD38 and the three tests):
//   +0x00 mpSceneQueryInterface              the producer              (asserts h:188/:216/:316)
//   +0x04 mpLineTestFinePostOffice           Clear: the specialised Clear (0x8221CC98)
//   +0x08 mpLineTestNearestPostOffice        Clear: `stw 0, 0xA0`      (asserts h:217)
//   +0x0C mpLineTestFastDoubleSidedPostOffice Clear: `stw 0, 0x28`
//   +0x10 mpSphereTestFastPostOffice         Clear: `stw 0, 0x28`
//   +0x14 mpVolumeTestFinePostOffice         Clear: `stw 0, 0x04`
//   +0x18 mpVolumeTestDeepestPostOffice      Clear: `stw 0, 0x28`      (asserts h:317)
// The console offsets are provenance; on this host the pointers are 8 bytes and every access is
// by name.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                                // Vector3, Matrix44Affine
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h"   // the producer + EExclusionMode
#include "GameShared/GameClasses/SceneManager/CgsEntityId.h"                              // CgsSceneManager::EntityId
#include "GameSource/Director/Utils/BrnDirectorPostOfficeTypes.h"                         // the six post-office / post-box types

namespace BrnDirector
{
    struct SceneQueryInterface
    {
        typedef CgsSceneManager::SceneManagerIO::SceneQueryInterface ProducerInterface;

        // The owner byte the director stamps into every query id it mints (`oris rX, rY, 1` --
        // bits [16..23] == 1 -- in LineTestFine 0x8223301C, LineTestNearest 0x822330FC and
        // VolumeTestDeepest 0x822331CC; SceneQueryId::Set packs exactly that).
        enum { KU_DIRECTOR_QUERY_OWNER = 1 };

        // DWARF :79. No out-of-line X360 copy: DirectorModule::PreSceneQueryUpdate @0x8225C768 and
        // ::Update @0x82275300 store the seven words inline (Update stores the producer and six
        // NULLs -- it answers, it does not ask). Clear() is NOT part of Construct; both callers
        // call it straight after.
        void Construct(ProducerInterface*                 lpSceneQueryInterface,
                       LineTestFinePostOffice*            lpLineTestFinePostOffice,
                       LineTestNearestPostOffice*         lpLineTestNearestPostOffice,
                       LineTestFastDoubleSidedPostOffice* lpLineTestFastDoubleSidedPostOffice,
                       SphereTestFastPostOffice*          lpSphereTestFastPostOffice,
                       VolumeTestFinePostOffice*          lpVolumeTestFinePostOffice,
                       VolumeTestDeepestPostOffice*       lpVolumeTestDeepestPostOffice)
        {
            mpSceneQueryInterface               = lpSceneQueryInterface;
            mpLineTestFinePostOffice            = lpLineTestFinePostOffice;
            mpLineTestNearestPostOffice         = lpLineTestNearestPostOffice;
            mpLineTestFastDoubleSidedPostOffice = lpLineTestFastDoubleSidedPostOffice;
            mpSphereTestFastPostOffice          = lpSphereTestFastPostOffice;
            mpVolumeTestFinePostOffice          = lpVolumeTestFinePostOffice;
            mpVolumeTestDeepestPostOffice       = lpVolumeTestDeepestPostOffice;
        }

        // @ 0x8221CD38 (DWARF :88) -- forget every id handed out last frame: each held post office
        // is Cleared (the producer, slot 0, is untouched).
        void Clear();

        // @ 0x82232F68 (DWARF :102) -- a FINE line test (every intersection along the line).
        void LineTestFine(LineTestFinePostBox&                              lrPostBox,
                          u32                                               lx32EntityTypeFlags,
                          u8                                                lxVolumeTypeFlags,
                          const Vector3&                                    lLineStart,
                          const Vector3&                                    lLineEnd,
                          CgsSceneManager::EntityId                         lExcludeEntityId,
                          CgsSceneManager::SceneManagerIO::EExclusionMode  leExclusionMode) const;

        // @ 0x82233048 (DWARF :114) -- the NEAREST hit along the line.
        void LineTestNearest(LineTestNearestPostBox&                           lrPostBox,
                             u32                                               lx32EntityTypeFlags,
                             u8                                                lxVolumeTypeFlags,
                             const Vector3&                                    lLineStart,
                             const Vector3&                                    lLineEnd,
                             CgsSceneManager::EntityId                         lExcludeEntityId,
                             CgsSceneManager::SceneManagerIO::EExclusionMode  leExclusionMode) const;

        // @ 0x82233128 (DWARF :147) -- the DEEPEST penetration of a volume. The volume and its
        // transform are the producer's opaque 128-byte / 64-byte images (see
        // CgsSceneManagerIO_SceneQueryInterface.cpp).
        void VolumeTestDeepest(VolumeTestDeepestPostBox&                         lrPostBox,
                               u32                                               lx32EntityTypeFlags,
                               u8                                                lxVolumeTypeFlags,
                               const void*                                       lpVolume,
                               const Matrix44Affine&                             lrTransform,
                               CgsSceneManager::EntityId                         lExcludeEntityId,
                               CgsSceneManager::SceneManagerIO::EExclusionMode  leExclusionMode) const;

        // The producer this interface forwards to (the DoUpdate_Director leg appends its queues to
        // the external query buffer).
        ProducerInterface* GetProducer() const { return mpSceneQueryInterface; }

    private:
        ProducerInterface*                 mpSceneQueryInterface;                // :162  +0x00
        LineTestFinePostOffice*            mpLineTestFinePostOffice;             // :164  +0x04
        LineTestNearestPostOffice*         mpLineTestNearestPostOffice;          // :165  +0x08
        LineTestFastDoubleSidedPostOffice* mpLineTestFastDoubleSidedPostOffice;  // :166  +0x0C
        SphereTestFastPostOffice*          mpSphereTestFastPostOffice;           // :167  +0x10
        VolumeTestFinePostOffice*          mpVolumeTestFinePostOffice;           // :168  +0x14
        VolumeTestDeepestPostOffice*       mpVolumeTestDeepestPostOffice;        // :169  +0x18
    };
}
