// ============================================================================
// b5-decomp/src/GameSource/GameState/BrnGameStateModuleIO.h
// ============================================================================
// GameStateModuleIO is a NAMESPACE. The three per-frame IO payload buffers the GameState
// module exchanges (PreWorldInputBuffer, PostWorldInputBuffer, OutputBuffer) all derive from
// CgsModule::IOBuffer (lock state machine; status byte at offset 0). The lock-guarded Get*/Set*
// accessors first assert the right lock then return &member-at-X360-offset.
//
// MINIMAL SLICE NOTE: each buffer's real layout (from the DWARF) is dozens of large composite
// interface members from other, un-reconstructed modules. Only the members the accessors in the
// GameStateModuleIO + OutputBuffer TUs touch are modelled, each pinned to its exact X360 byte
// offset with explicit u8 storage for the gaps. The interface types are forward-declared
// incomplete classes; the touched member is given as raw aligned storage of the correct width at
// the correct offset so the buffer's later (own-TU) full reconstruction can replace the
// storage+padding with the real typed members without moving anything.

#pragma once

#include <cstddef>   // offsetof (PreWorldInputBuffer layout asserts)
#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"      // CgsModule::IOBuffer (base; lock state)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"    // CgsModule::EventQueue<T,N> (mTrafficTypeResponseQueue)
#include "GameSource/BurnoutConstants.h"                    // EActiveRaceCarIndex (enum : s32)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::EPaybackType (enum : s32)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h" // BrnTraffic::BrnTrafficIO::TrafficTypeResponse
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h" // BrnAI::AIModuleIO::AICarOutputInterface (complete; embedded by value @ +0xAAC0)
#include "GameSource/Network/BrnNetworkModuleIO.h"                 // BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface (embedded @ +0x36B8)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusInterface (embedded @ +0x2CC8)

namespace BrnGameState
{
namespace GameStateModuleIO
{
    // ---- forward declarations of the interface member types (own TUs) --------
    // Full layouts live with their own reconstructions; only pointers are returned by the
    // accessors, so incomplete declarations suffice.
    class GameEventQueue;                  // == VariableEventQueue<1536,16>
    class TakedownEventInputQueueType;     // PreWorldInputBuffer +0x660
    // PreWorldInputBuffer +0x36B8: the network player-results interface is BrnNetwork's committed
    // PlayerResultsInterface (operator= @0x823B8F68); alias it so SetNetworkPlayerResultsInterface
    // can member-copy into it and the const getter still spells NetworkPlayerResultsInterface.
    typedef BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface NetworkPlayerResultsInterface;

    // ========================================================================
    // PreWorldInputBuffer member types (X360-recovered layouts).
    // ========================================================================

    // Controller-input button-state block embedded at PreWorldInputBuffer +0x34 (0x18 bytes).
    // SetButtonPressed (X360 0x823BA240) fills the 21 leading bool flags from the per-player pad
    // action-info record; GetControllerInput returns it read-locked. Field semantics are mostly
    // unknown (no DWARF/leak shape), so each bool is named by the controller action whose status
    // bit drives it where the bit position makes it clear, else by its source action-record offset.
    struct ControllerInput
    {
        bool mbDrivingActive;        // +0x00 (src+0x18C bit1) -- ControllerInput +0x34
        bool mbReverseHeld;          // +0x01 (src+0x16C bit1) -- +0x35
        bool mbHandbrakeHeld;        // +0x02 (src+0x174 bit1) -- +0x36
        bool mbBoostHeld;            // +0x03 (src+0x1AC bit1) -- +0x37
        bool mbAction14C;            // +0x04 (src+0x14C bit1) -- +0x38
        bool mbAction154;            // +0x05 (src+0x154 bit1) -- +0x39
        bool mbAction15C;            // +0x06 (src+0x15C bit1) -- +0x3A
        bool mbAction164;            // +0x07 (src+0x164 bit1) -- +0x3B
        bool mbAction1B4Pressed;     // +0x08 (src+0x1B4 bit1) -- +0x3C
        bool mbAction1B4Held;        // +0x09 (src+0x1B4 bit0) -- +0x3D
        bool mbAction1BCPressed;     // +0x0A (src+0x1BC bit1) -- +0x3E
        bool mbAction1BCHeld;        // +0x0B (src+0x1BC bit0) -- +0x3F
        bool mbAction05CPressed;     // +0x0C (src+0x05C bit1) -- +0x40
        bool mbAction1BCHeldDup;     // +0x0D (src+0x1BC bit0) -- +0x41
        bool mbAction1B4And1BCHeld;  // +0x0E ((src+0x1B4 bit0) && (src+0x1BC bit0)) -- +0x42
        bool mbAction024Pressed;     // +0x0F (src+0x024 bit1) -- +0x43
        bool mbAction1D4Pressed;     // +0x10 (src+0x1D4 bit1) -- +0x44
        bool mbBothSticksDeflected;  // +0x11 ((src+0x00 > 0.25) && (src+0x08 > 0.25)) -- +0x45
        bool mbAction004Held;        // +0x12 (src+0x004 bit0) -- +0x46
        bool mbAction1E4Pressed;     // +0x13 (src+0x1E4 bit1) -- +0x47
        bool mbAction13CPressed;     // +0x14 (src+0x13C bit1) -- +0x48
        u8   maPad0x15[0x03];        // +0x15..+0x18 trailing pad to close the 0x18-byte block
    };

