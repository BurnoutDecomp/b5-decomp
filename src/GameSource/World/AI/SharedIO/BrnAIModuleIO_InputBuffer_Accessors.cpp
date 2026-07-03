#include "GameSource/World/AI/SharedIO/BrnAIModuleIO.h"
#include "GameSource/World/AI/SharedIO/BrnRaceCarAIInterfaces.h"       // RaceCarAIInterface (Get/SetRaceCarAIInterface)
#include "GameSource/World/AI/SharedIO/BrnAIModuleRequestInterface.h"  // AIModuleRequestInterface::ResetOnTrackRequestQueue
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"             // RouteMapModuleIO::RaceRouteRequestQueue
#include "GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h" // CgsSystem::TimerStatusInterface
#include "GameSource/GameState/TakedownManager/BrnTakedownManagerTypes.h" // BrnGameState::TakedownEvent
#include "GameShared/GameClasses/Module/CgsEventQueue.h"               // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT

#include <cstring>   // std::memcpy (the memcpy setters model the Xbox block copy)

// Out-of-line bodies of BrnAI::AIModuleIO::InputBuffer's lock-guarded Get/Set/Append
// accessors over the buffer payload. Each returns / copies at `MemberImage() +
// <attested byte offset>` (EMemberOffset enum in BrnAIModuleIO.h), guarded by the
// lock bit the asm names -- read (bit 4) for getters, write (bit 3) for Get(), the
// setters and the appends. The lock-assert strings carry the trailing "\n" of the
// X360 rodata; the non-null asserts do not. Reproduced verbatim, not "fixed".

namespace BrnAI
{
namespace AIModuleIO
{
    // ---- getters (read-lock bit 4) ----------------------------------------------

    // X360 0x8276D728 (R) -- the pre-scene race-car AI view (this+0x10).
    const RaceCarAIInterface* InputBuffer::GetRaceCarAIInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const RaceCarAIInterface*>(
            MemberImage() + KU_RACE_CAR_AI_INTERFACE_OFFSET);
    }

