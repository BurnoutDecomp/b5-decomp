#pragma once

#include "types.hpp"

// CgsSystem::FrameRateManager - drives the variable-step simulation catch-up: each rendered
// frame the manager decides how many fixed simulation steps to run (single / capped /
// uncapped). Layout + API reconstructed from the DecFIGS DWARF
// (System/Timer/CgsFrameRate.h). StartUpdateFrame begins a frame in the requested mode; the
// owning game module records the step it is on as it runs the catch-up loop.
namespace CgsSystem
{
    // CgsSystem::EFrameRate - the display refresh rate the game is running at. This is the
    // canonical home (CgsFrameRate.h per the DecFIGS DWARF); CgsNetworkManager.cpp still
    // carries a stripped local copy for its own translation unit (CgsNetworkPlayer.h now
    // includes this header instead of its former local copy).
    // Values are the literal Hz figures the ARTIST asm compares against (ThreadLayout::Begin
    // seeds meFrameRate with 60 = E_FRAMERATE_60HZ).
    enum EFrameRate
    {
        E_FRAMERATE_UNKNOWN = -1,
        E_FRAMERATE_25HZ    = 25,
        E_FRAMERATE_30HZ    = 30,
        E_FRAMERATE_50HZ    = 50,
        E_FRAMERATE_60HZ    = 60,
    };

    enum EFrameRateManagerType
    {
        E_FRAMERATEMANAGER_SINGLE = 0,
        E_FRAMERATEMANAGER_MULTIPLE_CAPPED = 1,
        E_FRAMERATEMANAGER_MULTIPLE_UNCAPPED = 2,
    };

    struct FrameRateManager
    {
        // Note: the DWARF marks these protected; they are surfaced here because the owning
        // BrnGameModule writes miPrevNumSimulationStepsRequired directly while iterating the
        // per-frame simulation catch-up loop (BrnGameModule::GameMain).
        EFrameRateManagerType meFramerateType;
        s64                   mi64PrevFrameTime;
        s64                   mi64TotalTime;
        s64                   mi64FramerateToleranceCycles;
        s64                   mi64SimulationTimeStepCycles;
        s32                   miPrevNumSimulationStepsRequired;
        s32                   miNumSimulationSteps;
        s32                   miNumExtraSimulationUpdatesRequested;
        bool                  mbPaused;
        bool                  mbIsSimulationRunningInRealTime;

        void Construct(f32 lfTargetFrameRate, f32 lfSimulationRate);
        void Destruct();
        bool Prepare();
        bool Release();
        void StartUpdateFrame(EFrameRateManagerType leType, bool lbResetTiming);
        s32  UpdatePostRenderWait(s32 liArg0, s32 liArg1);
        void AttemptToInsertExtraSimulationUpdates(s32 liNumUpdates);
        void SetNumSimulationStepsAchieved(s32 liNumSteps);
        bool IsSimulationRunningInRealTime();
    };
}
