#ifndef BRN_TRIGGER_ENTITY_MODULE_H
#define BRN_TRIGGER_ENTITY_MODULE_H

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"        // CgsModule::ModuleSingleBuffered (real base)
#include "GameShared/GameClasses/Containers/CgsBitArray.h"                 // CgsContainers::BitArray<512> (mUsedTriggerList)
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerTypes.h"               // BrnWorld::Trigger, ETriggerTypeID, KF_PLANE_SEGMENT_TRIGGER_DEPTH
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleDebugComponent.h" // BrnWorld::TriggerEntityModuleDebugComponent
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"       // BrnWorld::TriggerEntityModuleIO buffers
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // TriggerManagementInputInterface (Process* params)

namespace CgsSceneManager { namespace SceneManagerIO { struct OutEventLineTestFineResult; } }

// ============================================================================
// BrnWorld::TriggerEntityModule -- the WORLD/physics-side trigger detector.
//
// The trigger entity module is the bridge between the physics scene and the gameplay
// trigger system. Each frame it:
//   * PreScene   : consumes the management input (add / remove trigger events) and turns
//                  each new trigger volume into a scene-manager dynamic-collision entity
//                  (a sphere / box / thin-box volume) registered with the broadphase.
//   * PostScene  : consumes the per-frame trigger query events (car-position line tests) and
//                  pushes a fine line-test request into the scene manager for each.
//   * PrePhysics : consumes the scene-manager query results (the fine line tests' fine-result
//                  intersection sets) and publishes the list of trigger entities each car
//                  overlapped this frame, which ultimately fires collectibles / events.
//
// SHAPE (DecFIGS DWARF BrnTriggerEntityModule.h, X360-offset-verified):
//   base  CgsModule::ModuleSingleBuffered                         (552 bytes)
//   +552  EPrepareStage mePrepareStage                            :117
//   +556  EReleaseStage meReleaseStage                            :118
//   +560  Trigger       mTriggers[KU_MAX_TRIGGERS]                :120  (stride 128, 512*128=65536)
//   +66096 BitArray<512> mUsedTriggerList                         :121  (8 * u64 = 64 bytes)
//   +66160 TriggerEntityModuleDebugComponent mTriggerEntityModuleDebugComponent :124
//
// The Construct/Prepare/Release/Destruct staging exactly mirrors the X360 bodies:
//   Construct (0x822D8ED0): base Construct, meReleaseStage=DONE, mePrepareStage=START,
//                           DebugComponent.Construct(this), clear mUsedTriggerList, mark new module.
//   Destruct  (0x822C42F8): base Destruct, DebugComponent.Destruct.
//   Prepare   (0x822A8E10): stage machine {START -> register DebugComponent, base Prepare; MANAGER -> done}.
//   Release   (0x822A8EC0): stage machine {<DONE -> base Release; done}.
// ============================================================================

namespace BrnWorld
{
    // KU_MAX_TRIGGERS == 512 (DWARF BrnTriggerEntityModule.h:36); the bit set and the trigger
    // array are both sized by it. The per-trigger entity index is packed into an EntityId whose
    // entity-index field must hold < KU_MAX_TRIGGERS.
    static const u16 KU_MAX_TRIGGERS = 512;

    class TriggerEntityModule : public CgsModule::ModuleSingleBuffered
    {
    public:
        // BrnTriggerEntityModule.h:56 -- Prepare() resumable stage machine.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START   = 0,
            E_PREPARESTAGE_MANAGER = 1,
            E_PREPARESTAGE_DONE    = 2,
        };

        // BrnTriggerEntityModule.h:63 -- Release() resumable stage machine.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        typedef TriggerEntityModuleIO::InputBuffer_PreScene   InputBuffer_PreScene;
        typedef TriggerEntityModuleIO::OutputBuffer_PreScene  OutputBuffer_PreScene;
        typedef TriggerEntityModuleIO::InputBuffer_PostScene  InputBuffer_PostScene;
        typedef TriggerEntityModuleIO::OutputBuffer_PostScene OutputBuffer_PostScene;
        typedef TriggerEntityModuleIO::InputBuffer_PrePhysics InputBuffer_PrePhysics;
        typedef TriggerEntityModuleIO::OutputBuffer_PrePhysics OutputBuffer_PrePhysics;