    // Per-player pad action-info source record fed to SetButtonPressed (the caller passes
    // PadInfo+0x18, X360 BrnGameModule::BridgeControllerToGameState 0x823CD738). Only the words
    // SetButtonPressed reads are named (each is an action-mapping status word: bit1 == pressed this
    // frame, bit0 == held); the two leading f32 are analogue stick deflections. Gaps are storage.
    struct ControllerActionSource
    {
        f32 mfStickX;                 // +0x000 (> 0.25 test)
        u32 mStatus004;               // +0x004 (bit0 held)
        f32 mfStickY;                 // +0x008 (> 0.25 test)
        u8  maPad0x00C[0x024 - 0x00C];
        u32 mStatus024;               // +0x024
        u8  maPad0x028[0x05C - 0x028];
        u32 mStatus05C;               // +0x05C
        u8  maPad0x060[0x13C - 0x060];
        u32 mStatus13C;               // +0x13C
        u8  maPad0x140[0x14C - 0x140];
        u32 mStatus14C;               // +0x14C
        u8  maPad0x150[0x154 - 0x150];
        u32 mStatus154;               // +0x154
        u8  maPad0x158[0x15C - 0x158];
        u32 mStatus15C;               // +0x15C
        u8  maPad0x160[0x164 - 0x160];
        u32 mStatus164;               // +0x164
        u8  maPad0x168[0x16C - 0x168];
        u32 mStatus16C;               // +0x16C
        u8  maPad0x170[0x174 - 0x170];
        u32 mStatus174;               // +0x174
        u8  maPad0x178[0x18C - 0x178];
        u32 mStatus18C;               // +0x18C
        u8  maPad0x190[0x1AC - 0x190];
        u32 mStatus1AC;               // +0x1AC
        u8  maPad0x1B0[0x1B4 - 0x1B0];
        u32 mStatus1B4;               // +0x1B4
        u8  maPad0x1B8[0x1BC - 0x1B8];
        u32 mStatus1BC;               // +0x1BC
        u8  maPad0x1C0[0x1D4 - 0x1C0];
        u32 mStatus1D4;               // +0x1D4
        u8  maPad0x1D8[0x1E4 - 0x1D8];
        u32 mStatus1E4;               // +0x1E4
    };

