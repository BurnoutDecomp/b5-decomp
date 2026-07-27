#ifndef GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H
#define GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H

// ============================================================================
// GameSource/World/EntityModules/WorldEntityModule/PVSModule/BrnPVSModule.h
//
// BrnWorld::PVSModule -- the world's PVS (Potentially-Visible-Set) module. It is a
// single-buffered module: each frame it consumes GetZoneRequest events (which zone
// is the listener in) and produces GetZoneResponse events (the per-zone visibility
// replies), and it owns a game-data request interface used to stream the PVS data.
//
// It derives from CgsModule::ModuleSingleBufferedTemplate<InputBuffer, OutputBuffer>
// (the PVS IO buffers live in SharedIO/BrnPVSModuleEvents.h). The canonical home for
// this header is the path baked into the X360 asserts:
//   d:\p4\b5_main\...\worldentitymodule\PVSModule/BrnPVSModule.h
//
// X360 ARTIST functions homed by this class:
//   PVSModule()              @ 0x827E4CC8  ctor
//   Construct()              @ 0x822C3F50
//   Prepare()                @ 0x82302E00
//   Update()                 @ 0x822EE050
//   Release()                @ 0x822A8AD8
//   GetInputInterface()      @ 0x822BAF78  (BrnPVSModule.h:122 assert)
//   GetOutputInterface()     @ 0x822BAFF0  (BrnPVSModule.h:130 assert)
//   GetGameDataRequestInt()  @ 0x822BB068  (BrnPVSModule.h:138 assert)
//
// X360 MEMBER MAP (byte offsets into the module object, all attested by the four
// bodies above; the sub-object base is the ModuleSingleBufferedTemplate with its two
// embedded IO buffers):
//   +0x1D70  s32                        mePrepareStage   (Prepare's switch variable)
//   +0x1D74  s32                        meReleaseStage   (Release's switch variable)
//   +0x1D78  ResourcePtr<ZoneList>      mZoneList        (Prepare: CreateFromHandle;
//                                                         Update: operator->)
//   +0x1D98  EventReceiverQueue<512,16> mReceiverQueue   (Construct seeds miCapacity
//                                                         512 / miAlignment 16 /
//                                                         mpBuffer = member+0x18)
//   +0x1FB0  bool                       mbCurrentZoneOnly (Construct clears it; Update
//                                                          tests it -- see below)
// The x64 build keeps these BY NAME (semantic parity); the absolute offsets are X360
// documentation only.
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBufferedTemplate.h"  // ModuleSingleBufferedTemplate
#include "GameShared/GameClasses/Module/CgsBaseEventReceiverQueue.h"        // EventReceiverQueue<512,16>
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"          // ResourcePtr<T>
#include "GameShared/GameClasses/SceneManager/Zones/ZoneList.h"             // CgsSceneManager::ZoneList
#include "GameSource/World/EntityModules/WorldEntityModule/PVSModule/SharedIO/BrnPVSModuleEvents.h" // PVSIO::InputBuffer/OutputBuffer
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"           // RequestInterface<512>

namespace BrnWorld
{
    class PVSModule
        : public CgsModule::ModuleSingleBufferedTemplate<PVSIO::InputBuffer, PVSIO::OutputBuffer>
    {
    public:
        // Prepare's stage machine (X360 Prepare @0x82302E00 switches on mePrepareStage with a
        // 5-entry jump table; the "Invalid Stage\n" default fires at BrnPVSModule.cpp:200).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START   = 0,   // nothing done yet (Construct's seed)
            E_PREPARESTAGE_MODULE  = 1,   // driving the ModuleSingleBuffered base Prepare
            E_PREPARESTAGE_REQUEST = 2,   // post the LoadPVS game-data request
            E_PREPARESTAGE_WAITING = 3,   // waiting for the type-58 zone-list reply
            E_PREPARESTAGE_DONE    = 4
        };

        // Release's stage machine (X360 Release @0x822A8AD8; "Invalid Stage\n" at :257 for >= 3).
        enum EReleaseStage
        {
            E_RELEASESTAGE_START  = 0,
            E_RELEASESTAGE_MODULE = 1,
            E_RELEASESTAGE_DONE   = 2     // Construct's seed
        };

