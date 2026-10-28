#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (the console's own assert in this body)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the console's own trace)

#include <xtl.h>   // QueryPerformanceCounter / QueryPerformanceFrequency
                       // -- the console calls QueryPerformanceCounter directly at 0x828D778C.

// ARTIST FrameRateManager: Prepare 0x828D7730, StartUpdateFrame 0x828D7760,
// UpdatePostRenderWait 0x828D7770. One simulation step is required per display frame.
namespace CgsSystem
{
    namespace
    {
        s64 PerformanceCounter()
        {
            LARGE_INTEGER lCounter;
            QueryPerformanceCounter(&lCounter);
            return lCounter.QuadPart;
        }

        s64 PerformanceFrequency()
        {
            LARGE_INTEGER lFrequency;
            QueryPerformanceFrequency(&lFrequency);
            return lFrequency.QuadPart;
        }
    }

    // The DWARF signature is Construct(float32_t lrSimulationTimeStep,
    // float32_t lrFramerateToleranceTimeMs) with locals li64Time / li64Frequency
    // (DecFIGS CgsFrameRate.cpp:49-52), i.e. it reads the platform timer's frequency and
    // converts both MILLISECOND arguments into counter cycles. The caller
    // (BrnGameModule::Construct @0x823CAE3C-68) passes
    //     a2 = simTimer.mfRate * simTimer.mfScaleCurrent * 1000.0f   (one step, in ms)
    //     a3 = simTimer.mfRate * simTimer.mfScaleCurrent *  100.0f   (a tenth of it)
    // so at the console's 1/60 s step these are 16.667 ms and 1.667 ms.
    void FrameRateManager::Construct(f32 lrSimulationTimeStep, f32 lrFramerateToleranceTimeMs)
    {
        const s64 li64Frequency = PerformanceFrequency();

        mi64SimulationTimeStepCycles =
            static_cast<s64>((static_cast<f64>(li64Frequency) * static_cast<f64>(lrSimulationTimeStep)) / 1000.0);
        mi64FramerateToleranceCycles =
            static_cast<s64>((static_cast<f64>(li64Frequency) * static_cast<f64>(lrFramerateToleranceTimeMs)) / 1000.0);

        meFramerateType                      = E_FRAMERATEMANAGER_SINGLE;
        mi64PrevFrameTime                    = PerformanceCounter();
        mi64TotalTime                        = 0;
        miPrevNumSimulationStepsRequired     = 0;
        miNumSimulationSteps                 = 0;
        miNumExtraSimulationUpdatesRequested = 0;
        mbPaused                             = false;
        mbIsSimulationRunningInRealTime      = true;

    }

    void FrameRateManager::Destruct() {}

    // @ 0x828D7730 -- verbatim. `li r10, 0` then `std r10, 0x10` / `std r10, 8` store the
    // 64-bit ZERO register into both time accumulators (the IDA pseudocode renders the pair
    // as 0x100000000LL by folding in the neighbouring `li r9, 1`; the asm is two zero
    // stores). The two cycle constants at +0x18/+0x20 are NOT touched -- they belong to
    // Construct -- which is why Prepare can be called repeatedly to re-base the timing
    // without losing the step size.
    bool FrameRateManager::Prepare()
    {
        meFramerateType                      = E_FRAMERATEMANAGER_SINGLE;
        mi64TotalTime                        = 0;
        mi64PrevFrameTime                    = 0;
        miNumSimulationSteps                 = 0;
        miPrevNumSimulationStepsRequired     = 0;
        miNumExtraSimulationUpdatesRequested = 0;
        mbIsSimulationRunningInRealTime      = true;

        return true;
    }

    bool FrameRateManager::Release() { return true; }

    // @ 0x828D7760 -- three instructions: `stw r4, 0(r3)` / `stb r5, 0x34(r3)` / `blr`.
    void FrameRateManager::StartUpdateFrame(EFrameRateManagerType leType, bool lbPaused)
    {
        meFramerateType = leType;
        mbPaused        = lbPaused;
    }