    // Timer-status payload embedded at PreWorldInputBuffer +0x04 (0x30 bytes == two 0x18-byte
    // entries). SetTimerStatusInterface (X360 0x823B8D08) copies both entries field-for-field from
    // the input source; the read-locked Get accessor returns it. Each entry is {s32, f32, f32, u8,
    // s32, f32}; the X360 build copies entry[0] then entry[1] (this+0x04, this+0x1C).
    struct TimerStatusInterface
    {
        struct Entry
        {
            s32 miWord00;   // +0x00
            f32 mfValue04;  // +0x04
            f32 mfValue08;  // +0x08
            u8  mbFlag0C;   // +0x0C
            u8  maPad0x0D[0x10 - 0x0D];
            s32 miWord10;   // +0x10
            f32 mfValue14;  // +0x14
        };
        Entry maEntries[2];  // 2 * 0x18 == 0x30 bytes
    };
    class VehicleOutputInterface;          // PostWorldInputBuffer +0x220
    // DWARF (BrnGameStateModuleIO.h:73,181,239): the PostWorldInputBuffer AICarOutputInterface
    // is a typedef IMPORT of the AI module's BrnAI::AIModuleIO::AICarOutputInterface (mirrors
    // the ActiveRaceCarOutputInterface == RCEntityActiveRaceCarOutputInterface pattern). It is
    // embedded by value @ +0xAAC0, so the real (complete) type is included above and aliased here.
    typedef BrnAI::AIModuleIO::AICarOutputInterface AICarOutputInterface; // PostWorldInputBuffer +0xAAC0
    class GameActionQueue;                 // OutputBuffer +0x04
    class ResourceRequestInterface;        // OutputBuffer +0x3414 (RequestInterface<3072>)
    class TakedownEventOutputQueueType;    // OutputBuffer +0x4040
    class GameStateToGuiInterface;         // OutputBuffer +0x4450
    class RaceCarRaceDistanceInterface;    // OutputBuffer +0x2A48C

    // Placeholder member types for the OutputBuffer interface members the OutputBuffer TU
    // returns by named pointer. Swap for the real DWARF types (TimerRequestInterface,
    // FrameRateTypeRequestInterface, InputBuffer::GuiEventQueue) when those are homed.
    struct OutputBufferTimerRequestInterface     { u8 maOpaque[16]; };
    struct OutputBufferFrameRateTypeReqInterface { u8 maOpaque[12]; };
    struct OutputBufferGuiEventQueue             { u8 maOpaque[1008]; };

    // TODO(conductor-review): CgsSystem::Time has no committed home yet (CgsTime.h missing).
    // Modelled here as the DWARF shape {s32 miSeconds; f32 mfFraction;} so the elapsed-time
    // accessors compile; replace with the real CgsSystem::Time when CgsTime.h is reconstructed.
    struct OutputBufferTime
    {
        s32 miSeconds;
        f32 mfFraction;
    };

    // ========================================================================
    // Minimal member types homed by the class:BrnGameState catch-all TU's remaining
    // GameStateModuleIO &member accessors. Each is a named opaque payload the accessor
    // returns the address of; their full (own-TU) reconstructions will widen the
    // opaque storage to real typed members without moving the surrounding anchors.
    // ========================================================================

    // PostWorldInputBuffer +0x10. DWARF (:198/199, :232): the per-frame race-car crash-event
    // queue == VehicleManagerOutputInterface::RaceCarCrashEventQueue (an
    // EventQueue<RaceCarCrashEvent,8>). Read-locked by ModeManager::ProcessPlayerCrashes;
    // modelled minimally as opaque storage spanning up to the next known member (VehicleOutput
    // @ +0x220). Swap for the real EventQueue<RaceCarCrashEvent,8> when that physics type is homed.
    struct RaceCarCrashEventQueue { u8 maOpaque[0x220 - 0x10]; };

    // PostWorldInputBuffer +0x6E34. DWARF (:207/208, :235): the trigger-entity-module output
    // interface (BrnWorld::TriggerEntityModuleIO::TriggerEntityModuleOutputInterface). Returned
    // read-locked (TriggerQueryManager::PostWorldUpdate) and write-locked (BridgeWorldToGameState).
    // Modelled minimally as a named opaque payload; swap for the real interface when it is homed.
    struct TriggerEntityModuleOutputInterface { u8 maOpaque[16]; };

