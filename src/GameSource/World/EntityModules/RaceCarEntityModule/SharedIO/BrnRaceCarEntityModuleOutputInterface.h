#pragma once

// Race-car entity-module output interface. Member names/types and the EMessageType
// enum are from the DecFIGS DWARF (BrnRaceCarEntityModuleOutputInterface.h), X360-gated.
// Originally this header held only the AudioCarDataLoadedEvent payload; it now also
// homes the five output-interface support structs (the active/global race-car output
// interfaces, the player-reset interface, BoostOutputInfo, CarsInTheRaceData) and the
// EActiveRaceCarEngineState enum. All by-value members resolve to complete in-tree types:
//   RaceCarState        -> BrnVehicleEvents.h (complete 1120-byte struct)
//   RwRGBAReal          -> rw/core/base/ostypes.h (16-byte, 4 floats)
//   CgsWorld::WorldMap2D-> CgsWorldMap2D.h
//   BrnWorld::WorldRegion -> SharedClasses/World/BrnWorldRegion.h
//   EGlobalRaceCarIndex / EActiveRaceCarIndex -> GameSource/BurnoutConstants.h
//   BrnWorld::EBoostType-> RaceCarEntityModule/Boost/BrnBoostType.h
#include "types.hpp"                                                // s8/s32/u16/u32/f32
#include "BrnCommonTypes.h"                                         // CgsID, Vector3, Vector4, EntityId
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"    // CgsModule::Event
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT (the header-inline accessors below)
#include "GameShared/GameClasses/Containers/CgsArray.h"             // Array<T,N>
#include "GameShared/GameClasses/Containers/CgsBitArray.h"          // CgsContainers::BitArray<N>
#include "GameShared/GameClasses/System/Resource/CgsResourceHandle.h" // CgsResource::ResourceHandle
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"  // CgsResource::ResourcePtr<T>
#include "GameShared/GameClasses/World/CgsWorldMap2D.h"             // CgsWorld::WorldMap2D
#include "SharedClasses/World/BrnWorldRegion.h"                     // BrnWorld::WorldRegion
#include "GameSource/BurnoutConstants.h"                            // EActiveRaceCarIndex, EGlobalRaceCarIndex
#include "GameSource/World/EntityModules/RaceCarEntityModule/Boost/BrnBoostType.h" // BrnWorld::EBoostType
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleEvents.h"           // BrnPhysics::Vehicle::RaceCarState (complete)
#include "rw/core/base/ostypes.h"                                   // RwRGBAReal (complete 16-byte)

namespace BrnResource { struct VehicleListEntry; }
namespace BrnPhysics { namespace Deformation { struct StreamedDeformationSpec; } }
namespace BrnGameState { namespace GameStateModuleIO { enum EPlayerScoringIndex : s32; } }
namespace BrnWorld { struct RaceCar; }

namespace BrnWorld
{
namespace RaceCarEntityModuleIO
{
    // Output event: a race car's audio data has finished (un)loading, or a (un)load
    // has been requested. The owning queue is 16-capacity; the event carries no SIMD
    // members so it takes its natural 8-byte alignment (CgsID is 64-bit).
    struct AudioCarDataLoadedEvent : public CgsModule::Event
    {
        enum EMessageType : s32
        {
            E_NONE                = 0,
            E_REQUEST_LOAD_DATA   = 1,
            E_DATA_IS_LOADED      = 2,
            E_REQUEST_UNLOAD_DATA = 3,
            E_DATA_IS_UNLOADED    = 4
        };

        EMessageType                         meMessageType;
        const BrnResource::VehicleListEntry* mpVehicleListEntry;
        CgsID                                mAssetID;
        u8                                   miActiveRaceCarIndex;
        bool                                 mbIsPlayer;
    };

    // DWARF :71 -- the player car's engine running-state. Full DWARF enumerator names.
    enum EActiveRaceCarEngineState : s32
    {
        E_ACTIVE_RACE_CAR_ENGINE_STATE_OFF      = 0,
        E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING = 1,
        E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING  = 2,
        E_ACTIVE_RACE_CAR_ENGINE_STATE_STOPPING = 3,
        E_ACTIVE_RACE_CAR_ENGINE_STATE_COUNT    = 4
    };