    // X360 0x8276D7D0 (R) -- the traffic-AI interface handle (this+0x43E0).
    const void* InputBuffer::GetTrafficAI() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return MemberImage() + KU_TRAFFIC_AI_INTERFACE_OFFSET;
    }

    // X360 0x8276D920 (R) -- the timer-status interface (this+0xFB80).
    const CgsSystem::TimerStatusInterface* InputBuffer::GetTimerInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const CgsSystem::TimerStatusInterface*>(
            MemberImage() + KU_TIMER_INTERFACE_OFFSET);
    }

    // X360 0x8276D878 (R) -- the AI-module request interface (this+0xFBB0).
    const AIModuleRequestInterface* InputBuffer::GetAIModuleRequestInterface() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const AIModuleRequestInterface*>(
            MemberImage() + KU_AI_MODULE_REQUEST_INTERFACE_OFFSET);
    }

    // X360 0x8276D530 (R) -- the game-action queue (this+0x103BC). Read-lock twin of Get().
    const void* InputBuffer::GetGameActionQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return MemberImage() + KU_GAME_ACTION_QUEUE_OFFSET;
    }

    // X360 0x8276D5D8 (R) -- the player vehicle controls block (this+0x1B9E8).
    const void* InputBuffer::GetPlayerVehicleControls() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return MemberImage() + KU_PLAYER_VEHICLE_CONTROLS_OFFSET;
    }

    // ---- write-locked game-action queue accessor (bit 3, faithfully the WRITE bit) ----

    // X360 0x8279C4F8 (W) -- the game-action queue (this+0x103BC). Asserts the WRITE lock.
    void* InputBuffer::Get()
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        return MemberImage() + KU_GAME_ACTION_QUEUE_OFFSET;
    }

    // ---- setters / appends (write-lock bit 3) -----------------------------------

    // X360 0x8279C700 (W) -- copies a RaceCarAIInterface into this+0x10.
    void InputBuffer::SetRaceCarAIInterface(const RaceCarAIInterface* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpInterface != nullptr, "lpInterface != NULL");
        std::memcpy(MemberImage() + KU_RACE_CAR_AI_INTERFACE_OFFSET, lpInterface,
                    KU_RACE_CAR_AI_INTERFACE_SIZE);
    }

    // X360 0x8279C7E0 (W) -- copies the traffic-AI interface into this+0x43E0.
    void InputBuffer::SetTrafficAIInterface(const void* lpInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpInterface != nullptr, "lpInterface != NULL");
        std::memcpy(MemberImage() + KU_TRAFFIC_AI_INTERFACE_OFFSET, lpInterface,
                    KU_TRAFFIC_AI_INTERFACE_SIZE);
    }

    // X360 0x8279C8C0 (W) -- copies a TimerStatusInterface into this+0xFB80.
    void InputBuffer::SetTimerInterface(const void* lpTimerInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpTimerInterface != nullptr, "lpTimerInterface != NULL");
        std::memcpy(MemberImage() + KU_TIMER_INTERFACE_OFFSET, lpTimerInterface,
                    KU_TIMER_INTERFACE_SIZE);
    }

    // X360 0x8279C428 (W) -- copies the race-car race-distance interface into this+0x13860.
    void InputBuffer::SetRaceCarRaceDistanceInterface(const void* lpObject)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(MemberImage() + KU_RACE_CAR_RACE_DISTANCE_INTERFACE_OFFSET, lpObject,
                    KU_RACE_CAR_RACE_DISTANCE_INTERFACE_SIZE);
    }

    // X360 0x8279C5A0 (W) -- copies the player vehicle controls into this+0x1B9E8.
    void InputBuffer::SetPlayerVehicleControls(const void* lpControls)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        std::memcpy(MemberImage() + KU_PLAYER_VEHICLE_CONTROLS_OFFSET, lpControls,
                    KU_PLAYER_VEHICLE_CONTROLS_SIZE);
    }

    // X360 0x827AC960 (W) -- Clear()+Append the reset-on-track request queue at this+0xFBB0.
    void InputBuffer::AppendAIModuleRequestInterface(const void* lpRequestInterface)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");
        CGS_ASSERT(lpRequestInterface != nullptr, "lpRequestInterface != NULL");

        typedef AIModuleRequestInterface::ResetOnTrackRequestQueue Queue;
        Queue& lDest = *reinterpret_cast<Queue*>(
            MemberImage() + KU_AI_MODULE_REQUEST_INTERFACE_OFFSET);
        const Queue& lSource = *reinterpret_cast<const Queue*>(lpRequestInterface);

        lDest.Clear();
        lDest.Append(lSource);
    }

    // X360 0x827A9560 (W) -- Append the race-route request queue at this+0x137D0.
    void InputBuffer::AppendRaceRouteRequestQueue(const void* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        typedef RouteMapModuleIO::RaceRouteRequestQueue Queue;
        Queue& lDest = *reinterpret_cast<Queue*>(
            MemberImage() + KU_RACE_ROUTE_REQUEST_QUEUE_OFFSET);
        const Queue& lSource = *static_cast<const Queue*>(lpQueue);

        lDest.Append(lSource);
    }

    // X360 0x827A9618 (W) -- Clear()+Append the takedown event queue at this+0x1B898.
    void InputBuffer::SetTakedownEventQueue(const void* lpQueue)
    {
        CGS_ASSERT(IsBufferLockedForWriting(), "Not locked for writing\n");

        typedef CgsModule::EventQueue<BrnGameState::TakedownEvent, 8> TakedownEventQueue;
        TakedownEventQueue& lDest = *reinterpret_cast<TakedownEventQueue*>(
            MemberImage() + KU_TAKEDOWN_EVENT_QUEUE_OFFSET);
        const TakedownEventQueue& lSource = *static_cast<const TakedownEventQueue*>(lpQueue);

        lDest.Clear();
        lDest.Append(lSource);
    }
}
}