    // PreWorldInputBuffer +0x7B0. DWARF (:140/141, :166): the network-to-game-state input
    // interface (NetworkToGameStateInterface). Read-locked by BurnoutSkillzManager::PreWorldUpdate.
    // Modelled minimally as a named opaque payload; swap for the real interface when it is homed.
    struct NetworkToGameStateInterface { u8 maOpaque[16]; };

    // OutputBuffer +0x9050. DWARF (:283/284, :347): the trigger-management input interface
    // (OutputBuffer::TriggerManagementInputInterface). Returned read-locked
    // (BridgeGameStateToWorld) and write-locked (ModeManager::StartGameMode). Modelled minimally
    // as a named opaque payload; swap for the real interface when it is homed.
    struct TriggerManagementInputInterface { u8 maOpaque[16]; };

    // OutputBuffer +0x43AC. DWARF (:293, :344): the game-state-to-controller output interface
    // (GameStateToControllerInterface). Write-locked by GameStateInviteManager::Update.
    // Modelled minimally as a named opaque payload; swap for the real interface when it is homed.
    struct GameStateToControllerInterface { u8 maOpaque[16]; };

    // ------------------------------------------------------------------------
    // Container element types for the two generic-container catch-all funcs.
    // ------------------------------------------------------------------------

    // 32-byte trigger-query record. Element of an Array<TriggerQueryRecord,16> indexed by
    // Array<T,16>::operator[] const (X360 0x8235FBE0); TriggerQueryManager::PreWorldUpdate reads
    // the two named words at +0x10/+0x14, the remaining bytes have no attested semantics and are
    // opaque storage closing the 32-byte stride. (No DWARF/leak shape -- recovered purely from the
    // caller's field reads.)
    struct TriggerQueryRecord
    {
        u8  maHeader[0x10]; // +0x00 leading block (no attested semantics)
        s32 miField10;      // +0x10 (read by TriggerQueryManager::PreWorldUpdate)
        s32 miField14;      // +0x14 (read by TriggerQueryManager::PreWorldUpdate)
        u8  maTail[0x20 - 0x18]; // +0x18 pad to the 32-byte stride
    };

    // 12-byte GUI-interface event. Element of a CgsModule::BaseEventQueue<GuiInterfaceEvent>
    // indexed by BaseEventQueue<T>::GetEvent(s32) const (X360 0x823AC338), drained by
    // BrnGameModule::TranslateGuiInterfaceToGuiEvents. No DWARF/leak shape -- only the 12-byte
    // stride is X360-attested (the accessor's `12*a2 + *a1`), so the three words are opaque storage.
    struct GuiInterfaceEvent
    {
        u32 muWord00; // +0x00
        u32 muWord04; // +0x04
        u32 muWord08; // +0x08
    };

    // ========================================================================
    // PreWorldInputBuffer  (DWARF BrnGameStateModuleIO.h:90)
    // ========================================================================
    struct PreWorldInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x823632F8 (read-lock; "Not locked for reading", line 377)
        const ControllerInput*                GetControllerInput() const;
        // X360 0x8231CD80 (read-lock; "Not locked for reading", line 130)
        const GameEventQueue*                 GetGameEventQueue() const;
        // X360 0x823B8C60 (write-lock; "Not locked for writing", line 131)
        GameEventQueue*                       GetGameEventQueue();
        // X360 0x823B8E18 (write-lock; "Not locked for writing", line 138)
        TakedownEventInputQueueType*          GetTakedownEventInputQueue();
        // X360 0x8231D020 (read-lock; "Not locked for reading", line 149)
        const NetworkPlayerResultsInterface*  GetNetworkPlayerResultsInterface() const;
        // X360 0x8231CED0 (read-lock; "Not locked for reading", line 140) -- read-side accessor for
        // the network-to-game-state input interface (this+0x7B0). class:BrnGameState catch-all TU;
        // BurnoutSkillzManager::PreWorldUpdate reads it read-locked.
        const NetworkToGameStateInterface*    GetNetworkToGameStateInterface() const;
        // X360 0x8231CF78 -- read-side accessor for the embedded in-game player-status interface
        // (this+0x2CC8). ADDITIVE GROW (declare-only) for the BrnMugshotManager TU:
        // UpdateCameraStatusData reads each player's active-race-car slot + camera status off it.
        // Body lands with the GameStateModuleIO TU (mirrors the other read-locked &member getters).
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* GetPlayerStatusInterface() const;