    // @ 0x828D7770. Local names are the DWARF's (CgsFrameRate.cpp:148-150, 170, 196, 231).
    s32 FrameRateManager::UpdatePostRenderWait(s32 liMinSimulationSteps, s32 liMaxSimulationSteps)
    {
        const s64 li64Time = PerformanceCounter();   // asm 0x828D778C

        if (!mbPaused && miNumSimulationSteps + miPrevNumSimulationStepsRequired <= 0)
        {
            miPrevNumSimulationStepsRequired = liMinSimulationSteps;
            mi64PrevFrameTime = li64Time;
            return liMinSimulationSteps;
        }

        // ---- the debt -------------------------------------------------------------------
        // asm 0x828D77A0-D8 (the accumulate) and 0x828D77F4 (the paused arm, which pins the
        // debt at -1 so no catch-up is ever requested while paused).
        s64 li64TimeBehind = -1;
        if (!mbPaused)
        {
            miNumSimulationSteps += miPrevNumSimulationStepsRequired;
            mi64TotalTime        += (li64Time - mi64PrevFrameTime);

            const s64 li64SimulationTime =
                mi64SimulationTimeStepCycles * static_cast<s64>(miNumSimulationSteps);
            li64TimeBehind = mi64TotalTime - li64SimulationTime;
        }
        mi64PrevFrameTime = li64Time;   // asm 0x828D7800, taken on both arms

        // ---- how many steps are owed ----------------------------------------------------
        s32 liNumSimulationStepsRequired = liMinSimulationSteps;
        if (li64TimeBehind > mi64FramerateToleranceCycles)
        {
            const s64 li64Numerator = li64TimeBehind + mi64SimulationTimeStepCycles
                                      - mi64FramerateToleranceCycles - 1;
            liNumSimulationStepsRequired = liMinSimulationSteps
                + static_cast<s32>(li64Numerator / mi64SimulationTimeStepCycles);
        }

        // ---- the max clamp, and the backlog repair --------------------------------------
        // asm 0x828D783C-8C.
        if (liNumSimulationStepsRequired > liMaxSimulationSteps)
        {
            liNumSimulationStepsRequired    = liMaxSimulationSteps;
            mbIsSimulationRunningInRealTime = false;

            // asm 0x828D7864-84. GIVE UP the time we are not going to simulate, so the debt
            // does not compound frame after frame into a death spiral. Rearranged, this store
            // is `mi64TotalTime = simulated + (max - min) * step`, i.e. the accounting is
            // re-based on the steps this frame is actually about to run. MULTIPLE_UNCAPPED
            // deliberately skips it (it wants the backlog kept).
            if (meFramerateType != E_FRAMERATEMANAGER_MULTIPLE_UNCAPPED && !mbPaused)
            {
                mi64TotalTime += static_cast<s64>(liMaxSimulationSteps - liMinSimulationSteps)
                                     * mi64SimulationTimeStepCycles
                               - li64TimeBehind;
            }
        }
        else
        {
            mbIsSimulationRunningInRealTime = true;   // asm 0x828D7888
        }

        // asm 0x828D7890-98: recorded BEFORE the SINGLE override, and not at all while paused.
        if (!mbPaused)
            miPrevNumSimulationStepsRequired = liNumSimulationStepsRequired;

        // asm 0x828D789C-A8: SINGLE means exactly the minimum, whatever the clock says.
        if (meFramerateType == E_FRAMERATEMANAGER_SINGLE)
            liNumSimulationStepsRequired = liMinSimulationSteps;

        // ---- the requested extra updates ------------------------------------------------
        // asm 0x828D78AC-5C. Only honoured on a frame that is not already catching up.
        if (miNumExtraSimulationUpdatesRequested > 0
            && liNumSimulationStepsRequired == liMinSimulationSteps)
        {
            const s32 liHeadroom = liMaxSimulationSteps - liMinSimulationSteps;
            if (miNumExtraSimulationUpdatesRequested >= liHeadroom)
                miNumExtraSimulationUpdatesRequested = liHeadroom;   // DWARF: rw::core::stdc::Min

            if ((CgsDev::Message::gxMessageFilterFlags & 1) != 0 && CgsDev::Log::gpDebugPrint != 0)
            {
                *CgsDev::Log::gpDebugPrint << "Inserting an extra "
                                           << miNumExtraSimulationUpdatesRequested
                                           << " simulation updates\n";
            }

            liNumSimulationStepsRequired += miNumExtraSimulationUpdatesRequested;
            CGS_ASSERT(liNumSimulationStepsRequired <= liMaxSimulationSteps,
                       "liNumSimulationStepsRequired <= liMaxSimulationSteps");

            // asm 0x828D7944-5C: charge the inserted steps to the clock so they are not
            // handed back again next frame.
            mi64TotalTime += mi64SimulationTimeStepCycles
                                 * static_cast<s64>(miNumExtraSimulationUpdatesRequested);
        }

        miNumExtraSimulationUpdatesRequested = 0;   // asm 0x828D7964

        return liNumSimulationStepsRequired;
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
