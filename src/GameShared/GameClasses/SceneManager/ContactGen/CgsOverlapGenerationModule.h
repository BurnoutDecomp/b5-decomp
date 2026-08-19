#pragma once

// ===========================================================================
// CgsSceneManager::OverlapGenerationModule
//   Home: GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModule.{h,cpp}
//
// The broad-phase overlap-generation stage of the scene manager's contact
// pipeline. It owns a sweep-and-prune SceneSweeper and feeds it the per-frame
// add/update/remove/force-no-padding body requests decoded from its input buffer,
// emitting the candidate overlapping volume-instance pairs the OverlapCullingModule
// then narrows. Derives from CgsModule::ModuleSingleBuffered and is embedded BY
// VALUE in CgsSceneManager::SceneManagerModule (mOverlapGenerator, X360 +0x290).
//
// Class shape (base, enums, member set/order, virtual surface) recovered from the
// DecFIGS DWARF (CgsOverlapGenerationModule.h); member offsets + the reconstructed
// bodies are pinned to the X360 ARTIST asm (Construct 0x828D0460, Prepare 0x828CB6B0,
// Release 0x828CB798, ProcessAddBodyQueue 0x828C1D18, ProcessUpdateBodyQueue
// 0x828C1DE8, ProcessRemoveBodyQueue 0x828C1E90).
// ===========================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"  // Vector3

#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"  // CgsModule::ModuleSingleBuffered base
#include "GameShared/GameClasses/Module/CgsEventQueue.h"            // CgsModule::BaseEventQueue / EventQueue<T,N> (input queues)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsSceneSweeper.h"  // CgsSceneManager::SceneSweeper (by-value member)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h"  // InEventAddForCollision::CullingGroup
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"  // CgsSceneManager::VolumeInstanceId (UpdateBody arg)
#include "GameShared/GameClasses/SceneManager/ContactGen/CgsOverlapGenerationModuleIO.h"  // ⭐ the REAL home of namespace OverlapGenerationIO (was a stand-in in this file)

namespace CgsSceneManager
{
    // ---- input-buffer event payloads + the InputBuffer ITSELF: NOT HERE ----------------
    // ⭐ 2026-08-18 (wave Q5 cluster D1, OverlapGenerationModuleIO owner). The 102-line
    // OverlapGenerationIO stand-in that used to sit here -- InAddBodyEvent /
    // InUpdateBodyEvent / InRemoveBodyEvent / a `typedef u32 InForceNoPadding`, the four
    // queue typedefs and a THREE-QUEUE InputBuffer built out of storage-less
    // BaseEventQueue<T> bases -- is RETIRED. Its own banner already said the real home was
    // "this module's IO TU (CgsOverlapGenerationModuleIO, not yet reconstructed)"; that TU
    // now exists, and the include below is it.
    //
    // This was not a cosmetic move. The stand-in was WRONG in ways that mattered:
    //   * it modelled the world box as `u8 maAABBox[0x24]` (36 bytes), which swallowed the
    //     culling-group word the producer stores at +0x20 and pushed every later field;
    //   * `muVolumeHandle` @+0x28 is really `rw::physics::BodyState meBodyState` (the
    //     STATIC/FROZEN/ACTIVE tag SceneSweeper::AddObject dispatches on) and `mu64Body`
    //     @+0x30 is really a `VolumeInstanceId` -- which is exactly why this TU's own
    //     ProcessAddBodyQueue/ProcessUpdateBodyQueue stopped compiling against the landed
    //     SceneSweeper signatures;
    //   * the InputBuffer was missing mForceNoPaddingQueue entirely and gave the other
    //     three no inline storage, so a Construct through it would have left mpEvents NULL
    //     on a 2,359,888-byte console buffer -- a silent corruption, not a compile error.
    // The real layout is pinned to InputBuffer::Construct @0x828CB918 and
    // DestroyIOBuffer<InputBuffer> @0x828C53F0; see CgsOverlapGenerationModuleIO.h.
    //
    // ⚠️ OWNERSHIP: this file belongs to the OverlapGenerationModule cluster, not to D1.
    // D1 changed exactly this block (delete + the include on the next line) because a
    // second definition of OverlapGenerationIO::InputBuffer cannot coexist with the real
    // one -- CgsSceneManagerModule.cpp includes BOTH this header and
    // ContactGen/CgsContactGenerationIO.h. Nothing else in this header was touched.

    class OverlapGenerationModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // Number of distinct culling-group "types" the per-type entity counters track.
        static const s32 KI_NUM_ENTITY_TYPES = 16;

        // CgsOverlapGenerationModule.h:50 (DWARF) -- the Prepare state machine stage.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START   = 0,
            E_PREPARESTAGE_MANAGER = 1,
            E_PREPARESTAGE_SWEEPER = 2,
            E_PREPARESTAGE_DONE    = 3,
        };

        // CgsOverlapGenerationModule.h:58 (DWARF) -- the Release state machine stage.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_SWEEPER = 1,
            E_RELEASESTAGE_MANAGER = 2,
            E_RELEASESTAGE_DONE    = 3,
        };

        // ---- module virtual surface (DWARF; bodies split across this + sibling TUs) ----
        void Construct() override;                                  // @ 0x828D0460 (this TU)
        bool Prepare(void* lpCullingGroupManager, void* lpCullingTable);  // @ 0x828CB6B0 (this TU)
        bool Release() override;                                    // @ 0x828CB798 (this TU)
        void Destruct() override;                                   // (sibling TU)
        void Update() override;                                     // (sibling TU)

        // Generate the candidate overlap pairs for this frame (sibling TU).
        void GenerateOverlaps(void* lpOutputBuffer, const void* lpEntityManager);

    private:
        // ---- per-frame input replay passes (this TU) -----------------------------------
        void ProcessAddBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);     // @ 0x828C1D18
        void ProcessUpdateBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // @ 0x828C1DE8
        void ProcessRemoveBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // @ 0x828C1E90

        // ---- members (DWARF order; offsets pinned to the X360 Construct asm) ------------
        EPrepareStage mePrepareStage;  // X360 +0x228 (switched by Prepare)
        EReleaseStage meReleaseStage;  // X360 +0x22C (switched by Release)

        SceneSweeper  mSweeper;        // X360 +0x230 (the sweep-and-prune broadphase)

        // Per-culling-group live-body counters, bumped/decremented as bodies are
        // added/removed (X360 +0xDC6F0; the trailing member of the module).
        u16 mau16NumEntitiesPerType[KI_NUM_ENTITY_TYPES];
    };
}