        // ---- this TU's 5 functions ----
        // X360 0x8231CE28 (read-lock; "Not locked for reading", line 133) -- read-side accessor for
        // the timer-status payload (this+0x04). (IDA spelled this Get returning unsigned char* this+4.)
        const TimerStatusInterface* GetTimerStatusInterface() const;
        // X360 0x823B8D08 (write-lock; line 135) -- copies a TimerStatusInterface into this+0x04.
        void SetTimerStatusInterface(const TimerStatusInterface* lpSource);
        // X360 0x823BA240 (write-lock; lines 393/394) -- derives the controller button-state block
        // (this+0x34) from the per-player pad action-info source.
        void SetButtonPressed(const ControllerActionSource* lpActionInfo);
        // X360 0x823C9550 (write-lock; line 147) -- copies an InGamePlayerStatusInterface into this+0x2CC8.
        void SetPlayerStatusInterface(const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface* lpSource);
        // X360 0x823C53D0 (write-lock; line 150) -- copies a PlayerResultsInterface into this+0x36B8.
        void SetNetworkPlayerResultsInterface(const NetworkPlayerResultsInterface* lpSource);

    private:
        // --- members pinned to X360 offsets (gaps = padding) -----------------
        u8                   maPad0x01[0x04 - sizeof(CgsModule::IOBuffer)]; // status byte end -> 0x04
        TimerStatusInterface mTimerStatusInterface;                         // @ +0x0004 (0x30 bytes)
        ControllerInput      mControllerInput;                              // @ +0x0034 (0x18 bytes)
        u8  mGameEventQueueStorage[0x660 - 0x4C];                       // GameEventQueue   @ +0x004C
        u8  mTakedownEventInputQueueStorage[0x7B0 - 0x660];            // TakedownEventQueue@ +0x0660
        NetworkToGameStateInterface mNetworkToGameStateInterface;      // @ +0x07B0 (named opaque)
        u8  maPadToPlayerStatus[0x2CC8 - (0x7B0 + sizeof(NetworkToGameStateInterface))]; // -> +0x2CC8
        BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface mPlayerStatusInterface; // @ +0x2CC8
        u8  maPadToNetworkResults[0x36B8 - (0x2CC8 + sizeof(BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface))]; // -> +0x36B8
        NetworkPlayerResultsInterface mNetworkPlayerResultsInterface;  // @ +0x36B8 (PlayerResultsInterface, 224B)

        // Compile-time offset guards (private members -> assert from a member-fn context).
        static void _AssertLayout()
        {
            static_assert(sizeof(ControllerInput) == 0x18, "ControllerInput must be 0x18 bytes");
            static_assert(sizeof(TimerStatusInterface) == 0x30, "TimerStatusInterface must be 0x30 bytes");
            static_assert(offsetof(PreWorldInputBuffer, mTimerStatusInterface)          == 0x04,   "mTimerStatusInterface @ +0x04");
            static_assert(offsetof(PreWorldInputBuffer, mControllerInput)               == 0x34,   "mControllerInput @ +0x34");
            static_assert(offsetof(PreWorldInputBuffer, mGameEventQueueStorage)         == 0x4C,   "mGameEventQueueStorage @ +0x4C");
            static_assert(offsetof(PreWorldInputBuffer, mTakedownEventInputQueueStorage) == 0x660,  "TakedownEventInputQueue @ +0x660");
            static_assert(offsetof(PreWorldInputBuffer, mPlayerStatusInterface)         == 0x2CC8, "mPlayerStatusInterface @ +0x2CC8");
            static_assert(offsetof(PreWorldInputBuffer, mNetworkPlayerResultsInterface) == 0x36B8, "mNetworkPlayerResultsInterface @ +0x36B8");
        }
    };

