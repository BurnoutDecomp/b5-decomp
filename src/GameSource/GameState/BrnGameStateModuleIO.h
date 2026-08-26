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
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h" // CgsModule::VariableEventQueue<N,16> (GUI output / dirty-trick HUD events)
#include "GameSource/BurnoutConstants.h"                    // EActiveRaceCarIndex (enum : s32)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::EPaybackType (enum : s32)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h" // BrnTraffic::BrnTrafficIO::TrafficTypeResponse
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h" // BrnAI::AIModuleIO::AICarOutputInterface (complete; embedded by value @ +0xAAC0)
#include "GameSource/Network/BrnNetworkModuleIO.h"                 // BrnNetwork::BrnNetworkModuleIO::PlayerResultsInterface (embedded @ +0x36B8)
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusInterface (embedded @ +0x2CC8)
#include "GameSource/World/EntityModules/TriggerEntityModule/SharedIO/BrnTriggerEntityModuleInputInterface.h" // BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface (OutputBuffer +0x9050, embedded by value)

// PostWorldInputBuffer hands out the active-race-car output interface by pointer only
// (GetActiveRaceCarOutputInterface, X360 0x8231D2C0); forward-declare its real home.
namespace BrnWorld { namespace RaceCarEntityModuleIO { struct RCEntityActiveRaceCarOutputInterface; } }

// TriggerQueryManager::UpdateTriggers arms/removes trigger regions through
// GetTriggerManagementInputInterface(); the grown interface methods below take these by
// incomplete pointer/reference, so forward declarations suffice.
namespace BrnTrigger { struct TriggerRegion; }
namespace BrnWorld { namespace TriggerEntityModuleIO { struct InRemoveTriggerEvent; } }

// The OutputBuffer's +0x3414 member is the GameData resource-request interface. Only the
// TYPE NAME is needed here (every accessor hands out a pointer); the request-builder bodies
// live in BrnGameDataRequestQueue.h, which callers include for themselves. Forward-declaring
// the template keeps this widely-included header off the GameData include chain.
namespace BrnResource { namespace GameDataIO { template <s32 N> struct RequestInterface; } }

// OutputBuffer::GetGameStateToNetworkInterface() (BrnPaybackManager grow) returns this by pointer
// only; forward-declare its real home (BrnNetworkModuleGameStateIOInterfaces.h).
namespace BrnNetwork { namespace BrnNetworkModuleIO { struct GameStateToNetworkInterface; } }

