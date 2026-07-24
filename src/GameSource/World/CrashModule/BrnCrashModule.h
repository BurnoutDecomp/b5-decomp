#pragma once

// Canonical (DWARF) home for BrnWorld::CrashModule (World/CrashModule/BrnCrashModule.h /
// BrnCrashModule.cpp). The crash module is a CgsModule::ModuleSingleBuffered that tracks every
// in-progress race-car crash and crashing-traffic vehicle the world is currently animating: it
// owns a small Array<RaceCarCrash,8> of active race-car wrecks, a larger Array<TrafficCrash,160>
// of crashing-traffic entries, per-traffic-vehicle bit masks, per-player crashing-traffic sets,
// and a recycled-traffic event queue mirrored from the vehicle manager. Class shape (member
// order/types/names, the two prepare/release stage enums, the nested FrozenTrafficTransform, and
// the method set) is taken verbatim from the DecFIGS DWARF (BrnCrashModule.h); behaviour of the
// three out-of-line methods reconstructed here comes from the X360 ARTIST spine.
//
// MINIMAL-COMPLETE BEHAVIOURAL SLICE: this header declares the full DWARF member set (so the
// embedded sub-objects land at their natural offsets and the slice's three methods access them
// by name) but only DEFINES the three methods the X360 emitted for this TU (BrnCrashModule.cpp):
//   CrashModule()                          @ 0x827DE960  -- the default ctor: base init + mark the
//                                                           two crash arrays unconstructed.
//   FindCrashForRaceCar(EActiveRaceCarIndex) const  @ 0x827C6AD8  -- linear search of mRaceCarCrashes
//                                                           for the crash owned by a race-car slot.
//   WillTrafficVehicleBeRecycledNextFrame(u16)      @ 0x827BBB10  -- is a traffic vehicle queued for
//                                                           recycling this frame.
// The remaining DWARF methods (Construct/Prepare/Release/PreSceneUpdate/... etc.) are declared so
// the class is the canonical CrashModule, but their bodies belong to their own ledger TUs.
//
// X360 MEMBER OFFSETS (authoritative, from the ctor / accessor asm):
//   vtable                 @ +0x000
//   base RWMutex (input)   @ +0x010   (ModuleSingleBuffered::mInputMutex  -- ctor RWMutex(+0x10))
//   base RWMutex (output)  @ +0x118   (ModuleSingleBuffered::mOutputMutex -- ctor RWMutex(+0x118))
//   mRaceCarCrashes        @ +0x230   (Array<RaceCarCrash,8>; count word @ +0x2F0 == +0x230+0xC0,
//                                      ctor stores -1 == Array::MarkUnconstructed)
//   mTrafficCrashes        @ +0x2F8   (Array<TrafficCrash,160>; count word @ +0x7F8 == +0x2F8+0x500,
//                                      ctor stores -1 == Array::MarkUnconstructed)
//   mRecycledTrafficQueue  @ +0x1530  (EventQueue<TrafficRemovedEvent,25>; miLength @ +0x1538)
// The PC build reproduces these offsets via the natural layout of the real (reconstructed) member
// types; they are documented rather than static_asserted because the base RWMutex/DataBuffer
// sub-object widths are platform ABI dependent.
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsModuleSingleBuffered.h"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"                          // CgsModule::IOBuffer                 // CgsModule::ModuleSingleBuffered
#include "GameShared/GameClasses/Containers/CgsArray.h"                            // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsSet.h"                              // CgsContainers::Set<T,N>
#include "GameShared/GameClasses/Containers/CgsFastBitArray.h"                     // CgsContainers::FastBitArray<N>
#include "GameShared/GameClasses/SceneManager/CgsVolumeInstanceId.h"              // CgsSceneManager::VolumeInstanceId
#include "BrnCommonTypes.h"                                                        // EntityId, Matrix44Affine
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex
#include "GameSource/World/CrashModule/BrnRaceCarCrash.h"                          // BrnWorld::RaceCarCrash
#include "GameSource/World/CrashModule/BrnTrafficCrash.h"                          // BrnWorld::TrafficCrash
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleTrafficIOInterfaces.h" // CrashIO::TrafficInputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h" // VehicleManagerOutputInterface
#include "GameSource/Physics/VehicleManager/BrnVehicleConstants.h"                // BrnPhysics::Vehicle::eCrashTrafficType

namespace BrnWorld
{
    // BrnCrashModule.h:37-42 -- crash-module capacity constants (DWARF).
    const u32 KU_MAX_RACE_CAR_CRASHES        = 8;
    const u32 KU_MAX_TRAFFIC_CRASHES         = 160;
    const u32 KU_MAX_FROZEN_TRAFFIC_VEHICLES = 160;