    // ========================================================================
    // PostWorldInputBuffer  (DWARF BrnGameStateModuleIO.h:182)
    // ========================================================================
    struct PostWorldInputBuffer : public CgsModule::IOBuffer
    {
        // X360 0x8231D218 (read-lock; "Not locked for reading", line 201)
        const VehicleOutputInterface* GetVehicleOutputInterface() const;
        // X360 0x823B9300 (write-lock; "Not locked for writing", line 202)
        VehicleOutputInterface*       GetVehicleOutputInterface();
        // X360 0x8231D0C8 (read-lock; "Not locked for reading", line 195)
        const GameEventQueue*         GetGameEventQueue() const;
        // X360 0x823B91B0 (write-lock; "Not locked for writing", line 196)
        GameEventQueue*               GetGameEventQueue();
        // X360 0x8231D410 (read-lock; "Not locked for reading", line 216)
        const AICarOutputInterface*   GetAICarOutputInterface() const;
        // X360 0x823B9648 (write-lock; "Not locked for writing", line 217) -- non-const twin
        AICarOutputInterface*         GetAICarOutputInterface();

        // X360 0x823C9600 (write-lock; "Not locked for writing", line 220) -- forwards to the
        // traffic-type response queue's BaseEventQueue<T>::Append.
        bool AppendTrafficTypeResponseQueue(
                const CgsModule::BaseEventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse>& lSource);

        // X360 0x8231D170 (read-lock; "Not locked for reading", line 198) -- read-side accessor for
        // the race-car crash-event queue (this+0x10). class:BrnGameState catch-all TU;
        // ModeManager::ProcessPlayerCrashes reads it read-locked.
        const RaceCarCrashEventQueue* GetRaceCarCrashEventQueue() const;
        // X360 0x82362A30 (read-lock; "Not locked for reading", line 207) -- read-side accessor for
        // the trigger-entity-module output interface (this+0x6E34). TriggerQueryManager::PostWorldUpdate.
        const TriggerEntityModuleOutputInterface* GetTriggerEntityOutputInterface() const;
        // X360 0x823B9450 (write-lock; "Not locked for writing", line 208) -- non-const twin of the
        // above (this+0x6E34). BridgeWorldToGameState writes it write-locked.
        TriggerEntityModuleOutputInterface*       GetTriggerEntityOutputInterface();

    private:
        u8  maPadToRaceCarCrashEventQueue[0x10 - sizeof(CgsModule::IOBuffer)]; // base end -> 0x0010
        RaceCarCrashEventQueue mRaceCarCrashEventQueue;               // @ +0x0010 (named opaque -> +0x220)
        u8  mVehicleOutputInterfaceStorage[0x6E34 - 0x220];          // VehicleOutputInterface @ +0x0220
        TriggerEntityModuleOutputInterface mTriggerEntityOutputInterface; // @ +0x6E34 (named opaque)
        u8  mPostVehicleOutputStorage[0xA4B0 - (0x6E34 + sizeof(TriggerEntityModuleOutputInterface))]; // -> +0xA4B0
        u8  mGameEventQueueStorage[0xAAC0 - 0xA4B0];                  // GameEventQueue         @ +0xA4B0
        // Real, complete AICarOutputInterface (BrnAI::AIModuleIO::AICarOutputInterface). sizeof == 0x14E8
        // (== 0xBFA8 - 0xAAC0), 4-aligned, so it occupies exactly the former placeholder span and leaves
        // mTrafficTypeResponseQueue pinned at +0xBFA8.
        AICarOutputInterface mAICarOutputInterface;                   // AICarOutputInterface   @ +0xAAC0
        // TrafficTypeResponse query-response queue @ +0xBFA8 (49064). Fixed-capacity EventQueue<...,32>;
        // 16B element stride == sizeof(TrafficTypeResponse). AppendTrafficTypeResponseQueue forwards to it.
        CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32> mTrafficTypeResponseQueue; // +0xBFA8
    };

