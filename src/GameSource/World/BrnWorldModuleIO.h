// ============================================================================
// b5-decomp/src/GameSource/World/BrnWorldModuleIO.h
//
// Home of BrnWorldIO::UpdateInputBuffer and BrnWorldIO::UpdateOutputBuffer -- the
// World module's per-frame "Update" input/output IO-buffers (derive
// CgsModule::IOBuffer). The 32 ledger functions for class:BrnWorldIO::
// UpdateInputBuffer and the 46 for class:BrnWorldIO::UpdateOutputBuffer are the
// out-of-line accessors/mutators the X360 build emitted for these buffers (the
// const/non-const getters that did not get inlined, the queue Append* mergers,
// and the interface Set* writers).
//
// LAYOUT (FROZEN, X360-authoritative):
//   The member list + order is the DecFIGS DWARF for BrnWorldModuleIO.h:324..354
//   (UpdateInputBuffer private members). The leading per-active-race-car arrays sit
//   at proven byte offsets (the X360 SetRaceCar*/SetLostContact/SetCarSelectStatus
//   bodies store at this+2/this+18/this+34/.../this+74), pinned below with
//   static_asserts in _AssertLayout(). The IOBuffer base is a single status byte;
//   MSVC pads it to +2 before the first u16 array exactly as the X360 layout does.
//
//   The large per-frame interface/queue payloads (VehicleInputInterface,
//   TakedownEventQueue, the trigger/traffic/crash/scoring/replay interfaces, ...) are
//   modelled as minimal-complete sized slices in their canonical-spelling local
//   namespaces. Per the project minimal-slice pattern (see the sibling
//   BrnRaceCarEntityModuleIO.h traffic slices) they are embedded BY VALUE and accessed
//   only BY NAME -- the accessor bodies return &member / forward to a member method, so
//   the byte-exact internal size of each payload is NOT load-bearing for this TU and is
//   grown additively by each payload's own canonical TU. The accessor lock semantics,
//   the member touched, and the store-for-store mutation ARE the parity contract here.
//
// ACCESSOR SHAPE (X360 binary, authoritative): every body tests the buffer status flag
// then acts on the named member. Lock bit -> const-ness / message:
//   ">>3 &1" (eStatusLockedForWrite 0x08) => write-lock, "Not locked for writing",
//                                            mutable getter / Append / Set mutator;
//   ">>4 &1" (eStatusLockedForRead  0x10) => read-lock,  "Not locked for reading",
//                                            const getter.
#pragma once

#include "types.hpp"                                              // s8/s16/s32/u8/u16/u32/f32
#include "GameShared/GameClasses/Core/CgsAssert.h"                // CGS_ASSERT (per-race-car inline getters)
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"            // CgsModule::IOBuffer base
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"  // CgsModule::VariableEventQueue<N,16>
#include "GameShared/GameClasses/Module/CgsEventQueue.h"          // CgsModule::EventQueue<T,N>
#include "GameSource/BurnoutConstants.h"                          // EActiveRaceCarIndex, EGlobalRaceCarIndex

// ---- UpdateOutputBuffer member-type homes (all committed, reused by name) ----
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_TriangleCache.h"                   // CgsSceneManager::SceneManagerIO::TriangleCacheInterface
#include "GameShared/GameClasses/System/AttribSys/CgsAttribSysModuleIO.h"                          // CgsAttribSys::AttribSysIO::AttribSysRequestInterface (<2048>-shaped)
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleOutputInterface.h"                  // BrnPhysics::Vehicle::VehicleOutputInterface / VehicleManagerOutputInterface
#include "GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h"             // BrnPhysics::Vehicle::VehicleDriverInputInterface
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"                                // BrnPhysics::ContactSpy::ContactSpyInterface
#include "GameSource/Physics/DeformationManager/SharedIO/BrnDeformationOutputInterface.h"          // BrnPhysics::Deformation::DeformationOutputInterface
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"                                 // BrnPhysics::Props::PropUpdateNotification
#include "GameSource/Resource/SharedIO/BrnGameDataRequestQueue.h"                                  // BrnResource::GameDataIO::RequestInterface<N>
#include "GameSource/Director/SharedIO/BrnDirectorVehicleInputInterface.h"                         // BrnDirector::BrnDirectorVehicleInputInterface
#include "GameSource/Effects/SharedIO/BrnEffectsEnvironmentInterface.h"                            // BrnEffects::EffectsEnvironmentInterface
#include "GameSource/Replays/BrnReplayRequestInterface.h"                                          // BrnReplays::ReplayIO::RequestInterface
#include "GameSource/Replays/BrnReplayStatusInterface.h"                                           // BrnReplays::ReplayIO::StatusInterface
#include "GameSource/Sound/Module/SharedIO/BrnSoundRootSharedIO.h"                                 // BrnSound::Module::Io::SoundWorldLoadEvent
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"                                  // BrnAI::AIModuleIO::AICarOutputInterface
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"                                         // BrnAI::RouteMapModuleIO::RouteResponseQueue
#include "GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h"               // BrnWorld::CrashIO::NetworkOutputInterface
#include "GameSource/World/EntityModules/PropEntityModule/BrnPropEntityModuleIO.h"                 // BrnWorld::PropEntityIO::PropVFXLocatorEvent
#include "GameSource/World/EntityModules/PropEntityModule/SharedIO/BrnPropBecamePhysicalEvent.h"   // BrnWorld::PropEntityIO::PropBecamePhysicalEvent
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActive/Global race-car output interfaces + AudioCarDataLoadedEvent
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficDirectorInterfaces.h"  // BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficNetworkInterfaces.h"   // BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficSoundInterfaces.h"     // BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h"       // BrnTraffic::BrnTrafficIO::TrafficTypeResponse
#include "GameSource/World/EntityModules/TriggerEntityModule/BrnTriggerEntityModuleIO.h"           // BrnWorld::TriggerEntityModuleIO::TriggerEntityModuleOutputInterface
#include "GameSource/World/EntityModules/WorldEntityModule/SharedIO/BrnWorldEntityStatusInterface.h"   // BrnWorld::WorldEntityIO::StatusInterface
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h"                          // BrnGameState::TakedownEvent

