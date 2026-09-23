// b5-decomp/src/GameShared/GameClasses/System/Timer/CgsTimerStatusInterface.h
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/System/Timer/CgsTime.h"   // CgsSystem::Time

// CgsSystem::TimerStatusInterface - a snapshot of the game-side and sim-side timer
// state, published once per frame to consumers (IO input/output buffers, replay,
// network, gamestate). Layout + member NAMES are authoritative from DecFIGS DWARF
// (System/Timer/CgsTimerStatusInterface.h:44/91): the interface is just two
// back-to-back TimerStatus blocks (game then sim). The X360 binary agrees: Clear
// zeroes a 48-byte object as two 24-byte TimerStatus runs.
//
// TimerStatus layout (DWARF :67-71, all members confirmed by Clear's stores):
//   miFrameCount         @ +0  (int32)   <- *(a1+0)  = 0
//   mfBaseTimeStep       @ +4  (float32) <- *(a1+4)  = 0.0
//   mfTimeStepMultiplier @ +8  (float32) <- *(a1+8)  = 1.0
//   mbRunning            @ +12 (bool)    <- *(a1+12) = 0
//   mTime                @ +16 (Time, 8B: miSeconds@+16, mfFraction@+20)
// => sizeof(TimerStatus) == 24. TimerStatusInterface = { game @ +0, sim @ +24 },
//    sizeof == 48 (Clear writes the sim block at +24..+44).
//
// CgsTimerStatusInterface.cpp owns TimerStatusInterface::Clear, operator=, StoreTimers,
// IsSimTimerFrequency50Hz and TimerStatus::operator=. The four GetGameTimerStatus /
// GetSimTimerStatus overloads, TimerStatus's accessors and TimerStatus::Clear are inline
// below. IsGameTimerFrequency50Hz is the one declaration without a body.
namespace CgsSystem
{
    class Timer;   // fwd: StoreTimers param (declared-only here)

    class TimerStatus
    {
    public:
        s32  GetFrameCount() const;            // DWARF :48 (declared-only)
        f32  GetBaseTimeStep() const;          // DWARF :51 (declared-only)
        f32  GetTimeStepMultiplier() const;    // DWARF :54 (declared-only)
        f32  GetCurrentTimeStep() const;       // DWARF :57
        bool IsRunning() const;                // DWARF :60 (declared-only)
        Time GetTime() const;                  // DWARF :63 (declared-only)

    private:
        void Clear();                                   // DWARF :74
        TimerStatus& operator=(const TimerStatus&);

        s32  miFrameCount;          // DWARF :67  @ +0
        f32  mfBaseTimeStep;        // DWARF :68  @ +4
        f32  mfTimeStepMultiplier;  // DWARF :69  @ +8
        bool mbRunning;             // DWARF :70  @ +12
        Time mTime;                 // DWARF :71  @ +16  (miSeconds@+16, mfFraction@+20)

        friend class TimerStatusInterface;  // interface Clear()/queries reach private state
    };

    class TimerStatusInterface
    {
    public:
        void Clear();                                                       // DWARF :95
        TimerStatusInterface& operator=(const TimerStatusInterface&);

        const TimerStatus* GetGameTimerStatus() const;
        const TimerStatus* GetSimTimerStatus() const;
        TimerStatus*       GetGameTimerStatus();
        TimerStatus*       GetSimTimerStatus();

        bool IsGameTimerFrequency50Hz() const;            // DWARF :113 (declared-only)
        bool IsSimTimerFrequency50Hz() const;             // DWARF :116

        void StoreTimers(Timer* lpGameTimer, Timer* lpSimTimer);

    private:
        TimerStatus mGameTimerStatus;   // DWARF :123  @ +0
        TimerStatus mSimTimerStatus;    // DWARF :124  @ +24
    };

    // ---- GetCurrentTimeStep: base step scaled by the multiplier --------------
    // (inlined into IsSimTimerFrequency50Hz on X360 as mfBaseTimeStep*mfTimeStepMultiplier;
    //  defined here so the query reads named members, not raw offsets.)
    inline f32
    TimerStatus::GetCurrentTimeStep() const
    {
        return mfBaseTimeStep * mfTimeStepMultiplier;
    }

    // ---- the trivial member queries (2026-07-29) ------------------------------
    // Each is a single named-member load -- the shape the X360 inlines everywhere it reads a
    // timer status (e.g. MainDirector::UpdateCameraBehavioursPostScene @0x8224FD30 asserts
    // `...GetGameTimerStatus()->IsRunning()` then reads `[+8] * [+4]` for the game step and
    // `[+32] * [+28]` / `[+28]` for the sim pair -- i.e. the two sub-statuses at +0 and +24).
    // Defined here so consumers reach them BY NAME instead of by raw offset into a foreign
    // type; no other TU defines them (checked).
    inline s32  TimerStatus::GetFrameCount() const         { return miFrameCount; }
    inline f32  TimerStatus::GetBaseTimeStep() const       { return mfBaseTimeStep; }
    inline f32  TimerStatus::GetTimeStepMultiplier() const { return mfTimeStepMultiplier; }
    inline bool TimerStatus::IsRunning() const             { return mbRunning; }
    inline Time TimerStatus::GetTime() const               { return mTime; }

    inline const TimerStatus* TimerStatusInterface::GetGameTimerStatus() const { return &mGameTimerStatus; }
    inline const TimerStatus* TimerStatusInterface::GetSimTimerStatus()  const { return &mSimTimerStatus; }
    inline TimerStatus*       TimerStatusInterface::GetGameTimerStatus()       { return &mGameTimerStatus; }
    inline TimerStatus*       TimerStatusInterface::GetSimTimerStatus()        { return &mSimTimerStatus; }

    // ---- TimerStatus::Clear (private) ----------------------------------------
    // Recovered from the X360 store pattern that TimerStatusInterface::Clear inlines
    // twice. Resets to: frame 0, zero base step, unity multiplier, stopped, zero time.
    inline void
    TimerStatus::Clear()
    {
        miFrameCount         = 0;
        mfBaseTimeStep       = 0.0f;
        mfTimeStepMultiplier = 1.0f;
        mbRunning            = false;
        mTime.SetFraction(0.0f);   // CgsSystem::Time::SetFraction(this+16, 0.0)
        mTime.SetSeconds(0);       // *(this+16) = 0
    }
}
