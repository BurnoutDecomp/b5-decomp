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

        // ---- module virtual surface (DWARF CgsOverlapGenerationModule.h:42-115; every
        //      declaration below is spelled with the DWARF's own parameter types, and the
        //      trailing comment carries the DWARF's .cpp decl_line + the X360 address) ----
        void Construct() override;                                              // .cpp:44  @ 0x828D0460

        // ⭐ 2026-08-18 (wave Q5 cluster E2). WAS `Prepare(void*, void*)`, a placeholder that
        // made the SWEEPER call at CgsOverlapGenerationModule.cpp:97 a hard C2664 (the landed
        // SceneSweeper::Prepare takes `u8*, rw::BitTable::Storage*`) and kept this whole TU
        // off the build. The DWARF spells it `virtual bool Prepare(uint8_t*, BitTable*)`; the
        // second type is narrowed to the BACKING STORE exactly as CgsSceneSweeper.h:231 and
        // CgsCullingGroupManager.h:53 already do (the console stores + walks the store, not a
        // handle object). The X360 body @0x828CB6B0 forwards both arguments UNCHANGED to
        // SceneSweeper::Prepare (0x828CB734 `mr r5,r28` / 0x828CB738 `mr r4,r29`), which is
        // what pins them: whatever type the sweeper takes, this takes.
        // ✅ CALL-SITE RESOLVED: the SceneManagerModule::Prepare site now passes
        // `mCullingGroupManager.GetVolumeInstanceCullingGroup()` (CgsSceneManagerModule.cpp
        // :412), not `&mCullingGroupManager`. That accessor is address-identical to the old
        // argument (CgsCullingGroupManager.h:58 -- maVolumeInstanceCullingGroup is at
        // +0x0000), so the repair was a retype, not a behaviour change.
        virtual bool Prepare(u8* lpaVolumeInstanceCullingGroup,
                             rw::BitTable::Storage* lpCullingTable);            // .cpp:77  @ 0x828CB6B0

        bool Release() override;                                                // .cpp:138 @ 0x828CB798

        // DWARF-attested override with NO reconstructed body anywhere in the tree: the only
        // definition is the WorldLinkStubs.cpp boot gate. Left declared (the DWARF lists it)
        // and left gated -- retiring that gate without a body is an instant LNK2019, because
        // SceneManagerModule::Destruct @CgsSceneManagerModule.cpp:285 calls it.
        void Destruct() override;                                               // .cpp:196 (NO BODY -- gate)

        // ⭐ 2026-08-18 (wave Q5 cluster E2). WAS `void Update() override;` -- a placeholder
        // that does NOT exist on the console class. The DecFIGS DWARF lists exactly one
        // Update on OverlapGenerationModule and it is `virtual void Update(const InputBuffer*)`
        // (.cpp:215); the no-argument module-framework Update stays where it is, on the base
        // (CgsModule::ModuleSingleBuffered::Update, CgsModuleSingleBuffered.cpp:159, which
        // remains the final overrider of that vtable slot). Replacing the declaration is what
        // lets the real 0x828CB878 body land: it is the four-pass input-queue replay.
        virtual void Update(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // .cpp:215 @ 0x828CB878

        // The per-frame broadphase: run the sweeper, then publish its colliding pairs onto
        // the output buffer under a write lock. Both parameter types are the DWARF's own and
        // both already match the live call site (CgsSceneManagerModule.cpp:558).
        void GenerateOverlaps(OverlapGenerationIO::OutputBuffer* lpOutputBuffer,
                              const EntityManager* lpEntityManager);            // .cpp:244 @ 0x828D5C08

    private:
        // ---- per-frame input replay passes (this TU) -----------------------------------
        void ProcessAddBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);     // .cpp:271 @ 0x828C1D18
        void ProcessUpdateBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // .cpp:305 @ 0x828C1DE8
        void ProcessRemoveBodyQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // .cpp:336 @ 0x828C1E90
        void ProcessForceNoPaddingQueue(const OverlapGenerationIO::InputBuffer* lpInputBuffer);  // .cpp:365 @ 0x828B56D8

        // ---- the output pass (this TU) --------------------------------------------------
        void OutputCollidingPairs(OverlapGenerationIO::OutputBuffer* lpOutputBuffer);        // .cpp:396 @ 0x828C1F58

        // ---- members (DWARF order; offsets pinned to the X360 Construct asm) ------------
        EPrepareStage mePrepareStage;  // X360 +0x228 (switched by Prepare)
        EReleaseStage meReleaseStage;  // X360 +0x22C (switched by Release)

        SceneSweeper  mSweeper;        // X360 +0x230 (the sweep-and-prune broadphase)

        // Per-culling-group live-body counters, bumped/decremented as bodies are
        // added/removed (X360 +0xDC6F0; the trailing member of the module).
        u16 mau16NumEntitiesPerType[KI_NUM_ENTITY_TYPES];

        // ---- DELIBERATELY NOT DECLARED (honest gap, not an oversight) --------------------
        // The DWARF class also carries eight public producer/accessor members at
        // CgsOverlapGenerationModule.h:101-135 -- AddBody(uint32_t, AABBox*,
        // InEventAddForCollision::CullingGroup, rw::physics::BodyState), RemoveBody(uint32_t),
        // UpdateBody(uint32_t, AABBox*), SetVolumeInstanceCullingGroup(uint32_t,
        // InEventAddForCollision::CullingGroup), SetCullingGroupPair(CullingGroup, CullingGroup,
        // bool), SetPadding(uint32_t, Vector3), ForceNoPadding(uint32_t) and
        // GetOverlappingPairQueue(). NONE of them has a reconstructed body, an identity.json
        // row or a caller in this tree (the console reaches the same effects through
        // OverlapGenerationIO::InputBuffer's four producers, which ARE bodied, in
        // CgsOverlapGenerationModuleIO_InputBuffer.cpp). Declaring them here would add eight
        // link holes that only bite once someone calls them; they belong to whoever recovers
        // their addresses. Recorded so the next reader knows the class shape is partial ON
        // PURPOSE.
    };
}