    // DWARF :157 -- per-car boost-bar telemetry published to the HUD/audio. meBoostType
    // is BrnWorld::EBoostType (authoritative DWARF :177), NOT BrnNetwork::EBoostType.
    struct BoostOutputInfo
    {
        bool               mbIsBoosting;            // :159
        bool               mbIsInAir;               // :160
        bool               mbIsOncoming;            // :161
        bool               mbIsDrifting;            // :162
        bool               mbNearMiss;              // :163
        bool               mbIsBlueMode;            // :164
        bool               mbWasChainJustCompleted; // :165
        bool               mbHoldingBoostButton;    // :166
        bool               mbIsTailgating;          // :167
        bool               mbTrafficCheck;          // :168
        bool               mbAllowedToBoost;        // :169
        bool               mbJustLostBoostChunk;    // :170
        u32                muNumChained;            // :171
        f32                mfBoostAmount;           // :172
        f32                mfMaxBoost;              // :173
        f32                mfCurrentBoostingTime;   // :174
        bool               mbBoostIsFull;           // :175
        BrnWorld::EBoostType meBoostType;           // :177

        void Construct();                           // :184 (declared-only; own TU)
    };

    // ------------------------------------------------------------------------------------
    // The per-car flags word published into RCEntityActiveRaceCarOutputInterface::
    // maxRaceCarFlags[idx]. The ONLY producer is RaceCarEntityModule::UpdateOutputInterfaces
    // @0x822F5CF8, which builds it bit by bit (`ori r24, r24, <bit>`); the consumers are the
    // interface's own Is*() predicates.
    //
    // PROVEN: bit 0 (RaceCarEntityModule sets it on every attached slot; the out-of-line
    // IsRaceCarActive @0x82277B10 is `& 1`), bit 2 (IsRaceCarRival @0x82705690 is `>> 2 & 1`,
    // producer arm `muType == E_RACE_CAR_TYPE_AI`), bit 3 (IsRaceCarNetwork @0x82310140 is
    // `>> 3 & 1`, producer arm `muType == E_RACE_CAR_TYPE_NETWORK`).
    // FLAG (inferred, not proven): the remaining five names. The producer's CONDITIONS are
    // asm-attested exactly as written below; what is inferred is which declared Is*()
    // predicate reads each bit, because those predicates are header inlines with no export.
    // ------------------------------------------------------------------------------------
    enum ERaceCarOutputFlag : u32
    {
        E_RACE_CAR_OUTPUT_FLAG_IN_USE        = 0x0001, // set for every attached slot
        E_RACE_CAR_OUTPUT_FLAG_PLAYER        = 0x0002, // mpRaceCar->muType == E_RACE_CAR_TYPE_PLAYER
        E_RACE_CAR_OUTPUT_FLAG_RIVAL         = 0x0004, // mpRaceCar->muType == E_RACE_CAR_TYPE_AI
        E_RACE_CAR_OUTPUT_FLAG_NETWORK       = 0x0008, // mpRaceCar->muType == E_RACE_CAR_TYPE_NETWORK
        E_RACE_CAR_OUTPUT_FLAG_LOADED        = 0x0010, // ActiveRaceCar::IsActive()  (muState == E_STATE_ACTIVE)
        E_RACE_CAR_OUTPUT_FLAG_CONNECTING    = 0x0020, // meOnlineState == E_ONLINE_STATE_CONNECTING
        E_RACE_CAR_OUTPUT_FLAG_LOST_CONTACT  = 0x0040, // mbNotSendingNetworkUpdates
        E_RACE_CAR_OUTPUT_FLAG_DISCONNECTED  = 0x0080, // mbIsDisconnectedFromNetwork
        E_RACE_CAR_OUTPUT_FLAG_IN_SHOWTIME   = 0x0100  // mbIsInShowtime
    };