        // -- module lifecycle (X360 0x822D8ED0 / 0x822A8E10 / 0x822A8EC0 / 0x822C42F8) ----------
        void Construct() override;   // 0x822D8ED0
        bool Prepare() override;     // 0x822A8E10
        bool Release() override;     // 0x822A8EC0
        void Destruct() override;    // 0x822C42F8

        // Recovered accessors used by the debug component's RenderWorld. X360 inlined these as
        // direct offset reads (mTriggers @ this+560 stride 128, mUsedTriggerList @ this+66096).
        Trigger&                             GetTrigger(s32 liIndex)            { return mTriggers[liIndex]; }
        const Trigger&                       GetTrigger(s32 liIndex) const      { return mTriggers[liIndex]; }
        const CgsContainers::BitArray<512>&  GetUsedTriggerList() const         { return mUsedTriggerList; }

    private:
        // -- ProcessAddTriggerEvents (X360 0x822D8F48) ------------------------------------------
        // Drains the management input's "add trigger" queue. For each event: allocate a free
        // trigger slot (first clear bit in mUsedTriggerList), fill the Trigger record from the
        // event (type / position / radius / dimensions), build the matching rw::collision volume
        // (sphere / box / thin plane-segment box) and register it with the scene manager via the
        // pre-scene scene-input interface (AddDynamicVolume + AddEntity + AddVolumeInstance).
        void ProcessAddTriggerEvents(const TriggerEntityModuleIO::TriggerManagementInputInterface* lpInputInterface,
                                     OutputBuffer_PreScene::SceneInputInterface* lpOutSceneInputInterface);

        // ProcessRemoveTriggerEvents (X360 0x????) is a SEPARATE ledger entry -- declared-only here.
        void ProcessRemoveTriggerEvents(const TriggerEntityModuleIO::TriggerManagementInputInterface* lpInputInterface,
                                        OutputBuffer_PreScene::SceneInputInterface* lpOutSceneInputInterface);

        // -- ProcessTriggerQueryEvents (X360 0x82306BB0) ----------------------------------------
        // Drains the post-scene trigger-query input queue (one car-line query per event) and pushes
        // a fine line-test request into the scene manager's fine-line-test queue for each.
        void ProcessTriggerQueryEvents(const InputBuffer_PostScene* lpInput, OutputBuffer_PostScene* lpOutput);

        // -- ProcessSceneQueryResults (X360 0x822EEC78) -----------------------------------------
        // Drains the pre-physics scene-result queue; for each fine line-test result event
        // (type == E_OUTEVENT_LINETESTFINERESULT == 1) forwards to ProcessLineTestFineResult.
        void ProcessSceneQueryResults(const InputBuffer_PrePhysics* lpInput, OutputBuffer_PrePhysics* lpOutput);

        // -- ProcessLineTestFineResult (X360 0x822D9FF8) ----------------------------------------
        // Converts one fine line-test result (a set of trigger-entity intersections) into a
        // trigger-overlap output event: maps every hit EntityId back to its Trigger handle and
        // packs them into the module's pre-physics output interface.
        void ProcessLineTestFineResult(const CgsSceneManager::SceneManagerIO::OutEventLineTestFineResult* lpLineTestFineResult,
                                       OutputBuffer_PrePhysics* lpOutput);

        // offsetof tripwire -- pins the X360-proven member offsets at compile time.
        static void _AssertLayout();

        EPrepareStage                     mePrepareStage;                      // :117  (+552)
        EReleaseStage                     meReleaseStage;                      // :118  (+556)
        Trigger                           mTriggers[KU_MAX_TRIGGERS];          // :120  (+560,  stride 128)
        CgsContainers::BitArray<512>      mUsedTriggerList;                    // :121  (+66096)
        TriggerEntityModuleDebugComponent mTriggerEntityModuleDebugComponent;  // :124  (+66160)
    };
}

#endif // BRN_TRIGGER_ENTITY_MODULE_H
