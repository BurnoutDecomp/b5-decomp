#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"

// CgsSystem::FrameRateManager. StartUpdateFrame is the real (trivial) body. UpdatePostRenderWait
// is the variable-step catch-up: the full X360 body cycle-counts elapsed time
// (QueryPerformanceCounter) against mi64SimulationTimeStepCycles to decide how many fixed sim
// steps this render frame must run, clamped to [min,max] per the framerate-manager mode. That
// cycle-accurate algorithm is reconstructed separately; this minimal-real body locks to the
// minimum (>=1) so the update spine runs exactly one simulation step per frame - enough to
// drive the boot/loading flow - and records it. (CgsFrameRate.cpp)
namespace CgsSystem
{
    void FrameRateManager::Construct(f32 /*lfTargetFrameRate*/, f32 /*lfSimulationRate*/)
    {
        meFramerateType = E_FRAMERATEMANAGER_SINGLE;
        mi64PrevFrameTime = 0;
        mi64TotalTime = 0;
        mi64FramerateToleranceCycles = 0;
        mi64SimulationTimeStepCycles = 0;
        miPrevNumSimulationStepsRequired = 1;
        miNumSimulationSteps = 0;
        miNumExtraSimulationUpdatesRequested = 0;
        mbPaused = false;
        mbIsSimulationRunningInRealTime = true;
    }

    void FrameRateManager::Destruct() {}
    bool FrameRateManager::Prepare() { return true; }
    bool FrameRateManager::Release() { return true; }

    // @ 0x828D7760
    void FrameRateManager::StartUpdateFrame(EFrameRateManagerType leType, bool lbPaused)
    {
        meFramerateType = leType;
        mbPaused = lbPaused;
    }

    // @ 0x828D7770 - simplified: run the minimum number of simulation steps this frame
    // (>=1). The cycle-accurate elapsed-time calculation is reconstructed separately.
    s32 FrameRateManager::UpdatePostRenderWait(s32 liMinSimulationSteps, s32 liMaxSimulationSteps)
    {
        s32 liSteps = liMinSimulationSteps;
        if (liSteps < 1)
            liSteps = 1;
        if (liSteps > liMaxSimulationSteps && liMaxSimulationSteps >= 1)
            liSteps = liMaxSimulationSteps;
        miPrevNumSimulationStepsRequired = liSteps;
        miNumExtraSimulationUpdatesRequested = 0;
        return liSteps;
    }

    void FrameRateManager::AttemptToInsertExtraSimulationUpdates(s32 liNumUpdates)
    {
        miNumExtraSimulationUpdatesRequested = liNumUpdates;
    }

    void FrameRateManager::SetNumSimulationStepsAchieved(s32 liNumSteps)
    {
        miNumSimulationSteps = liNumSteps;
    }

    bool FrameRateManager::IsSimulationRunningInRealTime()
    {
        return mbIsSimulationRunningInRealTime;
    }
}