    // DWARF :189 -- minimal per-car snapshot held in the active interface's race list.
    struct CarsInTheRaceData
    {
        Vector3             mPosition;            // :191
        Vector3             mPreviousPosition;    // :192
        Vector3             mDirection;           // :193
        EActiveRaceCarIndex meActiveRaceCarIndex; // :194
        EGlobalRaceCarIndex meGlobalRaceCarIndex; // :195
        bool                mbIsPlayer;           // :196
    };

    // DWARF :208 -- the per-active-race-car output interface (the player + up to 7 rivals).
    // maRaceCarStates is the real BrnPhysics::Vehicle::RaceCarState[8] (complete 1120-byte
    // type, by value). maRaceCarMaterialColours is the real RwRGBAReal[8] (by value). All
    // member accessors are declared-only (their own TUs); only GetRaceCarStateMutable is
    // routed to this header's .cpp (X360 0x8227D690).
    struct RCEntityActiveRaceCarOutputInterface
    {
        typedef BrnPhysics::Vehicle::RaceCarState RaceCarState;   // :61

        // DWARF :415 -- public live race-car list.
        Array<CarsInTheRaceData, 8u> maCarsInTheRace;             // :415

        // X360 0x826BB258 -- the member-wise copy constructor the build emitted out-of-line
        // (memcpy of the leading POD spans + per-element RaceCarState copy-construction). The
        // committed header previously declared only operator=; the copy ctor is added here
        // additively (its definition lives in this interface's own .cpp).
        RCEntityActiveRaceCarOutputInterface(const RCEntityActiveRaceCarOutputInterface&);
        RCEntityActiveRaceCarOutputInterface() {}