#include <cstddef>   // offsetof
#include <cstring>   // memcpy

// EPaybackType is a network enum homed elsewhere; only its name/width is needed here.
namespace BrnNetwork { enum EPaybackType : s32; }

namespace BrnWorldIO
{
    // BrnWorldModuleIO.h:67-68
    const s32 KI_WORLD_EVENT_QUEUE_MAX_SIZE        = 4096;
    const s32 KI_WORLD_RENDER_EVENT_QUEUE_MAX_SIZE = 32768;

    // ------------------------------------------------------------------------
    // Minimal-complete sized payload slices (canonical-spelling local namespaces).
    // Each carries the small surface the X360 accessor bodies actually invoke:
    //   - interface payloads: an Append(const T*) / Set(const T*) merge-or-copy entry;
    //   - the trigger/game-action queues: VariableEventQueue<N,16> typedefs reused by name.
    // Real layouts/sizes are grown by each payload's own TU; sizes here are NOMINAL.
    // ------------------------------------------------------------------------

    // VehicleInputInterface (BrnPhysics::Vehicle home not yet exposing the X360-attested
    // AppendVehicleInputInterface entry point). AppendVehicleInputInterface calls a distinct
    // "VehicleInputInterface::Append" symbol -- separate from the committed operator= (Clear()+
    // Append() per queue) -- whose full per-queue merge body has not been recovered yet. Modelled
    // as a sized slice with that single by-name entry point until Append() is grown onto the
    // canonical BrnPhysics::Vehicle::VehicleInputInterface home.
    struct VehicleInputInterface
    {
        void Append(const VehicleInputInterface* lpSource)
        {
            // Faithful merge: copy the source payload image (store-for-store).
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL -- grown by BrnVehicleInputInterface TU
    };

    // VehicleDriverInputInterface: canonical home is committed
    // (GameSource/Physics/VehicleManager/SharedIO/BrnVehicleDriverInputInterface.h). X360
    // AppendVehicleDriverInputInterface forwards `this` (the interface's base, i.e. its leading
    // mDriverUpdateQueue member) straight into BrnPhysics::Vehicle::VehicleDriverInputInterface::
    // Append, a thin queue-merge -- modelled via the committed type's own
    // GetUpdateDriverQueue()->Append(...) in the accessor body below.
    typedef BrnPhysics::Vehicle::VehicleDriverInputInterface VehicleDriverInputInterface;

    // Trigger query input interface = VariableEventQueue<4096,16> (X360 Append into +293412
    // dispatches CgsModule::VariableEventQueue<4096,16>::Append). Reused by name.
    typedef CgsModule::VariableEventQueue<KI_WORLD_EVENT_QUEUE_MAX_SIZE, 16> TriggerQueryInputInterface;

    // Trigger management input interface (Append* exists in DWARF :276; modelled as a sized
    // slice with an Append entry; not in the 32 out-of-line ledger set -- inlined on X360).
    struct TriggerManagementInputInterface
    {
        void Append(const TriggerManagementInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[64];    // NOMINAL
    };

    // TimerStatusInterface (X360 SetTimerStatusInterface copies 12 words from the source into
    // this+160900..+160948; modelled as a fixed POD the setter struct-assignment-copies field-
    // for-field). NOTE: an earlier revision modelled only 11 words (44 bytes); the X360 body
    // actually stores through offset 0x2C (asm: lfs/stfs f0,0x14(r10/r9) at the tail of the
    // second 6-field block, i.e. base+0x18+0x14 = base+0x2C), so the 12th word is load-bearing.
    struct TimerStatusInterface
    {
        f32 maData[12];      // X360 copies *a2 .. *(a2+48): 12 words
    };

    // RaceCarRaceDistanceInterface (X360 SetRaceCarRaceDistanceInterface copies 10 words).
    struct RaceCarRaceDistanceInterface
    {
        s32 maData[10];      // X360 copies a 10-word block
    };

    // ScoringInterface (X360 SetScoringInterface XMemCpy 2736 bytes into +317536).
    struct ScoringInterface
    {
        u8 maData[2736];     // X360 SetScoringInterface copies 2736 bytes
    };

    // OnlineScoringInterface (X360 SetOnlineScoringInterface memcpy 164 bytes into +322384).
    struct OnlineScoringInterface
    {
        u8 maData[164];      // X360 SetOnlineScoringInterface copies 164 bytes
    };

    // TrafficNetworkInputInterface: the canonical home (BrnTraffic::BrnTrafficIO::
    // TrafficNetworkInputInterface, GameSource/World/EntityModules/TrafficEntityModule/SharedIO/
    // BrnTrafficNetworkInterfaces.h) IS committed and byte-confirms the X360 body (queue header
    // Clear() = the member+8 zero-store, BaseEventQueue<ActivateHullEvent>::Append = the merge,
    // mbDiverged @ offset 0x6C = the trailing byte copy) -- but it exposes no public mutator for
    // its private mActivateHullQueue/mbDiverged (only a const GetActivateHullQueue() and
    // SetDiverged/HasDiverged, none of which allow a Clear()+Append() merge from outside). Adding
    // that mutator belongs to BrnTrafficNetworkInterfaces.h/.cpp (a different TU), so this stays a
    // local sized-slice placeholder here. Modelled with a Set entry pending that mutator landing.
    struct TrafficNetworkInputInterface
    {
        void Set(const TrafficNetworkInputInterface* lpSource)
        {
            if (lpSource && lpSource != this)
                std::memcpy(maPayload, lpSource->maPayload, sizeof(maPayload));
        }
        u8 maPayload[256];   // NOMINAL
    };

