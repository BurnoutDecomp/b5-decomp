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

namespace CgsSceneManager
{
    // ---- input-buffer event payloads -------------------------------------------------
    // The OverlapGeneration input buffer carries a small family of fixed-capacity event
    // queues -- the per-frame body mutations the SceneManagerModule pushes for this module
    // to apply to its SceneSweeper. The Process*BodyQueue passes pull the matching queue
    // out of the input structure and replay each event into the sweeper.
    //
    // Field offsets are read off the X360 Process*BodyQueue asm (the byte offsets each pass
    // loads out of the decoded event before calling SceneSweeper::Add/Remove/UpdateObject).
    // The element types' real home is this module's IO TU (CgsOverlapGenerationModuleIO,
    // not yet reconstructed); they are modelled here, by the offsets the asm attests, so
    // the Process passes compile against named members rather than raw offset pokes.
    namespace OverlapGenerationIO
    {
        // Culling-group tag -- the same u32 typedef the add-for-collision event family uses.
        typedef SceneManagerIO::InEventAddForCollision::CullingGroup CullingGroup;

        // ProcessAddBodyQueue element (X360 0x828C1D18): the sweeper add-body request.
        // The whole element doubles as the world AABB passed to SceneSweeper::AddObject.
        struct alignas(16) InAddBodyEvent
        {
            u8  maAABBox[0x24];     // +0x00 world AABB (passed whole to AddObject)
            u32 muObjectIndex;      // +0x24 sweeper object index
            u32 muVolumeHandle;     // +0x28 volume-instance index
            u8  maPad2C[0x04];      // +0x2C
            u64 mu64Body;           // +0x30 packed body word; high byte of hi word = CullingGroup
        };

        // ProcessRemoveBodyQueue element (X360 0x828C1E90).
        struct InRemoveBodyEvent
        {
            u64 mu64Body;           // +0x00 packed body word; high byte of hi word = CullingGroup
            u32 muObjectIndex;      // +0x08 sweeper object index
        };

        // ProcessUpdateBodyQueue element (X360 0x828C1DE8) / InputBuffer::UpdateBody producer
        // (X360 0x828BA430). 64-byte record, alignas(16). The whole 32-byte world AABB is
        // value-copied into mAabb (four ld/std qword pairs off the caller's box == the two
        // 16-byte rw::collision::AABBox corner rows), then the position lane, sweeper object
        // index and packed body word. mAabb is kept as a documented opaque 32-byte span: the
        // consumer (ProcessUpdateBodyQueue) passes &InUpdateBodyEvent whole to SceneSweeper::
        // UpdateObject as a const void* and never dereferences the box, and pulling the
        // SDKs/EATech rw::collision::AABBox definition in here would redefine rw::math::vpu::
        // Vector3 against the vendor one BrnCommonTypes.h already supplies. The producer's DWARF
        // names the tail field a VolumeInstanceId; it is the same packed 64-bit body word the
        // consumer reads whole as mu64Body and forwards to UpdateObject -- kept as u64 mu64Body
        // so the committed ProcessUpdateBodyQueue keeps compiling.
        struct alignas(16) InUpdateBodyEvent
        {
            u8      maAABBox[0x20]; // +0x00 world AABB (32B; whole box passed to UpdateObject)
            Vector3 mvPosition;     // +0x20 new world position lane (lvx128 +0x20)
            u32     muObjectIndex;  // +0x30 sweeper object index
            u8      maPad34[0x04];  // +0x34
            u64     mu64Body;       // +0x38 packed body word / volume-instance id
        };

        // ProcessForceNoPaddingEvent element (X360 AddEvent 0x828B89D0): a single 4-byte
        // object/volume-instance index the force-no-padding pass replays into the sweeper. No
        // interior fields are attested (stride 4, one 32-bit move), so it is a plain u32.
        // Distinct from the 8-byte SceneManagerIO::InEventForceNoPadding (a VolumeInstanceId).
        typedef u32 InForceNoPadding;

        // The per-frame body-mutation queues the input structure carries. The X360 Process*BodyQueue
        // passes pull these out of the input buffer and iterate them by GetLength()/GetEvent(index).
        // The update-body queue is the fixed-capacity (16384) derived EventQueue -- the queue whose
        // Construct the X360 emits out-of-line at 0x828C4BF0; it IS-A BaseEventQueue<InUpdateBodyEvent>
        // so GetUpdateBodyQueue()/ProcessUpdateBodyQueue/AddEvent keep working through the base.
        typedef CgsModule::BaseEventQueue<InAddBodyEvent>             InAddBodyQueue;
        typedef CgsModule::EventQueue<InUpdateBodyEvent, 16384>       InUpdateBodyQueue;
        typedef CgsModule::BaseEventQueue<InRemoveBodyEvent>          InRemoveBodyQueue;
        typedef CgsModule::BaseEventQueue<InForceNoPadding>          InForceNoPaddingQueue;

        // The overlap-generation module's input structure: the body-mutation request
        // queues the SceneManagerModule fills each frame. Its full layout (capacities +
        // any sibling queues) has its real home in this module's IO TU
        // (CgsOverlapGenerationModuleIO, not yet reconstructed); only the three queues
        // the body passes here read are modelled, by the accessors the X360 attests.
        struct InputBuffer
        {
            InAddBodyQueue    mAddBodyQueue;
            InUpdateBodyQueue mUpdateBodyQueue;
            InRemoveBodyQueue mRemoveBodyQueue;

            const InAddBodyQueue&    GetAddBodyQueue()    const { return mAddBodyQueue; }
            const InUpdateBodyQueue& GetUpdateBodyQueue() const { return mUpdateBodyQueue; }
            const InRemoveBodyQueue& GetRemoveBodyQueue() const { return mRemoveBodyQueue; }

            // Non-const accessor for the producing path -- the X360 UpdateBody body resolves the
            // update-body queue (sub_828B0230, == &mUpdateBodyQueue) and appends through it.
            InUpdateBodyQueue* GetUpdateBodyQueue() { return &mUpdateBodyQueue; }

            // Non-const add-body-queue accessor for the producing path -- the X360
            // SceneManagerModule::AddBody (0x828BA498) resolves the add-body queue (the
            // truncated CgsSceneMana(a2) callee, == &mAddBodyQueue since the queue is the
            // struct's first member) and appends the assembled event through it.
            InAddBodyQueue& GetAddBodyQueue() { return mAddBodyQueue; }

            // Push a per-frame body-position update request onto the update-body queue. @ 0x828BA430.
            // lpAabb is the world box (the X360 rw::collision::AABBox*); modelled as an opaque
            // 32-byte source span (see maAABBox above) block-copied whole into the queued event.
            void UpdateBody(u32 luIndex, const void* lpAabb, Vector3 lvPadding, VolumeInstanceId lVolumeInstanceID);
        };
    }

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