        // X360 0x827E4CC8. Chains the base (which constructs the two RWMutexes), sets the
        // PVSModule vtable, clears the PVS flag byte and default-constructs the zone-list
        // resource pointer (the "three zero words then three self-pointers then a zero word"
        // the ctor asm writes at +0x1D78 IS BaseResourcePtr's empty state).
        PVSModule();

        // X360 0x822C3F50. Chains ModuleSingleBuffered::Construct, seeds the two stage
        // machines (prepare START / release DONE), constructs the 512-byte game-data
        // receiver queue and clears the current-zone-only flag.
        void Construct() override;

        // X360 0x82302E00. Stage machine: base module prepare -> post LoadPVS -> wait for the
        // type-58 reply -> bind the ZoneList resource pointer + publish the total zone count.
        bool Prepare() override;

        // X360 0x822A8AD8. Unwinds the base module and resets the two stage machines.
        bool Release() override;

        // X360 0x822EE050. THE PVS QUERY: drains the GetZoneRequest queue, resolves each
        // request position to a Zone via the loaded ZoneList, and publishes one
        // GetZoneResponse per request (centre zone + its safe/unsafe neighbours, with the
        // per-zone render/immediate flags and the velocity-weighted zone weights).
        void Update() override;

        // X360 0x822BAF78. Returns the module's input data structure (the GetZoneRequest queue
        // buffer). The base GetInputStructure() is the lpInputBuffer the asm reads; the
        // SafeGetInputStructure() tripwires ("lpBuffer != NULL" / "lpInputBuffer") fire on null.
        PVSIO::InputBuffer* GetInputInterface();

        // X360 0x822BAFF0. Returns the module's output data structure (the GetZoneResponse queue
        // buffer). Tripwires "lpBuffer != NULL" / "lpOutputBuffer" on null.
        PVSIO::OutputBuffer* GetOutputInterface();

        // X360 0x822BB068. Returns the game-data request interface embedded in the output buffer
        // (asm: GetOutputStructure() + 0x1718 == &OutputBuffer::mGameDataRequestInterface).
        // Tripwires "lpBuffer != NULL" / "lpOutputBuffer" on null.
        BrnResource::GameDataIO::RequestInterface<512>* GetGameDataRequestInt();

    private:
        // The receiver-queue event TYPE the game-data module posts when the PVS zone list has
        // been acquired (GameDataModule::ProcessInternalAcquireResponse case 58 ->
        // PostGameDataResponse; Prepare asserts "Invalid event received\n" on anything else).
        static const s32 KI_EVENT_GET_PVS = 58;

        // The pool the PVS zone list is loaded into (Prepare passes 3 to LoadPVS -- the
        // open-world graphics pool, the same id WorldEntityModule::Prepare uses).
        static const s32 KI_PVS_POOL_ID = 3;

        s32 mePrepareStage;   // X360 +0x1D70
        s32 meReleaseStage;   // X360 +0x1D74

        // The loaded PVS zone list (bound from the type-58 reply's ResourceHandle).
        CgsResource::ResourcePtr<CgsSceneManager::ZoneList> mZoneList;   // X360 +0x1D78

        // The game-data reply queue the LoadPVS request names as its receiver.
        CgsModule::EventReceiverQueue<512, 16> mReceiverQueue;           // X360 +0x1D98

        // X360 +0x1FB0, cleared by Construct. When set, Update answers every request with the
        // centre zone ALONE (miNumZones == 1) instead of the centre plus its neighbours -- the
        // "no PVS expansion" path. FLAG: no writer for this byte was recovered in this slice
        // (Construct is the only attested store); the name describes the behaviour Update
        // selects on it, which IS attested (0x822EE138 lbz +0x1FB0 -> the single-zone branch).
        bool mbCurrentZoneOnly;                                          // X360 +0x1FB0
    };
}

#endif // GAMESOURCE_WORLD_ENTITYMODULES_WORLDENTITYMODULE_PVSMODULE_BRNPVSMODULE_H