    // CrashNetworkInputInterface: canonical home is committed
    // (GameSource/World/CrashModule/SharedIO/BrnCrashModuleNetworkIOInterfaces.h). X360
    // SetCrashNetworkInterface forwards to BrnWorld::CrashIO::NetworkInputInterface::operator=
    // (bitset copy + per-race-car queue clear+merge), which is the committed type's own
    // operator=.
    typedef BrnWorld::CrashIO::NetworkInputInterface CrashNetworkInputInterface;

    // PlayerVehicleControls (X360 SetPlayerVehicleControls memcpy 60 bytes into +317264).
    struct PlayerVehicleControls
    {
        u8 maData[60];       // X360 SetPlayerVehicleControls copies 60 bytes
    };

    // DebugController (X360 Get const at +317324 read-lock, Get non-const at +317324 write-lock).
    struct DebugController
    {
        u8 maData[212];      // NOMINAL (between +317324 and +317536)
    };

    // ReplayStatusInterface: canonical home is committed (GameSource/Replays/
    // BrnReplayStatusInterface.h). X360 SetReplayStatusInterface forwards to
    // BrnReplays::ReplayIO::StatusInterface::operator= (member-wise copy: status flags, all 6
    // reels, the two current-reel indices, and the trailing debug alpha), the committed type's
    // own operator=.
    typedef BrnReplays::ReplayIO::StatusInterface ReplayStatusInterface;

    // RequestInterface (mWorldEntityRequestInterface; X360 returns &member, both const R and
    // non-const W overloads). Modelled as a sized slice (accessed by-name only).
    struct WorldEntityRequestInterface
    {
        u8 maPayload[64];    // NOMINAL
    };

    // GameActionQueue / TakedownEventQueue are large variable-size queues whose Append merges a
    // source queue. GameActionQueue = VariableEventQueue<13312,16> (X360 +147572 Append).
    typedef CgsModule::VariableEventQueue<13312, 16> GameActionQueue;

    // TakedownEventQueue: X360 +160952 Append dispatches BrnGameState::TakedownEvent_::Append,
    // which is the committed CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> instantiation's
    // (== BaseEventQueue<TakedownEvent>) own Append merge (GameSource/GameState/TakedownManager/
    // BrnTakedownManagerTypes.h + EventQueue_TakedownEvent_8.cpp). Reused by name.
    typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownEventQueue;

    // ========================================================================
    // BrnWorldIO::UpdateInputBuffer  (DWARF BrnWorldModuleIO.h:184)
    // ========================================================================
    struct UpdateInputBuffer : public CgsModule::IOBuffer
    {
        // ---- per-active-race-car colour / paint / contact / select state -----------
        void SetRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16ColourIndex);       // :203 (W store-only)
        void SetRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex, u16 lu16PaintIndex);   // :216 (W store-only)
        void SetLostContact(EActiveRaceCarIndex leActiveRaceCarIndex);                                   // :228 (W store-only)
        void SetRegainedContact(EActiveRaceCarIndex leActiveRaceCarIndex);                               // :236 (W store-only)
        void SetCarSelectStatus(EActiveRaceCarIndex leActiveRaceCarIndex, bool lbStatus);                // :245 (W store-only)

        // ---- vehicle input interfaces ---------------------------------------------
        const BrnWorldIO::VehicleInputInterface*       GetVehicleInputInterface() const;                // :259 R (inlined; provided for completeness)
        void                                           AppendVehicleInputInterface(const BrnWorldIO::VehicleInputInterface*);       // :260 W
        const BrnWorldIO::VehicleDriverInputInterface* GetVehicleDriverInputInterface() const;           // :262 R (0x827A3660 GetVehicl)
        void                                           AppendVehicleDriverInputInterface(const BrnWorldIO::VehicleDriverInputInterface*); // :263 W

        // ---- game-action queue ----------------------------------------------------
        GameActionQueue*       GetGameActionQueue();                                                     // :266 W (0x823B4738 GetG)
        void                   AppendGameActionQueue(const GameActionQueue*);                            // :267 W (0x823C8B80)

        // ---- timer status ---------------------------------------------------------
        void                   SetTimerStatusInterface(const TimerStatusInterface*);                     // :270 W

        // ---- takedown event queue -------------------------------------------------
        void                   AppendTakedownEventQueue(const TakedownEventQueue*);                      // :273 W (0x823C8C38)

        // ---- trigger query interface ----------------------------------------------
        void                   AppendTriggerQueryInputInterface(const TriggerQueryInputInterface*);      // :279 W (0x823C8CF0)

        // ---- traffic / crash network interfaces -----------------------------------
        const TrafficNetworkInputInterface* GetTrafficNetworkInterface() const;                         // :284 R (0x827A3AF8 Get)
        void                                SetTrafficNetworkInterface(const TrafficNetworkInputInterface*); // :285 W (0x823C8DA8)
        const CrashNetworkInputInterface*   GetCrashNetworkInterface() const;                            // :287 R (0x827A3BA0 GetCrashNetworkIn)
        void                                SetCrashNetworkInterface(const CrashNetworkInputInterface*); // :288 W (0x823C8E70)

