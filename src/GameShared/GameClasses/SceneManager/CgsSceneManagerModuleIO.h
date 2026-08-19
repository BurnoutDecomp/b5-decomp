#pragma once

// Scene-manager IO event payloads (the subset the boot-path event queues embed).
// Reconstructed from the DecFIGS DWARF. Events derive from an empty per-module Event
// base (CgsModule event-queue convention).
#include <cstddef>                                                   // offsetof (OutEventLineTestNearestResult layout pin)
#include "types.hpp"
#include "BrnCommonTypes.h"                                          // Vector3, EntityId, Matrix44Affine
#include "GameShared/GameClasses/Module/CgsEventQueue.h"             // CgsModule::EventQueue<T,N> (OutErrorQueue base)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerErrorEvent.h" // CgsSceneManager::ErrorEvent (OutErrorQueue element)
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h" // CgsSceneManager::VolumeInstanceId
#include "GameShared/GameClasses/SceneManager/CgsSceneQueryId.h"     // CgsSceneManager::SceneQueryId
// InSceneUpdateInterface has a single canonical home (kills the prior ODR double-definition
// that broke BrnRaceCarEntityModuleIO's standalone compile): the full slice -- incl.
// SetEntityPosition / SetVolumeInstanceTransform(VolumeInstanceId) / RemoveAllEntities, merged
// here from the former local minimal slice -- now lives in CgsSceneManagerIO_SceneUpdate.h.
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneUpdate.h" // CgsSceneManager::SceneManagerIO::InSceneUpdateInterface
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_Event.h"       // CgsSceneManager::SceneManagerIO::Event (single canonical definition)

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    // struct Event lives in CgsSceneManagerIO_Event.h (hoisted to a single definition to avoid the
    // C2011 redefinition once SceneUpdate.h began co-including EventAddForCollision.h).

    // Output event: the nearest hit of a line test.
    //
    // LAYOUT pinned by the producer OutSceneQueryResultsQueue<32768>::
    // AddTriangleCollisionLineTestNearestResult @ 0x828D1E90, which stores mPosition at rec+0x00,
    // mNormal at rec+0x10, the line-param float at rec+0x30 (lfs/stfs), and the two halves of the
    // packed +0x60 source word at rec+0x34 (material, high16) / rec+0x36 (group, low16). The
    // declared field order already lands mfLineParam at +0x30 because VolumeInstanceId is 8 bytes
    // (muId u64): 16+16+8 = +0x28, +mQueryId(4) = +0x2C, +mEntityId(4) = +0x30. mbIntersection
    // trails the tag pair at +0x38; sizeof rounds to 0x40 under alignas(16). The _AssertLayout()
    // below pins the producer-attested offsets.
    struct alignas(16) OutEventLineTestNearestResult : public Event
    {
        Vector3          mPosition;          // +0x00 (16B lane)
        Vector3          mNormal;            // +0x10 (16B lane)
        VolumeInstanceId mVolumeInstanceId;  // +0x20 (u64)
        SceneQueryId     mQueryId;           // +0x28
        EntityId         mEntityId;          // +0x2C
        f32              mfLineParam;         // +0x30
        u16              mu16MaterialTag;     // +0x34  (high 16 of source +0x60)
        u16              mu16GroupTag;        // +0x36  (low 16  of source +0x60)
        bool             mbIntersection;      // +0x38
        // +0x39..+0x3F trailing pad -> sizeof == 0x40
    };

    // Pin the producer-attested OutEventLineTestNearestResult layout (X360 0x828D1E90):
    // mfLineParam@+0x30, mu16MaterialTag@+0x34, mu16GroupTag@+0x36, sizeof==0x40. offsetof in a
    // never-called function keeps the offsets honest.
    inline void OutEventLineTestNearestResult_AssertLayout()
    {
        static_assert(offsetof(OutEventLineTestNearestResult, mfLineParam)     == 0x30, "mfLineParam @ +0x30");
        static_assert(offsetof(OutEventLineTestNearestResult, mu16MaterialTag) == 0x34, "mu16MaterialTag @ +0x34");
        static_assert(offsetof(OutEventLineTestNearestResult, mu16GroupTag)    == 0x36, "mu16GroupTag @ +0x36");
        static_assert(sizeof(OutEventLineTestNearestResult)                    == 0x40, "sizeof == 0x40");
    }

    // Output event: the hit/no-hit result of a fast double-sided line test. DWARF
    // CgsSceneManagerModuleIO.h:251 (OutEventLineTestFastDoubleSidedResult) -- only a query
    // handle and an intersection flag. The producing queue is
    // VariableEventQueue<32768,16>; the typed AddEvent<OutEventLineTestFastDoubleSidedResult>
    // @ 0x828D0780 forwards to the three-arg AddEvent with liSize == sizeof(EventT) == 8
    // (asm loads r6 = 8), which pins sizeof to 8: mQueryId(4) + mbIntersection(1) padded to 8.
    // No Vector3 -> default (4-byte) alignment, unlike OutEventLineTestNearestResult.
    struct OutEventLineTestFastDoubleSidedResult : public Event
    {
        SceneQueryId mQueryId;        // +0x00
        bool         mbIntersection;  // +0x04
        // +0x05..+0x07 trailing pad -> sizeof == 8
    };

    // Pin the X360-attested event size (typed AddEvent @ 0x828D0780 forwards sizeof == 8).
    inline void OutEventLineTestFastDoubleSidedResult_AssertLayout()
    {
        static_assert(offsetof(OutEventLineTestFastDoubleSidedResult, mQueryId)       == 0x00, "mQueryId @ +0x00");
        static_assert(offsetof(OutEventLineTestFastDoubleSidedResult, mbIntersection) == 0x04, "mbIntersection @ +0x04");
        static_assert(sizeof(OutEventLineTestFastDoubleSidedResult)                   == 8,    "sizeof == 8");
    }

    // ------------------------------------------------------------------------
    // OutErrorQueue<N> -- DWARF CgsSceneManagerModuleIO.h:141. The DWARF prints it as
    //   struct OutErrorQueue<128> : public EventQueue<CgsSceneManager::ErrorEvent,128> {}
    // i.e. a named EventQueue specialisation with NO members and NO methods of its own.
    // SceneManagerIO::OutputBuffer embeds it at N == 128 through the :304 typedef
    // OutSmErrorQueue, and the X360 OutputBuffer::Construct @0x828C7CA0 brings it up with
    //   CgsSceneManager::ErrorEvent,128>::Construct(this + 199744)   (@0x828C4B10)
    // whose `addi r30, r31, 0xC` proves the plain 12-byte BaseEventQueue header with no
    // padding before maEvents (ErrorEvent alignment <= 4).
    // ⚠️ FLAG (reported, not fixed here -- CgsSceneManagerErrorEvent.h is not this TU's file):
    // the console ErrorEvent element is 136 bytes, not the 16 the placeholder models. It is
    // pinned by this queue's console EXTENT inside OutputBuffer: the queue runs from +199744
    // to the next member (mTriangleCacheInterface @ +217164), i.e. 17420 bytes, and
    // 17420 == 12 (header) + 128 * 136. Nothing addresses the element by offset on the host,
    // so the size is cosmetic here -- but the placeholder's "16 bytes" is a wrong fact.
    // ------------------------------------------------------------------------
    template <s32 N>
    class OutErrorQueue : public CgsModule::EventQueue<CgsSceneManager::ErrorEvent, N>
    {
    };

    // MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
    // reconstructed by SceneFineLineTestQueue's own TU (DWARF home
    // CgsSceneManagerModuleIO.h). Size 16400 (DWARF-derived: see below).
    //
    // In BrnRaceCarEntityModuleIO.h, OutputBuffer_PostScene declares
    //   typedef InputBuffer_Query::InFineLineTestQueue SceneFineLineTestQueue;  // :78
    // and embeds it BY VALUE (mSceneFineLineTestQueue, :388). InFineLineTestQueue is
    //   typedef EventQueue<CgsSceneManager::SceneManagerIO::InEventLineTestFine,256> ...
    //   (CgsSceneManagerModuleIO.h:261). EventQueue<T,256> : BaseEventQueue<T> adds
    //   T maEvents[256]; BaseEventQueue<T> = { T* mpEvents; s32 miMaxLength; s32 miLength; }
    //   (12 bytes, padded to 16 for the 16-byte-aligned element). InEventLineTestFine
    //   (CgsSceneManagerIO_FineQuery.h:50) = Vector3 mLineStart(16) + Vector3 mLineEnd(16)
    //   + SceneQueryId(4) + EntityTypeFlags u32(4) + EntityId(4) + EExclusionMode enum(4)
    //   + VolumeTypeFlags u8(1) -> 49 bytes, alignas(16) (carries Vector3) -> 64 bytes.
    //   So sizeof == 16 + 256*64 = 16400 bytes. Carries Vector3, so alignas(16).
    // Per the stub rules the real EventQueue/BaseEventQueue generic + the
    // InEventLineTestFine element are intentionally NOT pulled in; a complete sized
    // blob unlocks the buffer (the IO header only takes &member). Full layout belongs
    // to this type's own ledger TU.
    struct alignas(16) SceneFineLineTestQueue
    {
        unsigned char maReserved[16400];
    };
}
}
