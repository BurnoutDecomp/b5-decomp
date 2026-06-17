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

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"      // CgsModule::IOBuffer (base; lock state)
#include "GameShared/GameClasses/Module/CgsEventQueue.h"    // CgsModule::EventQueue<T,N> (mTrafficTypeResponseQueue)
#include "GameSource/BurnoutConstants.h"                    // EActiveRaceCarIndex (enum : s32)
#include "GameSource/Network/SharedIO/BrnNetworkSharedIO.h" // BrnNetwork::EPaybackType (enum : s32)
#include "GameSource/World/EntityModules/TrafficEntityModule/SharedIO/BrnTrafficTypeInterface.h" // BrnTraffic::BrnTrafficIO::TrafficTypeResponse

namespace BrnGameState
{
namespace GameStateModuleIO
{
    // ---- forward declarations of the interface member types (own TUs) --------
    // Full layouts live with their own reconstructions; only pointers are returned by the
    // accessors, so incomplete declarations suffice.
    class ControllerInput;                 // PreWorldInputBuffer +0x34
    class GameEventQueue;                  // == VariableEventQueue<1536,16>
    class TakedownEventInputQueueType;     // PreWorldInputBuffer +0x660
    class NetworkPlayerResultsInterface;   // PreWorldInputBuffer +0x36B8
    class VehicleOutputInterface;          // PostWorldInputBuffer +0x220
    class AICarOutputInterface;            // PostWorldInputBuffer +0xAAC0
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

    private:
        // --- raw storage pinned to X360 offsets (gaps = padding) -------------
        u8  maPadToControllerInput[0x34 - sizeof(CgsModule::IOBuffer)]; // base end -> 0x34
        u8  mControllerInputStorage[0x4C - 0x34];                       // ControllerInput  @ +0x0034
        u8  mGameEventQueueStorage[0x660 - 0x4C];                       // GameEventQueue   @ +0x004C
        u8  mTakedownEventInputQueueStorage[0x36B8 - 0x660];            // TakedownEventQueue@ +0x0660
        u8  mNetworkPlayerResultsInterfaceStorage[0x40];                // NetworkPlayerResultsInterface @ +0x36B8 (widen when its own TU lands)
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

    private:
        u8  maPadToVehicleOutput[0x220 - sizeof(CgsModule::IOBuffer)]; // base end -> 0x0220
        u8  mVehicleOutputInterfaceStorage[0xA4B0 - 0x220];           // VehicleOutputInterface @ +0x0220
        u8  mGameEventQueueStorage[0xAAC0 - 0xA4B0];                  // GameEventQueue         @ +0xA4B0
        u8  mAICarOutputInterfaceStorage[0xBFA8 - 0xAAC0];            // AICarOutputInterface   @ +0xAAC0 (placeholder width: spans to the next pinned member; widen/replace when AICarOutputInterface's own TU lands)
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
        // X360 0x823630F0 (write-lock; "Not locked for writing", line 311)
        RaceCarRaceDistanceInterface*     GetRaceCarRaceDistanceInterface();

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
        u8  mTakedownEventOutputQueueStorage[0x4450 - 0x4040];            // TakedownEventOutputQueue     @ +0x4040 (16448)
        u8  mGameStateToGuiInterfaceStorage[0x4840 - 0x4450];             // GameStateToGuiInterface      @ +0x4450 (17488)
        u8  mGuiEventQueueStorage[173180 - 0x4840];                       // GuiEventQueue                @ +18496 .. +173180
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