        // ---- debug controller -----------------------------------------------------
        const DebugController* GetDebugController() const;                                               // :290 R (0x827BBE48 GetDebugContro)
        DebugController*       GetDebugController();                                                     // :291 W (0x823B49B0)

        // ---- race-car race-distance interface -------------------------------------
        void                   SetRaceCarRaceDistanceInterface(const RaceCarRaceDistanceInterface*);     // :295 W (0x823B4A58)

        // ---- scoring interfaces ---------------------------------------------------
        const ScoringInterface* GetScoringInterface() const;                                            // :297 R (0x827A3CF0 Ge)
        void                    SetScoringInterface(const ScoringInterface*);                            // :298 W (0x823B4B28)
        void                    SetOnlineScoringInterface(const OnlineScoringInterface*);                // :301 W (0x823B4BE0)

        // ---- controller-active flag -----------------------------------------------
        bool GetControllerActive() const;                                                               // :303 R (0x827A3E40)
        void SetControllerActive(bool);                                                                 // :304 W (0x823B4C98)

        // ---- world-entity request interface ---------------------------------------
        WorldEntityRequestInterface*       GetWorldEntityRequestInterface();                            // :307 W (0x823B4D48 GetWorldEntityRequestI)
        const WorldEntityRequestInterface* GetWorldEntityRequestInterface() const;                      // :308 R (0x827A3EF0 GetWorldEntityRe)

        // ---- replay status interface ----------------------------------------------
        const ReplayStatusInterface* GetReplayStatusInterface() const;                                  // :310 R (0x827A3F98 GetReplayStatusInter)
        void                         SetReplayStatusInterface(const ReplayStatusInterface*);             // :311 W (0x823B4DF0)

        // ---- player vehicle controls ----------------------------------------------
        void SetPlayerVehicleControls(const PlayerVehicleControls*);                                    // :282 W (0x823B48F8)

        // ---- active payback -------------------------------------------------------
        void SetActivePaybackType(BrnNetwork::EPaybackType);                                            // :317 W (0x823B4F50)
        void SetActivePaybackAggressor(EActiveRaceCarIndex);                                            // :319 W (0x823B5000)

        // ---- ADDITIVE GROW (WorldBridgeInputToEntityModules TU): the consumer-side --
        // getters BridgeInputToEntityModules @0x827ADF88 drains the buffer through.
        //
        // (a) Out-of-line const (read-lock) getters -- real X360 symbols, their own
        //     ledger functions (declaration-only here). Truncated Hex-Rays names are
        //     resolved by the member each returns / the setter each pairs with.
        const TimerStatusInterface*             GetTimerStatusInterface() const;             // :271 R (asm-named)
        const PlayerVehicleControls*            GetPlayerVehicleControls() const;            // :283 R (asm-named)
        BrnNetwork::EPaybackType                GetActivePaybackType() const;                // :316 R (asm-named)
        EActiveRaceCarIndex                     GetActivePaybackAggressor() const;           // :318 R (asm-named)
        const TakedownEventQueue*               GetTakedownEventQueue() const;               // :272 R (IDA "UpdateInputBuffer_")
        const OnlineScoringInterface*           GetOnlineScoringInterface() const;           // :300 R (IDA sub_827A3D98)
        const TriggerManagementInputInterface*  GetTriggerManagementInputInterface() const;  // :276 R (IDA "UpdateInputB")
        const TriggerQueryInputInterface*       GetTriggerQueryInputInterface() const;       // :278 R (IDA sub_827A39A8)
        const GameActionQueue*                  GetGameActionQueue() const;                  // :265 R (IDA "UpdateInputBuffer")