    // BrnCrashModule.h:182 -- the world's crash module (single-buffered).
    // ------------------------------------------------------------------------
    // BrnWorld::CrashModuleIO::OutputBuffer_PostScene -- the crash module's
    // post-scene output the entity-module spines bridge from
    // (WorldModule::EntityModulePostSceneUpdate @0x827C3C58).
    //
    // FLAG (minimal-complete slice, size NOT X360-attested): the real aggregate is
    // the crash event queues (CrashIO::* element homes are committed); that layout
    // belongs to the crash IO TU and is NOT recovered here. GROW when that TU lands.
    // ------------------------------------------------------------------------
    namespace CrashModuleIO
    {
        struct OutputBuffer_PostScene : public CgsModule::IOBuffer
        {
            u8 maDeferredPayload[16];   // deferred interior (see FLAG)
        };
    }

    struct CrashModule : public CgsModule::ModuleSingleBuffered
    {
        // BrnCrashModule.h:254 -- "no crash" sentinel returned by the Find* searches (== -1 as u32).
        static const u32 KU_INVALID_CRASH = 0xFFFFFFFFu;

        // BrnCrashModule.h:222 -- prepare/release progress (the module's staged setup/teardown).
        enum EPrepareStage
        {
            E_PREPARESTAGE_START   = 0,
            E_PREPARESTAGE_MANAGER = 1,
            E_PREPARESTAGE_DONE    = 2,
        };

        // BrnCrashModule.h:229
        enum EReleaseStage
        {
            E_RELEASESTAGE_START   = 0,
            E_RELEASESTAGE_MANAGER = 1,
            E_RELEASESTAGE_DONE    = 2,
        };

        // BrnCrashModule.h:239 -- a frozen traffic vehicle's saved transform.
        struct FrozenTrafficTransform
        {
            u16            muVehicleId;   // :241
            Matrix44Affine mTransform;    // :243
        };

        // X360 0x827DE960. Default ctor: base ModuleSingleBuffered init (the two RWMutexes), then
        // mark the two crash arrays unconstructed (count = -1). Called by WorldModule::WorldModule.
        CrashModule();

        // NOTE: the DWARF declares the full ModuleSingleBuffered override set
        // (Construct/Prepare/Release/Destruct), the PreSceneUpdate/PostPhysicsUpdate entry points
        // and the ~25 private crash-handling helpers. They are intentionally NOT declared in this
        // behavioural slice: their signatures reference crash-module IO buffer / update-set types
        // whose headers are owned by their own ledger TUs. Those TUs additively grow this class
        // with their own declarations; this slice declares only the three methods it defines.

    private:
        // X360 0x827C6AD8. Linear search of mRaceCarCrashes for the crash record owned by the
        // given active-race-car slot; returns its index, or KU_INVALID_CRASH (-1) if none.
        u32 FindCrashForRaceCar(EActiveRaceCarIndex leActiveRaceCarIndex) const;

        // X360 0x827BBB10. True when the traffic vehicle (luVehicle, < KU_MAX_TOTAL_TRAFFIC == 600)
        // is present in mRecycledTrafficQueue, i.e. it will be removed/recycled next frame.
        bool WillTrafficVehicleBeRecycledNextFrame(u16 luVehicle);

        // ----- members (DWARF order/types; offsets in the banner above) -----
        EPrepareStage mePrepareStage;                                     // :257
        EReleaseStage meReleaseStage;                                     // :258

        Array<RaceCarCrash, 8>   mRaceCarCrashes;                         // :260  (@ +0x230)
        Array<TrafficCrash, 160> mTrafficCrashes;                         // :261  (@ +0x2F8)

        CgsContainers::FastBitArray<8>   mCrashingRaceCars;               // :263
        CgsContainers::FastBitArray<601> mCrashingTraffic;                // :264
        CgsContainers::FastBitArray<601> mCrashingNetworkTraffic;         // :265
        s8                               maiSlammedTrafficOwners[601];    // :266

        Set<u16, 160>                    maCrashingTrafficForPlayers[8];  // :268

        EActiveRaceCarIndex meLocalActiveRaceCarIndex;                    // :270
        f32                 mfPlayerCrashTime;                            // :271
        bool                mbFastCrashesForAI;                           // :272
        bool                mbIsOnlineGameMode;                           // :273
        bool                mbIsShowtimeGameMode;                         // :274
        bool                mbClearUpEnabled;                             // :275
        s8                  miNumCrashExtensions;                         // :276
        u8                  muNumFramesBeforeStateChange;                 // :277
        bool                mbNeedToSendEndingMessage;                    // :278
        bool                mbIsInAGameMode;                              // :279

        BrnPhysics::Vehicle::VehicleManagerOutputInterface::RemovedTrafficEventQueue
                            mRecycledTrafficQueue;                        // :283  (@ +0x1530)

        CrashIO::TrafficInputInterface::TrafficVehicleBits mTrafficRenderedLastFrame;      // :285
        CrashIO::TrafficInputInterface::TrafficVehicleBits mTrafficFarFromCameraLastFrame; // :286
    };
}