        void operator=(const RCEntityActiveRaceCarOutputInterface&);                         // :213 (own TU)
        void Clear();                                                                        // :216 (own TU)
        const RaceCarState* GetRaceCarState(EActiveRaceCarIndex) const;                       // :220 (own TU)
        const RwRGBAReal&   GetRaceCarColour(EActiveRaceCarIndex) const;                      // :224 (own TU)
        u16  GetRaceCarAISection(EActiveRaceCarIndex) const;                                 // :228 (own TU)
        CgsID GetRivalId(EActiveRaceCarIndex) const;                                         // :232 (own TU)
        EGlobalRaceCarIndex GetGlobalRaceCarIndex(EActiveRaceCarIndex) const;                // :236 (own TU)
        CgsID GetCarModelId(EActiveRaceCarIndex) const;                                      // :240 (own TU)
        bool IsRaceCarActive(EActiveRaceCarIndex) const;                                     // :244 (own TU)
        bool IsRaceCarLoaded(EActiveRaceCarIndex) const;                                     // :248 (own TU)
        bool IsRaceCarRival(EActiveRaceCarIndex) const;                                      // :252 (own TU)
        bool IsRaceCarPlayer(EActiveRaceCarIndex) const;                                     // :256 (own TU)
        bool IsRaceCarNetwork(EActiveRaceCarIndex) const;                                    // :260 (own TU)
        bool IsCarConnecting(EActiveRaceCarIndex) const;                                     // :264 (own TU)
        bool HaveWeLostContactWithThisCar(EActiveRaceCarIndex) const;                        // :268 (own TU)
        bool IsCarDisconnected(EActiveRaceCarIndex) const;                                   // :272 (own TU)
        bool IsCarInShowtime(EActiveRaceCarIndex) const;                                     // :276 (own TU)
        u32  GetActiveRaceCarColourIndex(EActiveRaceCarIndex) const;                         // :280 (own TU)
        s32  GetActiveRaceCarPaintFinishIndex(EActiveRaceCarIndex) const;                    // :284 (own TU)
        bool IsPlayerCarActive() const;                                                      // :287 (bodied @0x8259B9xx family in BrnRCEntityActiveRaceCarOutputInterface.cpp)
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const;                             // :290 (bodied @0x82277BF8 in BrnRCEntityActiveRaceCarOutputInterface.cpp)
        EntityId GetPlayerRaceCarEntityId() const;                                           // :293 (own TU)
        // X360 0x8259BB58 -- returns the player car's current speed (mfSpeedMPH @968 inside the
        // player's RaceCarState). Asserts the player index has been set, like GetPlayerRaceCarState.
        // Not in the committed declaration set; added additively (definition in this TU's .cpp).
        f32  GetPlayerSpeedMPH() const;
        // :296 -- HEADER INLINE (no out-of-line symbol in the image). Recovered from its
        // inlined call site at the top of BrnGameModule::BridgeWorldToDirector @0x823E3AB0:
        //   `v25 = mePlayerActiveRaceCarIndex;
        //    v25 == -1 ? 0 : *(1120*v25 + iface + 1914)`
        // 1914 - 816 (maRaceCarStates) == element +1098, which is the console offset of the
        // committed mbCrashing (@1094 here -- the uniform +4 X360 drift, see BrnPlayerInfo/
        // GameBridgeWorldToX). Note it does NOT bounds-assert -- only the -1 sentinel is
        // tested; the "< E_ACTIVE_RACE_CAR_INDEX_COUNT" assert at that call site belongs to
        // the IsPlayerCarActive() inline that guards it.
        bool IsPlayerCarCrashing() const
        {
            if (mePlayerActiveRaceCarIndex == E_ACTIVE_RACE_CAR_INDEX_INVALID)
                return false;
            return maRaceCarStates[mePlayerActiveRaceCarIndex].mbCrashing;
        }
        bool IsPlayerCarDeforming() const;                                                   // :299 (own TU)
        bool IsThePlayerDrivableFromCrash() const;                                           // :302 (own TU)
        bool IsPlayerCarFatalyCrashing() const;                                              // :305 (own TU)
        bool IsPlayerInAir() const;                                                          // :308 (own TU)
        f32  TimePlayerInAir() const;                                                        // :311 (own TU)
        bool IsPlayerInReverseGear() const;                                                  // :314 (own TU)
        bool IsPlayerEngineOn() const;                                                       // :317 (own TU)
        bool IsPlayerEngineStarting() const;                                                 // :320 (own TU)
        EActiveRaceCarEngineState GetPlayerEngineState() const;                              // :323 (own TU)
        // :327 / :331 -- HEADER INLINES (no out-of-line symbols). Recovered from the pair
        // BrnGameModule::BridgeWorldToDirector @0x823E3AB0 inlines and ORs together to fill
        // VehicleInfo::mbEngineOn:
        //   engine-on(idx)       = (idx != mePlayerActiveRaceCarIndex)
        //                       || (mePlayerActiveRaceCarIndex != -1 && engineState == RUNNING)
        //   engine-starting(idx) = (idx != mePlayerActiveRaceCarIndex)
        //                       || (mePlayerActiveRaceCarIndex != -1 && engineState == STARTING)
        // i.e. only the PLAYER's engine state is tracked here; every other slot reports "on".
        // The split of the single recovered expression into these two declared predicates is
        // the DWARF's own naming (the compared enumerators are RUNNING(2) and STARTING(1));
        // that split is INFERRED, the combined expression is asm-attested.
        bool IsRaceCarEngineOn(EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            if (leActiveRaceCarIndex != mePlayerActiveRaceCarIndex)
                return true;
            return mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   mePlayerEngineState == E_ACTIVE_RACE_CAR_ENGINE_STATE_RUNNING;
        }
        bool IsRaceCarEngineStarting(EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            if (leActiveRaceCarIndex != mePlayerActiveRaceCarIndex)
                return true;
            return mePlayerActiveRaceCarIndex != E_ACTIVE_RACE_CAR_INDEX_INVALID &&
                   mePlayerEngineState == E_ACTIVE_RACE_CAR_ENGINE_STATE_STARTING;
        }
        const BoostOutputInfo& GetBoostOutputInfo() const;                                   // :337 (own TU)
        void SetBoostOutputInfoN(EActiveRaceCarIndex, const BoostOutputInfo&);               // :342 (own TU)
        // :346 -- HEADER INLINE (no out-of-line symbol). BridgeWorldToDirector's inlined copy
        // carries this header's OWN assert lines (:1157 / :1158) and then indexes
        // `36*idx + this + 528` -- the 36-byte BoostOutputInfo stride at maBoostOutputInfo.
        const BoostOutputInfo* GetBoostOutputInfoN(EActiveRaceCarIndex leActiveRaceCarIndex) const
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0,
                                    "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");   // :1157
            CGS_ASSERT(leActiveRaceCarIndex <  E_ACTIVE_RACE_CAR_INDEX_COUNT,
                                    "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT"); // :1158
            return &maBoostOutputInfo[leActiveRaceCarIndex];
        }
        // ⚠️ PARAMETER ORDER (asm-attested, 2026-08-01): the console passes
        //   r9 = luFlags, r10 = lu16AISection, stack+0x54 = luColourIndex,
        //   stack+0x5C = liPaintFinishIndex
        // (X360 0x822CC080: `sthx r25(r9)` -> maxRaceCarFlags[idx] @2*(idx+0x13C0);
        //  `sthx r24(r10)` -> mau16ActiveRaceCarAISections[idx] @2*(idx+0x1378);
        //  `stwx arg_54` -> mauActiveRaceCarColourIndex[idx] @4*(idx+0x9AC);
        //  `stwx arg_5C` -> maiActiveRaceCarPaintFinishIndex[idx] @4*(idx+0x9B4)).
        // The u32 pair FLAGS/COLOUR-INDEX used to be transcribed the other way round here;
        // it was latent because the function had no caller. Corrected -- see the .cpp.
        void SetRaceCarState(EActiveRaceCarIndex, EGlobalRaceCarIndex, CgsID, CgsID,
                             const RaceCarState*, u32 /*luFlags*/, u16, u32 /*luColourIndex*/,
                             s32, Vector4, Vector3, bool, bool);                             // :362 (own TU)
        void SetPlayerActiveRaceCarData(EActiveRaceCarIndex, EActiveRaceCarEngineState);     // :367 (own TU)
        bool GetAllActiveCarsRead() const;                                                   // :370 (own TU)
        void SetAllActiveCarsReady(bool);                                                    // :374 (own TU)
        CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>
             GetDeformationModelResourcePtr(EActiveRaceCarIndex) const;                      // :378 (own TU)
        void SetDeformationModelResourcePtr(EActiveRaceCarIndex,
             const CgsResource::ResourcePtr<BrnPhysics::Deformation::StreamedDeformationSpec>&); // :383 (own TU)
        const CgsWorld::WorldMap2D* GetWorldMap2D() const;                                   // :386 (own TU)
        void SetWorldMap2D(const CgsWorld::WorldMap2D*);                                     // :390 (own TU)
        const RaceCarState* GetPlayerRaceCarState() const;                                   // :393 (own TU)
        Vector3 GetPlayerPosition() const;                                                   // :396 (own TU)
        Vector3 GetPlayerDirection() const;                                                  // :399 (own TU)
        Vector3 GetPlayerLinearVelocity() const;                                             // :402 (own TU)
        bool GetCanDriveAwayFromCrash() const;                                               // :405 (own TU)
        void ClearCarsInRace();                                                              // :408 (own TU)
        void AddCarToRace(BrnWorld::RaceCar*, EGlobalRaceCarIndex);                           // :413 (own TU)
        void SetActiveRaceCarIndex(BrnGameState::GameStateModuleIO::EPlayerScoringIndex, EActiveRaceCarIndex); // :420 (own TU)
        EActiveRaceCarIndex GetActiveRaceCarIndex(BrnGameState::GameStateModuleIO::EPlayerScoringIndex) const; // :424 (own TU)
        // :428 -- HEADER INLINE, not an own-TU function: the image has no out-of-line
        // SetPlayerWrecked symbol at all, and its only caller (RaceCarEntityModule::
        // UpdateOutputInterfaces @0x822F5CF8) reaches the member with a bare `stb r11, 0x28E0`.
        // (The batch annotation on the block above marked every declaration "own TU"; this one
        // is a plain store and is bodied where the DWARF declares it.)
        void SetPlayerWrecked(bool lbWrecked) { mbPlayerWrecked = lbWrecked; }   // @+0x28E0
        bool IsPlayerWrecked() const;                                                        // :431 (own TU)
        Vector3 GetCurrentInAirRotations(EActiveRaceCarIndex) const;                         // :435 (own TU)
        bool HasCrashedIntoWater(EActiveRaceCarIndex) const;                                 // :440 (own TU)