    // ========================================================================
    // OutputBuffer  (DWARF BrnGameStateModuleIO.h:253)
    //
    // Merged layout: the GameStateModuleIO TU's 5 OutputBuffer accessors (GameActionQueue +0x04,
    // ResourceRequestInterface +0x3414, TakedownEventOutputQueue +0x4040, GameStateToGuiInterface
    // +0x4450, RaceCarRaceDistanceInterface +0x2A48C) and the OutputBuffer TU's 18 accessors
    // (Timer/FrameRate/Gui interfaces, payback type/aggressor, elapsed time, the three valid/active
    // bools) all target the same struct, so the union of touched members is pinned here by byte
    // offset. Every gap is explicit u8 storage; named anchors will not move when the un-homed
    // member types are widened to their real DWARF layouts by their own TUs.
    // ========================================================================
    struct OutputBuffer : public CgsModule::IOBuffer
    {
        // ---- GameStateModuleIO TU accessors ----
        // X360 0x8231D4B8 (write-lock; "Not locked for writing", line 266)
        GameActionQueue*                  GetGameActionQueue();
        // X360 0x823B9798 (read-lock;  "Not locked for reading", line 268)
        const ResourceRequestInterface*   GetResourceRequestInterface() const;
        // X360 0x82362B80 (write-lock; "Not locked for writing", line 272)
        TakedownEventOutputQueueType*     GetTakedownEventOutputQueue();
        // X360 0x8231D8A8 (write-lock; "Not locked for writing", line 296)
        GameStateToGuiInterface*          GetGameStateToGuiInterface();
        // X360 0x823B9D80 (read-lock; "Not locked for reading", line 295) -- const twin of the above.
        // (class:BrnGameState catch-all TU; the BridgeGameStateToGui consumer reads it read-locked.)
        const GameStateToGuiInterface*    GetGameStateToGuiInterface() const;
        // X360 0x823630F0 (write-lock; "Not locked for writing", line 311)
        RaceCarRaceDistanceInterface*     GetRaceCarRaceDistanceInterface();
        // X360 0x82362D78 (write-lock; "Not locked for writing", line 293) -- write-side accessor for
        // the game-state-to-controller output interface (this+0x43AC). GameStateInviteManager::Update.
        GameStateToControllerInterface*   GetGameStateToControllerInterface();
        // X360 0x823B9AE0 (read-lock; "Not locked for reading", line 283) -- read-side accessor for
        // the trigger-management input interface (this+0x9050). BridgeGameStateToWorld.
        const TriggerManagementInputInterface* GetTriggerManagementInputInterface() const;
        // X360 0x8231D758 (write-lock; "Not locked for writing", line 284) -- non-const twin of the
        // above (this+0x9050). ModeManager::StartGameMode writes it write-locked.
        TriggerManagementInputInterface*       GetTriggerManagementInputInterface();

        // ---- OutputBuffer TU accessors ----
        // X360 0x8231D560 (write-lock; line 269) -- non-const twin of GetResourceRequestInterface()
        ResourceRequestInterface*                GetResourceRequestInterface();
        // X360 0x8231D608 (write, line 275) / 0x823B98E8 (read, line 274)
        OutputBufferTimerRequestInterface*       GetTimerRequestInterface();
        const OutputBufferTimerRequestInterface* GetTimerRequestInterface() const;
        // X360 0x8231D6B0 (write, line 278) / 0x823B9990 (read, line 277)
        OutputBufferFrameRateTypeReqInterface*       GetFrameRateTypeRequestInterface();
        const OutputBufferFrameRateTypeReqInterface* GetFrameRateTypeRequestInterface() const;
        // X360 0x82362C28 (write, line 281) / 0x823B9A38 (read, line 280)
        OutputBufferGuiEventQueue*       GetGuiEventQueue();
        const OutputBufferGuiEventQueue* GetGuiEventQueue() const;

        // Active payback (meActivePaybackType @+173180, meActivePaybackAggressor @+173184)
        BrnNetwork::EPaybackType GetActivePaybackType() const;                          // 0x823B9E28 read, line 298
        void                     SetActivePaybackType(BrnNetwork::EPaybackType lePaybackType); // 0x82362E20 write, line 299
        EActiveRaceCarIndex      GetActivePaybackAggressor() const;                     // 0x823B9ED8 read, line 301
        void                     SetActivePaybackAggressor(EActiveRaceCarIndex leAggressor);   // 0x82362ED0 write, line 302

