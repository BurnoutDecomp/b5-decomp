#pragma once

// ============================================================================
// b5-decomp/src/GameSource/World/AI/SharedIO/BrnAIModuleIO.h
// ============================================================================
// BrnAI::AIModuleIO -- the AI module's per-frame IO buffer slice the world bridges
// drive. InputBuffer is a large CgsModule::IOBuffer payload (>110KB) the world
// modules fill (write-locked) and the AI module reads (read-locked).
//
// MINIMAL SLICE (matches the sibling BrnAIModuleIO_OutputBuffer.h). The interior
// member sizes and inter-member gaps are NOT independently attested, so no named
// sub-members are declared; instead the payload is a single sized image blob and
// every accessor returns `MemberImage() + <attested byte offset>` -- exactly the
// X360 `addi this, N` return spine (each Get*/Set*/Append* is `unsigned __int8*`
// arithmetic in the asm). The attested member start offsets are the EMemberOffset
// enum below. Replace this slice with a fully named layout (without moving the
// pinned offsets) when the owning-buffer DWARF lands.
//
// Lock-bit guard per the recurring IOBuffer prologue (the trailing "\n" matches the
// X360 rodata aNotLockedForRe/aNotLockedForWr; the non-null asserts have NO "\n"):
//   read-lock  (status>>4 & 1) => IsBufferLockedForReading()  ("Not locked for reading\n")
//   write-lock (status>>3 & 1) => IsBufferLockedForWriting()  ("Not locked for writing\n")
// getters read-lock (bit4) EXCEPT Get()/GetGameActionQueue() which asserts WRITE
// (bit3), reproduced verbatim; setters/appends write-lock (bit3). The X360-baked
// file path + line-number assert args are dropped per project policy.
//
// Attested member byte offsets (base = this == MemberImage()):
//   mRaceCarAIInterface           @ +0x00010 (Set copies 0x43D0 = 17360 bytes)
//   mTrafficAIInterface           @ +0x043E0 (Set copies 0xB7A0 = 47008 bytes)
//   mTimerInterface               @ +0x0FB80 (Set copies 48 = two 24-byte TimerStatus runs)
//   mAIModuleRequestInterface     @ +0x0FBB0
//   mGameActionQueue              @ +0x103BC
//   mRaceRouteRequestQueue        @ +0x137D0
//   mRaceCarRaceDistanceInterface @ +0x13860 (Set copies 40 = 10 words)
//   mSceneResultQueue             @ +0x13888
//   mTakedownEventQueue           @ +0x1B898
//   mPlayerVehicleControls        @ +0x1B9E8 (Set copies 60)

#include "types.hpp"
#include "GameShared/GameClasses/Module/CgsIOBuffer.h"   // CgsModule::IOBuffer
#include "GameSource/Physics/ContactSpies/BrnContactSpyInterface.h"  // BrnPhysics::ContactSpy::ContactSpyInterface (embedded by value in InputBuffer_PostPhysics)
#include "GameSource/World/AI/Route/BrnRouteMapModuleIO.h"  // BrnAI::RouteMapModuleIO::RaceRouteRequestQueue (GetRaceRouteRequestQueue return type)

// Pointer-only return types for the read-locked getters; full homes are included by
// the accessor .cpp (CgsTimerStatusInterface.h / BrnAIModuleRequestInterface.h).
namespace CgsSystem { class TimerStatusInterface; }

namespace BrnAI
{
namespace AIModuleIO
{
    struct RaceCarAIInterface;         // BrnRaceCarAIInterfaces.h
    struct AIModuleRequestInterface;   // BrnAIModuleRequestInterface.h

    struct InputBuffer : public CgsModule::IOBuffer
    {
        // Attested member start offsets (bytes from this) + copy sizes for the memcpy setters.
        enum EMemberOffset
        {
            KU_RACE_CAR_AI_INTERFACE_OFFSET             = 0x00010,
            KU_RACE_CAR_AI_INTERFACE_SIZE               = 0x043D0,   // 17360
            KU_TRAFFIC_AI_INTERFACE_OFFSET              = 0x043E0,
            KU_TRAFFIC_AI_INTERFACE_SIZE                = 0x0B7A0,   // 47008
            KU_TIMER_INTERFACE_OFFSET                   = 0x0FB80,
            KU_TIMER_INTERFACE_SIZE                     = 48,
            KU_AI_MODULE_REQUEST_INTERFACE_OFFSET       = 0x0FBB0,
            KU_GAME_ACTION_QUEUE_OFFSET                 = 0x103BC,
            KU_RACE_ROUTE_REQUEST_QUEUE_OFFSET          = 0x137D0,
            KU_RACE_CAR_RACE_DISTANCE_INTERFACE_OFFSET  = 0x13860,
            KU_RACE_CAR_RACE_DISTANCE_INTERFACE_SIZE    = 40,
            KU_SCENE_RESULT_QUEUE_OFFSET                = 0x13888,
            KU_TAKEDOWN_EVENT_QUEUE_OFFSET              = 0x1B898,
            KU_PLAYER_VEHICLE_CONTROLS_OFFSET           = 0x1B9E8,
            KU_PLAYER_VEHICLE_CONTROLS_SIZE             = 60,
        };