        // (b) Per-active-race-car getters -- X360 HEADER-INLINES (the bridge's asm has
        //     no bl for them, only the range-assert pairs citing THIS header's X360
        //     lines, folded to static text below per the project assert convention).
        u16 GetRaceCarColourIndex(EActiveRaceCarIndex leActiveRaceCarIndex) const            // X360 h:696/:697
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mau16RaceCarColourIndex[leActiveRaceCarIndex];
        }
        bool IsRaceCarColourIndexValid(EActiveRaceCarIndex leActiveRaceCarIndex) const       // X360 h:715/:716
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mabRaceCarColourIndexValid[leActiveRaceCarIndex];
        }
        u16 GetRaceCarPaintFinishIndex(EActiveRaceCarIndex leActiveRaceCarIndex) const       // X360 h:752/:753
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mau16RaceCarPaintFinishIndex[leActiveRaceCarIndex];
        }
        bool IsRaceCarPaintFinishValid(EActiveRaceCarIndex leActiveRaceCarIndex) const       // X360 h:771/:772 (PS3 DWARF name)
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mabRaceCarPaintFinishIndexValid[leActiveRaceCarIndex];
        }
        bool GetLostContact(EActiveRaceCarIndex leActiveRaceCarIndex) const                  // X360 h:791 (combined form)
        {
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT && leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )");
            return mabLostContactThisFrame[leActiveRaceCarIndex];
        }
        bool GetRegainedContact(EActiveRaceCarIndex leActiveRaceCarIndex) const              // X360 h:809 (combined form)
        {
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT && leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID,
                       "( leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT ) && ( leActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID )");
            return mabRegainedContactThisFrame[leActiveRaceCarIndex];
        }
        bool GetCarSelectStatus(EActiveRaceCarIndex leActiveRaceCarIndex) const              // X360 h:829/:830
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mabCarSelectStatus[leActiveRaceCarIndex];
        }
        bool IsCarSelectStatusValid(EActiveRaceCarIndex leActiveRaceCarIndex) const          // X360 h:839/:840
        {
            CGS_ASSERT(leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0, "leActiveRaceCarIndex >= E_ACTIVE_RACE_CAR_INDEX_0");
            CGS_ASSERT(leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT, "leActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT");
            return mabCarSelectStatusValid[leActiveRaceCarIndex];
        }

    private:
        // ---- FROZEN LAYOUT (DWARF :324..354 order; leading arrays X360-offset-proven) ----
        u16  mau16RaceCarColourIndex[8];        // :324  this+2
        u16  mau16RaceCarPaintFinishIndex[8];   // :325  this+18
        bool mabRaceCarColourIndexValid[8];     // :326  this+34
        bool mabRaceCarPaintFinishIndexValid[8];// :327  this+42
        bool mabLostContactThisFrame[8];        // :328  this+50
        bool mabRegainedContactThisFrame[8];    // :329  this+58
        bool mabCarSelectStatus[8];             // :330  this+66
        bool mabCarSelectStatusValid[8];        // :331  this+74
        VehicleInputInterface        mVehicleInputInterface;        // :332
        VehicleDriverInputInterface  mVehicleDriverInputInterface;  // :333
        s32                          miPlayerRaceCarIndex;          // :334
        GameActionQueue              mGameActionQueue;              // :335
        TimerStatusInterface         mTimerStatusInterface;         // :336
        TakedownEventQueue           mTakedownEventQueue;           // :337
        TriggerManagementInputInterface mTriggerManagementInputInterface; // :338
        TriggerQueryInputInterface   mTriggerQueryInputInterface;   // :339
        BrnNetwork::EPaybackType     meActivePaybackType;           // :340
        EActiveRaceCarIndex          meActivePaybackAggressor;      // :341
        TrafficNetworkInputInterface mTrafficNetworkInterface;      // :343
        CrashNetworkInputInterface   mCrashNetworkInterface;        // :344
        PlayerVehicleControls        mPlayerVehicleControls;        // :345
        DebugController              mDebugController;              // :346
        RaceCarRaceDistanceInterface mRaceCarRaceDistanceInterface; // :347
        ScoringInterface             mScoringInterface;             // :348
        bool                         mbControllerActive;            // :349
        WorldEntityRequestInterface  mWorldEntityRequestInterface;  // :350
        ReplayStatusInterface        mReplayStatusInterface;        // :351
        OnlineScoringInterface       mOnlineScoringInterface;       // :354

        // Pin the X360-proven leading-array byte offsets (store-for-store load-bearing).
        // Defined non-inline in BrnWorldModuleIO.cpp so the static_asserts are always compiled
        // (a never-ODR-used inline member would be skipped by MSVC); it is a private member so the
        // offsetof()s have legal access to the private array members.
        static void _AssertLayout();
    };

    // ========================================================================
    // BrnWorldIO::UpdateOutputBuffer  (DWARF BrnWorldModuleIO.h:499)
    //
    // The World module's per-frame "Update" OUTPUT IO-buffer: aggregates the ~25
    // output interfaces/queues the world publishes each frame (vehicle/race-car
    // snapshots, AI route responses, traffic/crash/deformation/effects payloads,
    // the resource/attrib-sys/replay request queues and the game-event queue).
    //
    // LAYOUT (FROZEN, DWARF member order :621-:659; X360 byte offsets attested by
    // the 46 out-of-line accessor bodies -- each member's console offset is cited
    // on its declaration; on this LLP64 host the offsets differ, so only the
    // pointer-invariant leading offset is static_assert-pinned in _AssertLayout()).
    // Every member type is a committed home reused BY NAME (see the include list);
    // the in-class typedefs reproduce the DWARF typedef spellings (:115-:171).
    // Sibling-buffer typedefs the DWARF routes through (OutputBuffer::,
    // OutputBuffer_PostPhysics::, InputBuffer::, PropOutputInterface::) are not
    // committed yet, so those typedefs alias the underlying committed queue types
    // directly, each citing its DWARF line.
    //
    // ACCESSOR SHAPE (X360 binary, authoritative) == the UpdateInputBuffer contract
    // above: ">>3 &1" write-lock => mutable getter / Append / Set,
    // "Not locked for writing"; ">>4 &1" read-lock => const getter,
    // "Not locked for reading".
    // ========================================================================
    struct UpdateOutputBuffer : public CgsModule::IOBuffer
    {
        // ---- DWARF typedef spellings (class-local, :115-:171 + sibling-buffer aliases) ----
        typedef BrnPhysics::Vehicle::VehicleOutputInterface        VehicleOutputInterface;         // :132
        typedef BrnPhysics::Vehicle::VehicleManagerOutputInterface VehicleManagerOutputInterface;  // :133
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface
                                                                   RCEntityActiveRaceCarOutputInterface; // :149
        typedef BrnWorld::RaceCarEntityModuleIO::RCEntityGlobalRaceCarOutputInterface
                                                                   RCEntityGlobalOutputInterface;  // :150
        typedef BrnWorld::TriggerEntityModuleIO::TriggerEntityModuleOutputInterface
                                                                   TriggerEntityModuleOutputInterface;  // :153
        typedef BrnDirector::BrnDirectorVehicleInputInterface      DirectorVehicleInputInterface;  // :159
        typedef BrnAI::AIModuleIO::AICarOutputInterface            AICarOutputInterface;           // :156
        typedef BrnPhysics::ContactSpy::ContactSpyInterface        ContactSpyInterface;            // :161
        // :629 spells OutputBuffer::RouteResponseQueue -- the BrnWorldIO::OutputBuffer sibling is
        // not committed; alias the underlying committed queue (BrnRouteMapModuleIO.h typedef).
        typedef BrnAI::RouteMapModuleIO::RouteResponseQueue        RouteResponseQueue;
        // :630 -> :173 -> EventQueue<AudioCarDataLoadedEvent,16> (DWARF BrnWorldModuleIO.h:1505).
        typedef CgsModule::EventQueue<BrnWorld::RaceCarEntityModuleIO::AudioCarDataLoadedEvent, 16>
                                                                   AudioCarLoadedDataQueue;
        // :115 spells RequestInterface<4096> (== KI_WORLD_EVENT_QUEUE_MAX_SIZE).
        typedef BrnResource::GameDataIO::RequestInterface<KI_WORLD_EVENT_QUEUE_MAX_SIZE>
                                                                   WorldResourceRequestInterface;
        // :118 spells AttribSysRequestInterface<2048>; the committed home is the
        // <2048>-shaped (VariableEventQueue<2048,16>-backed) templated interface.
        typedef CgsAttribSys::AttribSysIO::AttribSysRequestInterface<2048> AttribSysVaultRequestInterface;
        typedef BrnTraffic::BrnTrafficIO::TrafficNetworkOutputInterface TrafficNetworkOutputInterface; // :122
        typedef BrnTraffic::BrnTrafficIO::TrafficSoundOutputInterface   TrafficSoundOutputInterface;   // :123
        typedef BrnTraffic::BrnTrafficIO::TrafficDirectorOutputInterface TrafficDirectorOutputInterface; // :632 member type
        // :124 spells OutputBuffer_PostPhysics::TrafficTypeResponseQueue (sibling not committed);
        // capacity 32 == the committed EventQueue_TrafficTypeResponse_32 instantiation and the
        // X360 528-byte member span (16 hdr + 32*16).
        typedef CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32>
                                                                   TrafficTypeResponseQueue;
        typedef BrnWorld::CrashIO::NetworkOutputInterface          CrashNetworkOutputInterface;    // :128
        typedef BrnPhysics::Deformation::DeformationOutputInterface DeformationOutputInterface;    // :138
        typedef BrnEffects::EffectsEnvironmentInterface            EffectsEnvironmentInterface;    // :164
        typedef BrnWorld::WorldEntityIO::StatusInterface           StatusInterface;                // :644 member type
        // BrnSoundRootSharedIO.h:145
        typedef CgsModule::EventQueue<BrnSound::Module::Io::SoundWorldLoadEvent, 25>
                                                                   SoundWorldLoadInterface;
        typedef BrnReplays::ReplayIO::RequestInterface             ReplayRequestInterface;         // :171
        // :647 spells OutputBuffer_PostPhysics::PropVFXLocatorQueue (sibling not committed);
        // capacity 10 == the committed EventQueue_PropVFXLocatorEvent_10 instantiation and the
        // X360 816-byte member span (16 hdr + 10*80).
        typedef CgsModule::EventQueue<BrnWorld::PropEntityIO::PropVFXLocatorEvent, 10>
                                                                   PropVFXLocatorQueue;
        // :648 spells InputBuffer::GuiEventInputQueue (sibling not committed); the X360 member
        // span 170176..202960 == 32784 == VariableEventQueue<32768,16>
        // (KI_WORLD_RENDER_EVENT_QUEUE_MAX_SIZE + the 16-byte VEQ header).
        typedef CgsModule::VariableEventQueue<KI_WORLD_RENDER_EVENT_QUEUE_MAX_SIZE, 16>
                                                                   GuiEventInputQueue;
        // :650 spells OutputBuffer_PostPhysics::PropBecamePhysicalEventQueue (sibling not
        // committed); capacity 20 == the committed EventQueue_PropBecamePhysicalEvent_20
        // instantiation and the X360 336-byte member span (16 hdr + 20*16).
        typedef CgsModule::EventQueue<BrnWorld::PropEntityIO::PropBecamePhysicalEvent, 20>
                                                                   PropBecamePhysicalEventQueue;
        // :651 spells PropOutputInterface::PropUpdateNotificationQueue == EventQueue<
        // PropUpdateNotification,200> (DWARF BrnPropOutputInterface.h:19; X360 span 12816
        // == 16 hdr + 200*64).
        typedef CgsModule::EventQueue<BrnPhysics::Props::PropUpdateNotification, 200>
                                                                   PropUpdateNotificationQueue;
        typedef CgsSceneManager::SceneManagerIO::TriangleCacheInterface OutTriangleCacheInterface; // :167
        // :141; the X360 AppendGameEventQueue dispatches VariableEventQueue<1536,16>::Append<1536,16>.
        typedef CgsModule::VariableEventQueue<1536, 16>            GameEventQueue;

        // ---- lifecycle (own TUs; declared for the IOBufferStack Create/Destroy path) ----
        void Construct();   // :504  X360 0x827CA0F8 (own TU)
        void Destruct();    // :508  X360 0x827C4A08 (own TU)

        // ---- player race-car indices ----
        EActiveRaceCarIndex GetPlayerActiveRaceCarIndex() const;   // :511 R (0x823B6D80, own TU)
        EGlobalRaceCarIndex GetPlayerGlobalRaceCarIndex() const;   // :514 R (0x823B6E58)

        // ---- triangle cache (vehicle input mirror) ----
        // DWARF :518 spells the param const VehicleInputInterface::InTriangleCacheInterface*
        // (a typedef of the same CgsSceneManager type routed through the not-yet-committed
        // BrnPhysics::Vehicle::VehicleInputInterface); no lock-bit test in the X360 body.
        void AppendTriangleCacheInterface(const OutTriangleCacheInterface* lpTriangleCacheInterface); // :518 (0x8279BAF8)

        // ---- resource request interface ----
        // X360 ledger names both getters GetResourceRequestResourceInterface (a post-FIGS
        // rename; the PS3 DWARF :520/:521 still spells GetResourceRequestInterface).
        const WorldResourceRequestInterface* GetResourceRequestResourceInterface() const; // :520 R (0x823B5780, "GetResour")
        WorldResourceRequestInterface*       GetResourceRequestResourceInterface();       // :521 W (0x827A4440)
        void AppendResourceRequestInterface(const WorldResourceRequestInterface* lpInterface); // :522 W (0x827AD0E8)

        // ---- attrib-sys vault request interface ----
        AttribSysVaultRequestInterface* GetAttribSysVaultRequestInterface();               // :525 W (0x827BCAD0, "GetA")

        // ---- vehicle output interfaces ----
        const VehicleOutputInterface* GetVehicleOutputInterface() const;                   // :528 R (0x823B58D0, "GetVehicleOut")
        VehicleOutputInterface*       GetVehicleOutputInterface();                         // :529 W (0x827A44E8, "GetVehicleOutputInt")
        const VehicleManagerOutputInterface* GetVehicleManagerOutputInterface() const;     // :531 R (0x823B5978, "GetVeh")

        // ---- director vehicle input interface ----
        const DirectorVehicleInputInterface* GetDirectorVehicleInputInterface() const;     // :535 R (0x823B5A20, "GetDirector")
        void SetDirectorVehicleInputInterface(const DirectorVehicleInputInterface* lpInterface); // :536 W (0x827AD1A0)

        // ---- race-car entity / trigger output interfaces ----
        void SetRaceCarGlobalOutputInterface(const RCEntityGlobalOutputInterface* lpInterface);   // :539 W (0x827A4638)
        void SetTriggerEntityOutputInterface(const TriggerEntityModuleOutputInterface* lpInterface); // :542 W (0x827A46F0)
        void SetActiveRaceCarOutputInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface); // :545 W (0x827A47A8)
        void SetReplayActiveRaceCarOutputInterface(const RCEntityActiveRaceCarOutputInterface* lpInterface); // :548 W (0x827A4860)

        // ---- contact spy ----
        const ContactSpyInterface* GetContactSpyInterface() const;                         // :550 R (0x823B5D68, "GetContactSpy")
        ContactSpyInterface*       GetContactSpyInterface();                               // :551 W (0x827A4918, "GetContactSpyInterf")

        // ---- AI car output ----
        const AICarOutputInterface* GetAICarOutputInterface() const;                       // :554 R (0x823B5E10, "GetAICarOutputInt")
        void SetAICarOutputInterface(const AICarOutputInterface* lpInterface);             // :555 W (0x827A49C0)

        // ---- player vehicle controls ----
        const PlayerVehicleControls* GetPlayerVehicleControls() const;                     // :557 R (0x823B5EB8)
        void SetPlayerVehicleControls(const PlayerVehicleControls* lpControls);            // :558 W (0x827BCB78)

        // ---- AI route responses ----
        void AppendRouteResponseQueue(const RouteResponseQueue* lpQueue);                  // :561 W (0x827AA5A0)

        // ---- traffic network / sound / director ----
        const TrafficNetworkOutputInterface* GetTrafficNetworkOutputInterface() const;     // :563 R (0x823B6008, "G")
        void SetTrafficNetworkOutputInterface(const TrafficNetworkOutputInterface* lpInterface); // :564 W (0x827AD258)
        const TrafficSoundOutputInterface* GetTrafficSoundOutputInterface() const;         // :566 R (0x823B60B0, "Get")
        void SetTrafficSoundOutputInterface(const TrafficSoundOutputInterface* lpInterface); // :567 W (0x827A4A78)
        const TrafficDirectorOutputInterface* GetTrafficDirectorOutputInterface() const;   // :569 R (0x823B6158, "::")
        void SetTrafficDirectorOutputInterface(const TrafficDirectorOutputInterface* lpInterface); // :570 W (0x827A8AB0)

        // ---- crash network ----
        const CrashNetworkOutputInterface* GetCrashNetworkOutputInterface() const;         // :572 R (0x823B6200, "GetCrashNetwork")
        void SetCrashNetworkOutputInterface(const CrashNetworkOutputInterface* lpInterface); // :573 W (0x827AD310)

        // ---- game event queue ----
        const GameEventQueue* GetGameEventQueue() const;                                   // :575 R (0x823B62A8, "GetGameEv")
        GameEventQueue*       GetGameEventQueue();                                         // :576 W (0x827A4B30, "GetGameEventQue")
        void AppendGameEventQueue(const GameEventQueue* lpQueue);                          // :577 W (0x827AD3C8)

        // ---- deformation ----
        const DeformationOutputInterface* GetDeformationOutputInterface() const;           // :579 R (0x823B6350, "GetDe")
        void SetDeformationOutputInterface(const DeformationOutputInterface* lpInterface); // :580 W (0x827AA658)

        // ---- traffic type responses ----
        void AppendTrafficTypeResponseQueue(const TrafficTypeResponseQueue* lpQueue);      // :586 W (0x827AA710)

        // ---- effects environment ----
        const EffectsEnvironmentInterface* GetEffectsEnvironmentInterface() const;         // :588 R (0x823B64A0, "GetEffectsEnviron")
        EffectsEnvironmentInterface*       GetEffectsEnvironmentInterface();               // :589 W (0x827BCC30, "GetEffectsEnvironmentIn")

        // ---- world entity status ----
        const StatusInterface* GetWorldEntityStatusInterface() const;                      // :591 R (0x823B6548, "GetWorldEntitySt")
        void SetWorldEntityStatusInterface(const StatusInterface* lpInterface);            // :592 W (0x827A4BD8)

        // ---- sound world load ----
        void AppendSoundWorldLoadInterface(const SoundWorldLoadInterface* lpInterface);    // :595 W (0x827AA7C8)

        // ---- replay requests ----
        const ReplayRequestInterface* GetReplayRequestInterface() const;                   // :597 R (0x823B6698, "GetReplayRequestIn")
        void AppendReplayRequestInterface(const ReplayRequestInterface* lpInterface);      // :598 W (0x827A4CA8)

        // ---- prop VFX locators ----
        void SetPropVFXLocatorQueue(const PropVFXLocatorQueue* lpQueue);                   // :600 W (0x827AA880)

        // ---- gui events (own TU: one of the pair is X360 @ 0x823B6890) ----
        const GuiEventInputQueue* GetGuiEventQueue() const;                                // :607 (own TU)
        GuiEventInputQueue*       GetGuiEventQueue();                                      // :608 (own TU)

        // ---- prop physical/update notifications ----
        void AppendPropBecamePhysicalEventQueue(const PropBecamePhysicalEventQueue* lpQueue); // :614 W (0x827AA938)
        void AppendPropUpdateNotificationQueue(const PropUpdateNotificationQueue* lpQueue);   // :618 W (0x827AA9F0)

    private:
        // ---- FROZEN LAYOUT (DWARF :621-:659 order; X360 byte offsets attested by the
        //      accessor bodies cited above; console spans noted where the asm proves them) ----
        VehicleOutputInterface               mVehicleOutputInterface;              // :621  X360 +16      (27664 B)
        VehicleManagerOutputInterface        mVehicleManagerOutputInterface;       // :622  X360 +27680   (2176 B)
        RCEntityActiveRaceCarOutputInterface mActiveRaceCarOutputInterface;        // :623  X360 +29856   (10480 B, XMemCpy @0x827A47A8)
        RCEntityActiveRaceCarOutputInterface mReplayActiveRaceCarOutputInterface;  // :624  X360 +40336   (10480 B, XMemCpy @0x827A4860)
        TriggerEntityModuleOutputInterface   mTriggerEntityOutputInterface;        // :625  X360 +50816   (1040 B, memcpy @0x827A46F0)
        DirectorVehicleInputInterface        mDirectorVehicleInputInterface;       // :626  X360 +51856   (816 B)
        RCEntityGlobalOutputInterface        mRaceCarGlobalOutputInterface;        // :627  X360 +52672   (2416 B, XMemCpy @0x827A4638)
        AICarOutputInterface                 mAICarOutputInterface;                // :628  X360 +55088   (5352 B, memcpy @0x827A49C0)
        RouteResponseQueue                   mRouteResponseQueue;                  // :629  X360 +60440
        AudioCarLoadedDataQueue              mAudioCarLoadedDataQueue;             // :630  X360 +142632
        TrafficDirectorOutputInterface       mTrafficDirectorOutputInterface;      // :632  X360 +143040  (3616 B)
        ContactSpyInterface                  mContactSpyInterface;                 // :634  X360 +146656  (4 B)
        WorldResourceRequestInterface        mResourceRequestInterface;            // :635  X360 +146660  (4112 B)
        PlayerVehicleControls                mPlayerVehicleControls;               // :636  X360 +150772  (60 B, memcpy @0x827BCB78)
        AttribSysVaultRequestInterface       mAttribSysVaultRequestInterface;      // :637  X360 +150832  (2064 B)
        TrafficNetworkOutputInterface        mTrafficNetworkOutputInterface;       // :638  X360 +152896  (144 B)
        TrafficSoundOutputInterface          mTrafficSoundOutputInterface;         // :639  X360 +153040  (2576 B)
        TrafficTypeResponseQueue             mTrafficTypeResponseQueue;            // :640  X360 +155616  (528 B)
        CrashNetworkOutputInterface          mCrashNetworkOutputInterface;         // :641  X360 +156144  (1936 B)
        DeformationOutputInterface           mDeformationOutputInterface;          // :642  X360 +158080  (10992 B)
        EffectsEnvironmentInterface          mEffectsEnvironmentInterface;         // :643  X360 +169072  (16 B)
        StatusInterface                      mWorldEntityStatusInterface;          // :644  X360 +169088  (5 B, 5-byte copy @0x827A4BD8)
        SoundWorldLoadInterface              mSoundWorldLoadInterface;             // :645  X360 +169096  (212 B)
        ReplayRequestInterface               mReplayRequestInterface;              // :646  X360 +169308
        PropVFXLocatorQueue                  mPropVFXLocatorQueue;                 // :647  X360 +169360  (816 B)
        GuiEventInputQueue                   mGuiEventQueue;                       // :648  X360 +170176  (32784 B)
        PropBecamePhysicalEventQueue         mPropBecamePhysicalEventQueue;        // :650  X360 +202960  (336 B)
        PropUpdateNotificationQueue          mPropUpdateNotificationQueue;         // :651  X360 +203296  (12816 B)
        OutTriangleCacheInterface            mTriangleCacheInterface;              // :654  X360 +216112  (4 B, ptr store @0x8279BAF8)
        GameEventQueue                       mGameEventQueue;                      // :658  X360 +216116  (1552 B)
        bool                                 mbWorldWantsDebugControllerFocus;     // :659  X360 +217668

        // Pin the pointer-invariant X360-proven facts (see BrnWorldModuleIO_UpdateOutputBuffer.cpp).
        static void _AssertLayout();
    };
}
