#pragma once

// BrnDirector::SceneQueryInterface  (X360 asserts ..\GameSource\Director/Utils/
// BrnSceneQueryInterface.h:316/317) -- the Director-side thin wrapper/adaptor over the
// SceneManager producer CgsSceneManager::SceneManagerIO::SceneQueryInterface. It holds the
// producer plus a small table of "post office" hand-off objects (one per query kind). Each
// public test first mints a SceneQueryId via the matching post office's OutEvent* and then
// forwards the staged query to the producer.
//
// MEMBER OFFSETS (X360-attested):
//   +0x00 mpSceneQueryInterface            producer (assert @ .h:316; *this in VolumeTestDeepest;
//                                          Clear leaves slot 0 untouched)
//   +0x04..+0x14  five post-office slots    Clear resets each non-null slot (interior field
//                                          offsets attested; slot element TYPES not recovered)
//   +0x18 mpVolumeTestDeepestPostOffice     deepest-volume-test post office (assert @ .h:317;
//                                          a1[6] in VolumeTestDeepest; Clear slot 6)
// The post-office element TYPES are NOT attested by name, so slots +0x04..+0x18 are modelled as
// raw void* hand-off pointers (HARD RULE 3: names/types not fabricated). Only the two
// assert-named members carry a recovered type.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryInterface.h" // producer + EExclusionMode

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // Post-office OutEvent for the deepest volume test -- mints a 16-bit SceneQueryId for the
    // staged query record. Body not yet recovered (own ledger TU); declared here, resolved at
    // link time. (X360 free function; Hex-Rays: int(int a1, _DWORD* a2). Low 16 bits are the id.)
    u32 OutEventVolumeTestDeepest(void* lpPostOffice, void* lpQueryParams);
}
}

namespace BrnDirector
{
    // Slot-1 post-office reset helper (address-only name sub_8221CC98 in the X360 build).
    u32* sub_8221CC98(void* lpSlot);

    struct SceneQueryInterface
    {
        CgsSceneManager::SceneManagerIO::SceneQueryInterface* mpSceneQueryInterface;  // +0x00
        void*                                                 mpPostOffice04;         // +0x04
        void*                                                 mpPostOffice08;         // +0x08
        void*                                                 mpPostOffice0C;         // +0x0C
        void*                                                 mpPostOffice10;         // +0x10
        void*                                                 mpPostOffice14;         // +0x14
        void*                                                 mpVolumeTestDeepestPostOffice; // +0x18

        // @ 0x8221CD38 -- resets each held post office (slot 0 producer untouched).
        u32* Clear();

        // @ 0x82233128 -- mints a deepest-volume-test SceneQueryId via
        // mpVolumeTestDeepestPostOffice's OutEvent, sets the 0x10000 "fine" bit, then forwards
        // the query to the producer. Returns int (producer's bool widened back to r3).
        int VolumeTestDeepest(void*                                           lpQueryParams,
                              u32                                             lx32EntityTypeFlags,
                              u8                                              lxVolumeTypeFlags,
                              const void*                                     lpVolumeData,
                              const void*                                     lpTransform,
                              u32                                             lExcludeEntityId,
                              CgsSceneManager::SceneManagerIO::EExclusionMode leExclusionMode);

        static void _AssertLayout();
    };
}