        // Game-mode elapsed time (mGameModeElapsedTime @+173188)
        void                       SetGameModeElapsedTime(const OutputBufferTime* lpTime);  // 0x82362F80 write, line 305

        // Controller-active flag (mbControllerActive @+192490)
        bool GetControllerActive() const;            // 0x823B9F88 read, line 307
        void SetControllerActive(bool lbActive);     // 0x82363040 write, line 308

        // SetUpAllEventStarts interface valid flag (mbSetUpAllEventStartsInterfaceIsValid @+192488)
        bool GetSetUpAllEventStartsInterfaceIsValid() const;         // 0x823BA0E0 read, line 328
        void SetSetUpAllEventStartsInterfaceIsValid(bool lbValid);   // 0x82363198 write, line 329

        // SpecificGameModeEvent interface valid flag (mbSpecificGameModeEventInterfaceIsValid @+192489)
        bool GetSpecificGameModeEventInterfaceIsValid() const;       // 0x823BA190 read, line 334
        void SetSpecificGameModeEventInterfaceIsValid(bool lbValid); // 0x82363248 write, line 335

    private:
        // --- data members at exact X360 byte offsets (absolute from `this`) ----
        u8  maPadToGameActionQueue[0x04 - sizeof(CgsModule::IOBuffer)];   // base end -> 0x0004
        u8  mGameActionQueueStorage[0x3414 - 0x04];                       // GameActionQueue              @ +0x0004
        u8  mResourceRequestInterfaceStorage[0x4024 - 0x3414];            // ResourceRequestInterface     @ +0x3414 (RequestInterface<3072>, 3088B -> +0x4024 = 16420)
        u8  mTimerRequestInterfaceStorage[0x4034 - 0x4024];               // TimerRequestInterface        @ +16420 (16B)
        u8  mFrameRateTypeRequestInterfaceStorage[0x4040 - 0x4034];       // FrameRateTypeRequestInterface@ +16436 (12B)
        u8  mTakedownEventOutputQueueStorage[0x43AC - 0x4040];            // TakedownEventOutputQueue     @ +0x4040 (16448)
        GameStateToControllerInterface mGameStateToControllerInterface;   // @ +0x43AC (17324, named opaque)
        u8  maPadToGameStateToGui[0x4450 - (0x43AC + sizeof(GameStateToControllerInterface))]; // -> +0x4450
        u8  mGameStateToGuiInterfaceStorage[0x4840 - 0x4450];             // GameStateToGuiInterface      @ +0x4450 (17488)
        u8  mGuiEventQueueStorage[0x9050 - 0x4840];                       // GuiEventQueue                @ +18496 .. +0x9050
        TriggerManagementInputInterface mTriggerManagementInputInterface; // @ +0x9050 (36944, named opaque)
        u8  mPostGuiEventQueueStorage[173180 - (0x9050 + sizeof(TriggerManagementInputInterface))]; // -> +173180
        BrnNetwork::EPaybackType meActivePaybackType;                     // +173180
        EActiveRaceCarIndex      meActivePaybackAggressor;                // +173184
        OutputBufferTime         mGameModeElapsedTime;                    // +173188 (8B -> +173196)
        u8  mRaceCarRaceDistanceInterfaceStorage[192488 - (173188 + 8)];  // RaceCarRaceDistanceInterface @ +173196 (0x2A48C) .. +192488
        bool mbSetUpAllEventStartsInterfaceIsValid;                       // +192488
        bool mbSpecificGameModeEventInterfaceIsValid;                     // +192489
        bool mbControllerActive;                                          // +192490
    };

    // ---- free predicate over EGameModeType (X360 0x821F2B08) -----------------
    // (Declared here for completeness; body lives in BrnGameStateSharedIO.cpp where its
    // EGameModeType home -- BrnGameStateSharedIO.h -- is the owning header.)

} // namespace GameStateModuleIO
} // namespace BrnGameState
