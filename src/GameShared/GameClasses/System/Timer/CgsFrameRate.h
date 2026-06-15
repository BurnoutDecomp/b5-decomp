#pragma once

#include "types.hpp"

// CgsSystem::FrameRateManager - drives the variable-step simulation catch-up: each rendered
// frame the manager decides how many fixed simulation steps to run (single / capped /
// uncapped). Layout + API reconstructed from the DecFIGS DWARF
// (System/Timer/CgsFrameRate.h). StartUpdateFrame begins a frame in the requested mode; the
// owning game module records the step it is on as it runs the catch-up loop.
namespace CgsSystem
{
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