        // ---- getters (read-lock bit4, except Get()/GetGameActionQueue() -> write-lock bit3) ----
        // X360 0x8276D728 (R) -- the pre-scene race-car AI view (this+0x10).
        const RaceCarAIInterface* GetRaceCarAIInterface() const;
        // X360 0x8276D7D0 (R) -- the traffic-AI interface handle (this+0x43E0).
        const void* GetTrafficAI() const;
        // X360 0x8276D920 (R) -- the timer-status interface (this+0xFB80).
        const CgsSystem::TimerStatusInterface* GetTimerInterface() const;
        // X360 0x8276D878 (R) -- the AI-module request interface (this+0xFBB0).
        const AIModuleRequestInterface* GetAIModuleRequestInterface() const;
        // X360 0x8276D530 (R) -- the game-action queue (this+0x103BC). Read-lock twin of Get().
        const void* GetGameActionQueue() const;
        // X360 0x8276D5D8 (R) -- the player vehicle controls block (this+0x1B9E8).
        const void* GetPlayerVehicleControls() const;
        // X360 0x8276D488 (R) -- the race-route request queue (this+0x137D0).
        const RouteMapModuleIO::RaceRouteRequestQueue* GetRaceRouteRequestQueue() const;

        // X360 0x8279C4F8 (W) -- the game-action queue (this+0x103BC). Asserts the WRITE lock.
        void* Get();

        // ---- setters / appends (write-lock bit3) ----
        // X360 0x8279C700 (W) -- copies a RaceCarAIInterface into this+0x10.
        void SetRaceCarAIInterface(const RaceCarAIInterface* lpInterface);
        // X360 0x8279C7E0 (W) -- copies the traffic-AI interface into this+0x43E0.
        void SetTrafficAIInterface(const void* lpInterface);
        // X360 0x8279C8C0 (W) -- copies a TimerStatusInterface into this+0xFB80.
        void SetTimerInterface(const void* lpTimerInterface);
        // X360 0x8279C428 (W) -- copies the race-car race-distance interface into this+0x13860.
        void SetRaceCarRaceDistanceInterface(const void* lpObject);
        // X360 0x8279C5A0 (W) -- copies the player vehicle controls into this+0x1B9E8.
        void SetPlayerVehicleControls(const void* lpControls);
        // X360 0x827AC960 (W) -- Clear()+Append the reset-on-track request queue at this+0xFBB0.
        void AppendAIModuleRequestInterface(const void* lpRequestInterface);
        // X360 0x827A9560 (W) -- Append the race-route request queue at this+0x137D0.
        void AppendRaceRouteRequestQueue(const void* lpQueue);
        // X360 0x827A9618 (W) -- Clear()+Append the takedown event queue at this+0x1B898.
        void SetTakedownEventQueue(const void* lpQueue);

    protected:
        // Payload image: the buffer's members live at the attested EMemberOffset byte
        // offsets from `this`. MemberImage() returns the base so the accessors reproduce
        // the X360 `addi this, N` arithmetic verbatim. Sized to cover the last member.
        u8* MemberImage()             { return reinterpret_cast<u8*>(this); }
        const u8* MemberImage() const { return reinterpret_cast<const u8*>(this); }

    private:
        // Storage that gives the buffer object its X360 size. Starts right after the
        // IOBuffer base; spans through the trailing player-vehicle-controls block. The
        // accessors index from `this` (MemberImage), so this blob only sizes the object.
        u8 maImage[0x1BA30 - sizeof(CgsModule::IOBuffer)];
    };

    // ------------------------------------------------------------------------
    // InputBuffer_PostPhysics -- the AI module's post-physics INPUT buffer (a
    // SEPARATE, small CgsModule::IOBuffer from the large InputBuffer above). The
    // physics side fills it with contact-spy results; the AI post-physics step
    // drains it. Exactly one member per the DWARF (BrnAIModuleIO.h:286/:368):
    // the embedded contact-spy interface at +0x04 (right after the 1-byte IOBuffer
    // status flag). Construct/Destruct bodied in
    // BrnAIModuleIO_InputBuffer_PostPhysics.cpp.
    // ------------------------------------------------------------------------
    struct InputBuffer_PostPhysics : public CgsModule::IOBuffer
    {
        typedef BrnPhysics::ContactSpy::ContactSpyInterface ContactSpyInterface;

        void Construct();   // X360 0x8277BCD0
        void Destruct();    // X360 0x8277BCE8

    private:
        static void _AssertLayout();

        ContactSpyInterface mContactInterface;   // +0x04  DWARF BrnAIModuleIO.h:286
    };
}
}
