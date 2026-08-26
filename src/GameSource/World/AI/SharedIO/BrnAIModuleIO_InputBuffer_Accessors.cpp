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
    // ---------------------------------------------------------------------------------------------
    // InputBuffer::Construct @0x8278AB80   -- A MINIMAL-COMPLETE SLICE, and it closes a latent AV.
    //
    // The console body, in its own order (offsets are this-relative; see the declaration's banner
    // in BrnAIModuleIO.h for why most of it is not reproduced):
    //   *this = 1                                              -- IOBuffer::Construct's status byte
    //   *(this + 17376) = 0
    //   BrnTrafficIO::RivalInTrafficUpdateEvent_34_::Construct(this + 62448)
    //   *(this + 64232) = 0 ; *(this + 64372) = 0
    //   CgsSystem::TimerStatusInterface::Clear(this + 64384)
    //   nine 8-byte zero stores at this + 704 .. + 768
    //   CgsModule::VariableEventQueue<16384,16>::Construct(this + 776)
    //   *(this + 17220) = 0
    //   ⭐ BrnAI::AIModuleIO::ResetOnTrackRequest_128_::Construct(this + 64432)   == +0xFBB0
    //   RaceCarRaceDistanceInterface::Clear(this + 79968)
    //   VariableEventQueue<13312,16>::Construct(this + 66492)
    //   VariableEventQueue<32768,16>::Construct(this + 80008)
    //   BrnGameState::TakedownEvent_8_::Construct(this + 112792)
    //   twenty-two zero stores at this + 113128 .. + 113186
    //   RouteMapModuleIO::RaceRouteRequest_1_::Construct(this + 79824)
    //
    // ⭐ 64432 == 0xFBB0 == KU_AI_MODULE_REQUEST_INTERFACE_OFFSET, and the interface's ONLY member
    // is that queue (DWARF BrnAIModuleRequestInterface.h:109) -- which is why the console's
    // Construct call takes the interface's own base address. Constructed here BY NAME through the
    // same typed reinterpret the Append accessor below already uses, so the two can never disagree
    // about where the queue is.
    //
    // ⛔ [FLAG PC bring-up] EVERY OTHER LEG IS ABSENT. Each one lands inside this type's
    // attested-offset image blob, which has NO named members (see the header's MINIMAL SLICE
    // note), so constructing them would mean writing through raw offsets into an opaque payload --
    // and getting one wrong is a silent 100 KB scribble, not a compile error. They land WITH the
    // named layout. Nothing on the reset-on-track path reads any of them.
    // ⚠️ Consequence, stated plainly: the OTHER queues in this buffer are still unconstructed, so
    // the same latent AV remains for THEM. This function fixes the one member with a live
    // producer, and the header banner says so. DELETE-WHEN this type gets a named layout.
    // ---------------------------------------------------------------------------------------------
    void InputBuffer::Construct()
    {
        CgsModule::IOBuffer::Construct();

        typedef AIModuleRequestInterface::ResetOnTrackRequestQueue ResetOnTrackRequestQueue;
        reinterpret_cast<ResetOnTrackRequestQueue*>(
            MemberImage() + KU_AI_MODULE_REQUEST_INTERFACE_OFFSET)->Construct();

        // [FLAG PC bring-up] the fourteen other legs of the console's Construct -- see the banner.
    }

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

    // X360 0x8276D488 (R) -- the race-route request queue (this+0x137D0). Read-lock
    // getter twin of AppendRaceRouteRequestQueue (write-locked, same offset). Callers
    // BrnAI::AIModule::PausedUpdate / Update.
    const RouteMapModuleIO::RaceRouteRequestQueue* InputBuffer::GetRaceRouteRequestQueue() const
    {
        CGS_ASSERT(IsBufferLockedForReading(), "Not locked for reading\n");
        return reinterpret_cast<const RouteMapModuleIO::RaceRouteRequestQueue*>(
            MemberImage() + KU_RACE_ROUTE_REQUEST_QUEUE_OFFSET);
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