        // X360-only NON-const per-index element getter (X360 0x8227D690). DISTINCT from the
        // const GetRaceCarState(:220); routed to this header's .cpp. Returns &maRaceCarStates[idx].
        RaceCarState* GetRaceCarStateMutable(EActiveRaceCarIndex);

    private:
        BoostOutputInfo           maBoostOutputInfo[8];                  // :443
        RaceCarState              maRaceCarStates[8];                    // :444 (by value; base +816, stride 1120)
        CgsID                     maRivalIds[8];                         // :445
        CgsID                     maCarModelIds[8];                      // :446
        u32                       mauActiveRaceCarColourIndex[8];        // :447
        s32                       maiActiveRaceCarPaintFinishIndex[8];   // :448
        u16                       mau16ActiveRaceCarAISections[8];       // :449
        RwRGBAReal                maRaceCarMaterialColours[8];           // :450 (by value)
        u16                       maxRaceCarFlags[8];                    // :451
        Vector3                   maCurrentInAirRotations[8];            // :452
        bool                      mbHasCrashedIntoWater[8];              // :453
        EGlobalRaceCarIndex       maGlobalRaceCarIndices[8];             // :454
        EActiveRaceCarIndex       maeActiveRaceCarIndex[8];              // :455
        EActiveRaceCarIndex       mePlayerActiveRaceCarIndex;            // :456
        EActiveRaceCarEngineState mePlayerEngineState;                   // :457
        bool                      mbIsPlayerCarActive;                   // :458
        bool                      mbAllActiveCarsReady;                  // :459
        CgsResource::ResourceHandle maDeformationModelResourceHandles[8];// :460
        CgsWorld::WorldMap2D      mWorldMap2D;                           // :461 (by value)
        bool                      mbPlayerWrecked;                       // :462
        bool                      mbCanDriveAwayFromCrash;               // :463
    };

    // DWARF :476 -- the global race-car output interface: a STRUCT OF PARALLEL 35-ELEMENT
    // ARRAYS (the player + up to 34 rivals/traffic racers). Member accessors are declared-only
    // (own TUs); only GetActiveCarDataElementAddress is routed to this header's .cpp (0x823101C0).
    struct RCEntityGlobalRaceCarOutputInterface
    {
        void operator=(const RCEntityGlobalRaceCarOutputInterface&);    // :483 (own TU)
        void Clear();                                                   // :486 (own TU)
        void SetPlayerGlobalRaceCarIndex(EGlobalRaceCarIndex);          // :490 (own TU)
        void SetRaceCarData(Vector3, Vector3, BrnWorld::WorldRegion, CgsID, CgsID, f32, u16,
                            EGlobalRaceCarIndex, s8, EActiveRaceCarIndex,
                            bool, bool, bool, bool, bool, bool);        // :509 (own TU)
        EGlobalRaceCarIndex GetPlayerGlobalRaceCarIndex() const;        // :512 (own TU)
        Vector3 GetRaceCarPosition(EGlobalRaceCarIndex) const;          // :516 (own TU)
        Vector3 GetRaceCarAt(EGlobalRaceCarIndex) const;                // :520 (own TU)
        BrnWorld::WorldRegion GetWorldRegion(EGlobalRaceCarIndex) const;// :524 (own TU)
        CgsID GetRivalId(EGlobalRaceCarIndex) const;                    // :528 (own TU)
        CgsID GetCarModelId(EGlobalRaceCarIndex) const;                 // :532 (own TU)
        f32  GetRaceCarSpeed(EGlobalRaceCarIndex) const;                // :536 (own TU)
        EActiveRaceCarIndex GetActiveRaceCarIndex(EGlobalRaceCarIndex) const; // :540 (own TU)
        EGlobalRaceCarIndex GetGlobalRaceCarIndex(EActiveRaceCarIndex) const; // :544 (own TU)
        s16  GetAISectionIndex(EGlobalRaceCarIndex) const;              // :548 (own TU)
        bool IsPlayer(EGlobalRaceCarIndex) const;                       // :552 (own TU)
        bool IsRivalAI(EGlobalRaceCarIndex) const;                      // :556 (own TU)
        s8   GetRivalIndex(EGlobalRaceCarIndex) const;                  // :560 (own TU)
        bool IsNetwork(EGlobalRaceCarIndex) const;                      // :564 (own TU)
        bool IsInCurrentMode(EGlobalRaceCarIndex) const;                // :568 (own TU)
        bool IsDispersing(EGlobalRaceCarIndex) const;                   // :572 (own TU)
        bool IsInRange(EGlobalRaceCarIndex) const;                      // :576 (own TU)
        CgsContainers::BitArray<35u> GetGlobalRaceCarBitArray() const;  // :579 (own TU)

        // X360-only element-address accessor (X360 0x823101C0); routed to this header's .cpp.
        const void* GetActiveCarDataElementAddress(EActiveRaceCarIndex) const;

    private:
        Vector3               maRaceCarPositions[35];     // :583
        Vector3               maRaceCarAts[35];           // :584
        BrnWorld::WorldRegion maRaceCarWorldRegions[35];  // :585
        CgsID                 maRivalIds[35];             // :586
        CgsID                 maCarModelIds[35];          // :587
        f32                   mafRaceCarSpeeds[35];       // :589
        EActiveRaceCarIndex   maeActiveRaceCarIndices[35];// :590
        s8                    maiRivalIndices[35];        // :591
        u16                   mauAISectionIndices[35];    // :592
        CgsContainers::BitArray<35u> mGlobalRaceCarIndices;   // :593
        CgsContainers::BitArray<35u> mIsPlayerFlags;         // :594
        CgsContainers::BitArray<35u> mIsRivalAIFlags;        // :595
        CgsContainers::BitArray<35u> mIsNetworkFlags;        // :596
        CgsContainers::BitArray<35u> mIsInCurrentModeFlags;  // :597
        CgsContainers::BitArray<35u> mIsDispersingFlags;     // :598
        CgsContainers::BitArray<35u> mIsInRangeFlags;        // :599
        EGlobalRaceCarIndex   mePlayerGlobalRaceCarIndex; // :600
    };

    // DWARF :612 -- the player-reset interface (rest position + this-frame reset flag).
    struct RCEntityPlayerResetInterface
    {
        // The four accessors are header-inline on the console (no X360 symbol, no ledger row);
        // every reconstructed caller so far had them inlined. Clear() is the form
        // BrnTrafficIO::InputBuffer_PrePhysics::Construct @0x827615F8 inlines at 0x8276163C..
        // 0x82761658 (`vspltisw v0,0 ; stvx128 v0 -> mRestPos ; stb 0 -> mbPlayerResetThisFrame`),
        // landed 2026-08-19 (wave Q5 round-3 integration). The other three stay declaration-only
        // until a caller needs them.
        void Clear()                                        // :615
        {
            mRestPos.x = 0.0f;
            mRestPos.y = 0.0f;
            mRestPos.z = 0.0f;
            mRestPos.w = 0.0f;
            mbPlayerResetThisFrame = false;
        }
        void SetPlayerResetPos(Vector3);                    // :619
        bool PlayerHasRestThisFrame() const;                // :622
        Vector3 GetPlayerResetPos() const;                  // :625

    private:
        Vector3 mRestPos;                                   // :629
        bool    mbPlayerResetThisFrame;                     // :630
    };
}
}