namespace BrnGameState
{
namespace GameStateModuleIO
{
    // ---- forward declarations of the interface member types (own TUs) --------
    // Full layouts live with their own reconstructions; only pointers are returned by the
    // accessors, so incomplete declarations suffice.
    // ⭐ [gateui] SEAT PROMOTED 2026-08-20 -- this was `class GameEventQueue;`, forward-only, over
    // two 1552/1556-byte `u8` placeholders. It is now the DWARF definition verbatim:
    //     dwarfdump GameSource/GameState/BrnGameEvents.h:314
    //     `struct GameEventQueue : public VariableEventQueue<1536,16> {}`
    // WHY NOW: owner `bridge` lands BridgeWorldToGameState this round, which Appends the world's
    // per-frame queue into PostWorldInputBuffer's copy -- an `Append<1536,16>` call that CANNOT be
    // written against an incomplete type. Every existing caller reached it through a
    // `reinterpret_cast<GameEventQueue*>(&storage)` in the accessor; those casts still compile
    // unchanged, they now just hand back a usable type.
    //
    // ⚠️ THE PADS ARE A LOADED GUN -- the two seats were sized from CONSOLE offset arithmetic
    // (0x660-0x4C == 1556 and 0xAAC0-0xA4B0 == 1552), so promoting the seat means re-deriving the
    // remainder AT HOST SIZE, not assuming the console's fit. Measured, not assumed:
    // VariableEventQueue<1536,16> is POINTER-FREE (bool mbIsConstructed; char macData[1536];
    // three s32), so its host sizeof is 1552 -- the SAME as the console's -- and the
    // KI_..._SEAT_SIZE static_asserts below fail loudly the day that stops being true. The
    // PostWorld seat is therefore an exact fit; the PreWorld seat keeps 4 bytes of trailing
    // console padding, spelled as an explicit pad rather than folded into the member.
    class GameEventQueue : public CgsModule::VariableEventQueue<1536, 16>
    {
    };

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
    // PostWorldInputBuffer +0x7250: the active-race-car output interface
    // (BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface). Returned read-locked
    // by GetActiveRaceCarOutputInterface (X360 0x8231D2C0, "Not locked for reading" line 210); only
    // a pointer is handed out, so a forward declaration suffices.
    // DWARF (BrnGameStateModuleIO.h:73,181,239): the PostWorldInputBuffer AICarOutputInterface
    // is a typedef IMPORT of the AI module's BrnAI::AIModuleIO::AICarOutputInterface (mirrors
    // the ActiveRaceCarOutputInterface == RCEntityActiveRaceCarOutputInterface pattern). It is
    // embedded by value @ +0xAAC0, so the real (complete) type is included above and aliased here.
    typedef BrnAI::AIModuleIO::AICarOutputInterface AICarOutputInterface; // PostWorldInputBuffer +0xAAC0
    // GameActionQueue (OutputBuffer +0x04) is a REAL typedef now -- see
    // BrnGameStateSharedIO.h (CgsModule::VariableEventQueue<13312,16>).
    // ⭐ RE-HOMED 2026-08-01 (was `class ResourceRequestInterface;` -- an INCOMPLETE
    // forward-declared class that no TU could ever define). The DWARF and the X360 agree the
    // OutputBuffer member at +0x3414 IS a BrnResource::GameDataIO::RequestInterface<3072>:
    //   * TriggerQueryManager::Prepare @0x82398218 calls
    //     `RequestInterface<3072>::LoadBundle(outBuf->GetResourceRequestInterface(), ...)`;
    //   * GameStateModule::Prepare @0x8239E578 calls GetVehicleList/GetWheelList/
    //     GetFreeburnChallengeList on the same pointer;
    //   * GamePrepare @0x823EFBD0 / LoadGameState2 @0x823EF4D8 hand it to
    //     `GameDataIO::InputBuffer::AppendRequestInterface<3072>`;
    //   * StreetManager::LoadAIData already reinterpret_casts it to exactly this type
    //     (BrnGameStateStreetManager_wB_01.cpp:63).
    // The storage blob below is 0x4024-0x3414 == 3088 bytes == sizeof(RequestInterface<3072>)
    // on the host too (the type is pointer-free: a VariableEventQueue<3072,16> is a 3072-byte
    // buffer plus a 16-byte {bool, 3*s32} header), so this is a pure type re-home -- no
    // layout change. Asserted in the .cpp.
    typedef BrnResource::GameDataIO::RequestInterface<3072> ResourceRequestInterface; // OutputBuffer +0x3414
    class TakedownEventOutputQueueType;    // OutputBuffer +0x4040
    struct GameStateToGuiInterface;        // OutputBuffer +0x4450 (complete def: BrnGameStateToGuiIOInterfaces.h)
    class RaceCarRaceDistanceInterface;    // OutputBuffer +0x2A48C (complete def: BrnGameStateSharedIO.h)
    // The two scoring snapshots BridgeGameStateToWorld (X360 0x823E1890) forwards. Both are
    // COMMITTED types with real member sets (BrnGameStateSharedIO.h); they are returned by
    // pointer only, so incomplete declarations keep this widely-included header off that chain.
    struct ScoringOutputInterface;         // OutputBuffer +173240 (console span 2736)
    struct OnlineScoringOutputInterface;   // OutputBuffer +175976 (console span 164)
    struct GameModeOutputInterface;        // OutputBuffer +176344 (console span 16; phase C3b)

    // OutputBuffer +169068. The console's OutputBuffer::Construct runs
    // `VariableEventQueue<4096,16>::Construct(this + 169068)` on it and BridgeGameStateToWorld
    // hands it to UpdateInputBuffer::AppendTriggerQueryInputInterface, whose own member is the
    // same instantiation -- the canonical name the PS3 DWARF spells for the merge is
    // BrnWorld::TriggerEntityModuleIO::TriggerQueryInputInterface::Append, but that aggregate
    // has no committed home yet, so (exactly as BrnWorldModuleIO.h does for its own copy) the
    // instantiation is reused by name here.
    typedef CgsModule::VariableEventQueue<4096, 16> TriggerQueryInputInterface;

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
    struct NetworkToGameStateInterface
    {
        // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. The X360 PaybackManager::
        // ProcessDirtyTrickEventQueue reaches the per-frame inbound dirty-trick queue as
        // lpInput->GetNetworkToGameStateInterface()->GetDirtyTrickQueue() (the queue lives at
        // interface +0x2268: a CgsModule::EventQueue<DirtyTrickEvent,28> walked by GetLength()/
        // GetEvent(i)). Body + the real embedded-queue member land with this interface's own TU.
        const CgsModule::EventQueue<BrnNetwork::BrnNetworkModuleIO::DirtyTrickEvent, 28>*
            GetDirtyTrickQueue() const;

        u8 maOpaque[16];
    };

    // OutputBuffer +0x9050. DWARF (:283/284, :347): the trigger-management input interface.
    // Returned read-locked (BridgeGameStateToWorld) and write-locked (ModeManager::StartGameMode).
    //
    // ⛔ RETYPED 2026-08-01 (BridgeGameStateToWorld wave). This was a LOCAL 16-byte opaque
    // placeholder struct with two declared-only methods. It is the SAME aggregate the world's
    // UpdateInputBuffer already carries by its canonical name -- and the console proves it here
    // too: OutputBuffer::Construct (X360 0x82382940) runs
    //     VariableEventQueue<131072,16>::Construct(this + 36944)   // == +0x9050, the add queue
    //     InRemoveTriggerEvent<..,256>::Construct(this + 168032)   // == 36944 + 131088
    // i.e. exactly the committed aggregate's two embedded queues at its own two offsets, and
    // 36944 + 132124 == 169068 == the trigger-query interface accessor's return (X360 0x823B9B88),
    // so the aggregate occupies the whole span. The old 16-byte model made the buffer's
    // Append/Construct legs impossible to write and would have had BridgeGameStateToWorld hand
    // AppendTriggerManagementInputInterface a 16-byte object to read 132128 bytes out of.
    // (Same retype BrnWorldModuleIO.h made for its own copy of this member, and for the same
    // reason.) The two placeholder methods are gone: AddTriggerRegion is the real type's own
    // (X360 0x8238ECF8), and RemoveTrigger is grown onto it there.
    typedef BrnWorld::TriggerEntityModuleIO::TriggerManagementInputInterface TriggerManagementInputInterface;

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
        // [gateui] real typed seat (was `u8 mGameEventQueueStorage[0x660 - 0x4C]`).
        GameEventQueue       mGameEventQueue;                               // @ +0x004C
        // The console seat is 4 bytes wider than the queue; keep the remainder explicit so
        // mTakedownEventInputQueueStorage stays pinned at +0x660 (asserted below).
        u8  maPadAfterGameEventQueue[(0x660 - 0x4C) - sizeof(GameEventQueue)];
        u8  mTakedownEventInputQueueStorage[0x7B0 - 0x660];            // TakedownEventQueue@ +0x0660
        NetworkToGameStateInterface mNetworkToGameStateInterface;      // @ +0x07B0 (named opaque)
        u8  maPadToPlayerStatus[0x2CC8 - (0x7B0 + sizeof(NetworkToGameStateInterface))]; // -> +0x2CC8
        BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusInterface mPlayerStatusInterface; // @ +0x2CC8
        // mPlayerStatusInterface (0x9F0 bytes) ends flush at +0x36B8, so the gap to the next
        // member is ZERO -- no padding array here (a u8[0] is ill-formed under MSVC /permissive-).
        // The +0x36B8 offset of mNetworkPlayerResultsInterface is independently pinned by the
        // static_assert in _AssertLayout(), which guards this flush fit.
        NetworkPlayerResultsInterface mNetworkPlayerResultsInterface;  // @ +0x36B8 (PlayerResultsInterface, 224B)

        // Compile-time offset guards (private members -> assert from a member-fn context).
        static void _AssertLayout()
        {
            static_assert(sizeof(ControllerInput) == 0x18, "ControllerInput must be 0x18 bytes");
            static_assert(sizeof(TimerStatusInterface) == 0x30, "TimerStatusInterface must be 0x30 bytes");
            static_assert(offsetof(PreWorldInputBuffer, mTimerStatusInterface)          == 0x04,   "mTimerStatusInterface @ +0x04");
            static_assert(offsetof(PreWorldInputBuffer, mControllerInput)               == 0x34,   "mControllerInput @ +0x34");
            // [gateui] the promoted seat: pin BOTH its offset and its HOST size, because the pad
            // that follows it is computed from `sizeof(GameEventQueue)` -- if the queue ever grows
            // a pointer, this is the assert that fires instead of silently shifting +0x660.
            static_assert(sizeof(GameEventQueue) == 1552, "GameEventQueue host sizeof (pointer-free: bool + char[1536] + 3 * s32)");
            static_assert(offsetof(PreWorldInputBuffer, mGameEventQueue)                == 0x4C,   "GameEventQueue @ +0x4C");
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
        // X360 0x8231D2C0 (read-lock; "Not locked for reading", line 210) -- the active-race-car
        // output interface (this+0x7250). BurnoutSkillzManager::PostWorldUpdate reads it read-locked.
        const BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface*
                                      GetActiveRaceCarOutputInterface() const;
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
        u8  mPostVehicleOutputStorage[0x7250 - (0x6E34 + sizeof(TriggerEntityModuleOutputInterface))]; // -> +0x7250
        // The active-race-car output interface (BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface)
        // @ +0x7250. Named opaque storage (its full layout lives in its own TU); GetActiveRaceCarOutputInterface
        // returns &this member. Its GetPlayerActiveRaceCarIndex reads at interface+0x2858.
        u8  mActiveRaceCarOutputInterfaceStorage[0x2890]; // RCEntityActiveRaceCarOutputInterface @ +0x7250 (covers +0x2858 read)
        u8  mPostActiveCarOutputStorage[0xA4B0 - (0x7250 + 0x2890)]; // -> +0xA4B0
        // [gateui] real typed seat (was `u8 mGameEventQueueStorage[0xAAC0 - 0xA4B0]`). This seat
        // is an EXACT fit at host size (0xAAC0 - 0xA4B0 == 1552 == sizeof(GameEventQueue)), so no
        // trailing pad -- and the two asserts below are what keep that claim honest.
        GameEventQueue       mGameEventQueue;                         // GameEventQueue         @ +0xA4B0
        // Real, complete AICarOutputInterface (BrnAI::AIModuleIO::AICarOutputInterface). sizeof == 0x14E8
        // (== 0xBFA8 - 0xAAC0), 4-aligned, so it occupies exactly the former placeholder span and leaves
        // mTrafficTypeResponseQueue pinned at +0xBFA8.
        AICarOutputInterface mAICarOutputInterface;                   // AICarOutputInterface   @ +0xAAC0
        // TrafficTypeResponse query-response queue @ +0xBFA8 (49064). Fixed-capacity EventQueue<...,32>;
        // 16B element stride == sizeof(TrafficTypeResponse). AppendTrafficTypeResponseQueue forwards to it.
        CgsModule::EventQueue<BrnTraffic::BrnTrafficIO::TrafficTypeResponse, 32> mTrafficTypeResponseQueue; // +0xBFA8

        // Compile-time offset guards (private members -> assert from a member-fn context).
        static void _AssertLayout()
        {
            static_assert(offsetof(PostWorldInputBuffer, mActiveRaceCarOutputInterfaceStorage) == 0x7250,
                          "RCEntityActiveRaceCarOutputInterface @ +0x7250");
            static_assert(offsetof(PostWorldInputBuffer, mGameEventQueue) == 0xA4B0,
                          "GameEventQueue @ +0xA4B0");
            // [gateui] the exact-fit claim, spelled out: the seat the console's offsets carve is
            // 0xAAC0 - 0xA4B0 == 1552, and the host queue is 1552. If either moves, this fires
            // before mAICarOutputInterface silently slides off +0xAAC0.
            static_assert((0xAAC0 - 0xA4B0) == sizeof(GameEventQueue),
                          "PostWorldInputBuffer GameEventQueue seat is an exact host fit");
            static_assert(offsetof(PostWorldInputBuffer, mAICarOutputInterface) == 0xAAC0,
                          "AICarOutputInterface @ +0xAAC0");
        }
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
        // X360 0x82382940. Raises the base status byte and constructs every embedded queue /
        // interface. Body (with the console's full construct list, verbatim) in the .cpp.
        void Construct();

        // ---- GameStateModuleIO TU accessors ----
        // X360 0x8231D4B8 (write-lock; "Not locked for writing", line 266)
        GameActionQueue*                  GetGameActionQueue();
        // X360 0x823B96F0 (read-lock;  "Not locked for reading", BrnGameStateModuleIO.h line 265).
        // RECOVERED IDENTITY: the exports carry this as an unnamed `sub_823B96F0`; it is the
        // const half of GetGameActionQueue (returns this+4, asserts the read lock, and its
        // assert's __FILE__/__LINE__ is BrnGameStateModuleIO.h:265, one line above the
        // write-side's 266). BrnGameModule::BridgeGameStateToDirector @0x823CD170 is its
        // caller -- it is the SOURCE of every game action the director ever sees.
        const GameActionQueue*            GetGameActionQueue() const;
        // X360 0x823B9840 (read-lock; BrnGameStateModuleIO.h line 271) -- likewise an unnamed
        // `sub_823B9840` in the exports; returns this+0x4040, the takedown-event output queue.
        const TakedownEventOutputQueueType* GetTakedownEventOutputQueue() const;
        // X360 0x823B9798 (read-lock;  "Not locked for reading", line 268)
        const ResourceRequestInterface*   GetResourceRequestInterface() const;
        // X360 0x82362B80 (write-lock; "Not locked for writing", line 272)
        TakedownEventOutputQueueType*     GetTakedownEventOutputQueue();
        // X360 0x8231D8A8 (write-lock; "Not locked for writing", line 296)
        GameStateToGuiInterface*          GetGameStateToGuiInterface();
        // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. The X360 PaybackManager bodies
        // (HandleHavingPayback / ProcessDirtyTrickEventQueue) post the per-car dirty-trick game-action
        // records onto the output buffer's variable-size game-action queue
        // (CgsModule::VariableEventQueue<13312,16> == the GameStateModuleIO GameActionQueue) via
        // AddEvent(&ev, type, size) / AddGameAction<T>. Concrete-typed twin of GetGameActionQueue()
        // (whose forward-declared incomplete GameActionQueue cannot be called through); returned
        // write-locked, body lands with the OutputBuffer TU.
        CgsModule::VariableEventQueue<13312, 16>* GetGuiOutputQueue();

        // ADDITIVE GROW (declare-only) for the BrnPaybackManager TU. X360 PaybackManager::Update
        // appends the manager's per-frame outbound dirty-trick queue into the output buffer's
        // GameState->Network interface (GetGameStateToNetworkInterface()->GetDirtyTrickQueue()->
        // Append(...)). Returns the embedded interface by pointer; body lands with the OutputBuffer TU.
        BrnNetwork::BrnNetworkModuleIO::GameStateToNetworkInterface* GetGameStateToNetworkInterface();
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

        // ---- BridgeGameStateToWorld (X360 0x823E1890) read side ---------------------------
        // The four remaining sources that bridge reads off this buffer. The first two are real
        // X360 symbols the exports carry UNNAMED (recovered by their asserts' own
        // __FILE__/__LINE__ + the member offset each returns, exactly as GetGameActionQueue()
        // const / GetTakedownEventOutputQueue() const were); the last two are INLINED on the
        // X360 (the bridge computes `outputBuffer + 173240` / `+ 175976` in-line and passes the
        // address straight to the world setters -- see the two `addis r4,r30,3` pairs at
        // 0x823E1938 / 0x823E1948), so they carry no own address.
        //
        // X360 0x823B9B88 (read-lock; BrnGameStateModuleIO.h line 279) -- returns this+169068.
        const TriggerQueryInputInterface*      GetTriggerQueryInputInterface() const;
        // X360 0x823BA038 (read-lock; BrnGameStateModuleIO.h line 310) -- returns this+173196.
        const RaceCarRaceDistanceInterface*    GetRaceCarRaceDistanceInterface() const;
        // INLINED on X360 (bridge computes this+173240).
        const ScoringOutputInterface*          GetScoringOutputInterface() const;
        // INLINED on X360 (bridge computes this+175976).
        const OnlineScoringOutputInterface*    GetOnlineScoringOutputInterface() const;
        // INLINED on X360 (BridgeGameStateToSound @0x823CDE50 computes this+176344; phase C3b).
        const GameModeOutputInterface*         GetGameModeOutputInterface() const;

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
        // ⚠️ EXPLICITLY GLOBAL-QUALIFIED (2026-08-01). Two distinct `enum EActiveRaceCarIndex : s32`
        // exist in this tree -- the global one in BurnoutConstants.h (which THIS header includes
        // for exactly this member; it is also the one BrnWorldIO::UpdateInputBuffer::
        // SetActivePaybackAggressor takes) and BrnGameState::EActiveRaceCarIndex in
        // BrnTakedownManagerTypes.h. These three spellings sit inside `namespace BrnGameState`, so
        // unqualified they bound to WHICHEVER of the two the including TU happened to have pulled
        // in: the same accessor had two different return types, hence two different mangled names,
        // depending on include order. Caught by BridgeGameStateToWorld, whose destination takes the
        // global one ("cannot convert BrnGameState::EActiveRaceCarIndex to EActiveRaceCarIndex").
        // Unifying the two enums is its own cleanup; pinning the scope here is not.
        ::EActiveRaceCarIndex    GetActivePaybackAggressor() const;                     // 0x823B9ED8 read, line 301
        void                     SetActivePaybackAggressor(::EActiveRaceCarIndex leAggressor);   // 0x82362ED0 write, line 302

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
        // ⛔ CORRECTED 2026-08-01 (BridgeGameStateToWorld wave). These two members used to be
        //     TriggerManagementInputInterface mTriggerManagementInputInterface;   // 16-byte placeholder
        //     u8 mPostGuiEventQueueStorage[173180 - (0x9050 + sizeof(...))];      // 136220-byte blob
        // -- a 16-byte stand-in followed by an unnamed blob labelled "post-GUI event queue".
        // NEITHER label was right. The console's own OutputBuffer::Construct list (transcribed at
        // the body in the .cpp) reads
        //     VariableEventQueue<131072,16>::Construct(this + 36944)   // == +0x9050
        //     InRemoveTriggerEvent<..,256>::Construct(this + 168032)   // == 36944 + 131088
        //     VariableEventQueue<4096,16>::Construct (this + 169068)
        // The first two ARE the committed TriggerManagementInputInterface aggregate at its own two
        // internal offsets, so it spans 36944..169068 (132124); the third is a trigger-QUERY input
        // interface (VariableEventQueue<4096,16>, 4112 bytes) filling 169068..173180 exactly, and
        // 169068 is precisely what the read-side accessor X360 0x823B9B88 returns. So the whole
        // 136236-byte region is two named interfaces, and the buffer's Construct could not build
        // either of them while they were an opaque blob. There is NO post-GUI event queue here.
        //
        // Both are real typed members now; the host sizes them (132128 + 4112 == 136240, four
        // bytes over the console's 136236 because the embedded remove queue pads differently on
        // x64). Everything after them therefore sits four bytes past its console offset -- which
        // is fine and is why the trailing comments say "console +N", not "+N": every access to
        // this buffer is BY NAMED MEMBER. Nothing in the tree pokes it at an absolute offset
        // (verified by grep over 173180/173196/173240/175976/192488).
        TriggerManagementInputInterface mTriggerManagementInputInterface; // console +0x9050 (36944)
        TriggerQueryInputInterface      mTriggerQueryInputInterface;      // console +169068 (4112)
        BrnNetwork::EPaybackType meActivePaybackType;                     // console +173180
        ::EActiveRaceCarIndex    meActivePaybackAggressor;                // console +173184 (see the accessor's ⚠️)
        OutputBufferTime         mGameModeElapsedTime;                    // console +173188 (8B)
        // The tail, carved at the console's own anchors (all three spans are attested by
        // OutputBuffer::Construct's zero-fills / memsets and by the two inlined address
        // computations in BridgeGameStateToWorld). Each stays OPAQUE storage of the CONSOLE's
        // width, with a .cpp static_assert that the committed type fits -- see the ⚠️ note on
        // GetScoringOutputInterface's body for why the console width, not the host sizeof, is
        // the one that matters here.
        u8  mRaceCarRaceDistanceInterfaceStorage[173240 - 173196];        // console +173196 (44)
        u8  mScoringOutputInterfaceStorage[175976 - 173240];              // console +173240 (2736)
        u8  mOnlineScoringOutputInterfaceStorage[176140 - 175976];        // console +175976 (164)
        u8  maPadToGameMode[176344 - 176140];                             // console +176140 .. +176344
        u8  mGameModeOutputInterfaceStorage[176360 - 176344];             // console +176344 (16; phase C3b)
        u8  maUnmodelledTailStorage[192488 - 176360];                     // console +176360 .. +192488
        bool mbSetUpAllEventStartsInterfaceIsValid;                       // console +192488
        bool mbSpecificGameModeEventInterfaceIsValid;                     // console +192489
        bool mbControllerActive;                                          // console +192490
    };

    // ---- free predicate over EGameModeType (X360 0x821F2B08) -----------------
    // (Declared here for completeness; body lives in BrnGameStateSharedIO.cpp where its
    // EGameModeType home -- BrnGameStateSharedIO.h -- is the owning header.)

} // namespace GameStateModuleIO
} // namespace BrnGameState
