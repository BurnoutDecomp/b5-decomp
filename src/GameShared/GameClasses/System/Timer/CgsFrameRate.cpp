#include "GameShared/GameClasses/System/Timer/CgsFrameRate.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT (the console's own assert in this body)
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // gpDebugPrint / gxMessageFilterFlags (the console's own trace)

#include <Windows.h>   // QueryPerformanceCounter / QueryPerformanceFrequency
                       // -- the console calls QueryPerformanceCounter directly at 0x828D778C.

// ============================================================================
// CgsSystem::FrameRateManager
//
//   Construct              @ (inlined caller BrnGameModule::Construct 0x823CAE68)
//   Prepare                @ 0x828D7730
//   StartUpdateFrame       @ 0x828D7760
//   UpdatePostRenderWait   @ 0x828D7770
//
// This is the machine that decides, once per rendered frame, how many fixed-size
// simulation steps the update spine must run. It was a stub returning "the minimum,
// at least one" until 2026-08-17; the bodies below are the real ones, taken from the
// ARTIST assembly store-for-store, plus one clearly-marked PC extension.
//
// ---- THE ACCOUNTING, IN ONE PARAGRAPH ---------------------------------------
// mi64TotalTime accumulates REAL elapsed cycles since the first pass, and
// miNumSimulationSteps accumulates the number of simulation steps that have actually
// been run. The difference between real time and simulated time --
//     li64TimeBehind = mi64TotalTime - mi64SimulationTimeStepCycles * miNumSimulationSteps
// -- is the debt, and the whole function is "how many steps do I owe?". Because both
// halves are CUMULATIVE the accounting is self-correcting: a frame that runs too few
// or too many steps is paid back automatically on later frames, and the long-run
// average is exactly one step per mi64SimulationTimeStepCycles of wall-clock, whatever
// the render rate does.
//
// ---- ⚠️ THE PC DEVIATION, STATED PLAINLY ------------------------------------
// The console always asks for a MINIMUM OF ONE step per rendered frame
// (BrnGameModule::Construct seeds mi8FrameRateMinSteps = 1, mi8FrameRateMaxSteps = 3),
// because it is vsync-locked at the same rate the simulation runs at: one displayed
// frame IS one simulation tick, and the manager only ever has to decide whether to run
// one, two or three of them to catch up after a slow frame. On a PC that renders faster
// than 60 Hz that minimum is the bug -- it made the simulation run at the RENDER rate
// (144 Hz panel, VSync off => 144 ticks/second => 2.4x speed).
//
// So the game module now asks for a minimum of ZERO, and this function grows one extra
// branch for that case. The two modes are strictly separate:
//
//   liMinSimulationSteps >= 1  -- CONSOLE MODE. Byte-for-byte the ARTIST body. The
//                                interpolation alpha is published as 1.0, which makes
//                                every downstream blend a no-op, so this path renders
//                                exactly what it rendered before.
//   liMinSimulationSteps == 0  -- DECOUPLED MODE. A step is owed only once a WHOLE
//                                mi64SimulationTimeStepCycles of debt has accrued
//                                (floor, not the console's tolerance-biased ceil -- see
//                                the comment at the branch), and the leftover fraction
//                                is published as the interpolation alpha so the render
//                                legs can blend between the last two simulation states.
//
// Everything else -- the cumulative accounting, the max clamp, the "give up the backlog"
// repair, the SINGLE override, the extra-updates insertion -- is shared and is the
// console's.
// ============================================================================
namespace CgsSystem
{
    namespace
    {
        s64 HostPerformanceCounter()
        {
            LARGE_INTEGER lCounter;
            QueryPerformanceCounter(&lCounter);
            return lCounter.QuadPart;
        }

        s64 HostPerformanceFrequency()
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
        const s64 li64Frequency = HostPerformanceFrequency();

        mi64SimulationTimeStepCycles =
            static_cast<s64>((static_cast<f64>(li64Frequency) * static_cast<f64>(lrSimulationTimeStep)) / 1000.0);
        mi64FramerateToleranceCycles =
            static_cast<s64>((static_cast<f64>(li64Frequency) * static_cast<f64>(lrFramerateToleranceTimeMs)) / 1000.0);

        // A zero step would divide by zero in UpdatePostRenderWait (the console guards the
        // same divide with `tdllei r11, 0` at 0x828D7814, i.e. it traps rather than
        // tolerating it). One 60 Hz step is the value every caller on this build passes.
        if (mi64SimulationTimeStepCycles <= 0)
            mi64SimulationTimeStepCycles = li64Frequency / 60;

        meFramerateType                      = E_FRAMERATEMANAGER_SINGLE;
        mi64PrevFrameTime                    = HostPerformanceCounter();
        mi64TotalTime                        = 0;
        miPrevNumSimulationStepsRequired     = 0;
        miNumSimulationSteps                 = 0;
        miNumExtraSimulationUpdatesRequested = 0;
        mbPaused                             = false;
        mbIsSimulationRunningInRealTime      = true;

        mbTimingReset             = true;
        mfInterpolationAlpha      = 1.0f;
        mbDecoupledFromRenderRate = false;
        mi64PerformanceFrequency  = (li64Frequency > 0) ? li64Frequency : 1;
        mfLastFrameSeconds        = lrSimulationTimeStep / 1000.0f;
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

        // ⚠️ FLAG PC: re-arm the first-pass latch. On the console the zeroing of the two
        // step counters IS the re-arm, because the console's first-pass test reads them
        // (`miNumSimulationSteps + miPrevNumSimulationStepsRequired <= 0`). The latch
        // replaces that test -- see UpdatePostRenderWait -- so it has to be re-armed here
        // to keep Prepare's meaning intact.
        mbTimingReset        = true;
        mfInterpolationAlpha = 1.0f;
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
        const s64 li64Time = HostPerformanceCounter();   // asm 0x828D778C

        mbDecoupledFromRenderRate = (liMinSimulationSteps <= 0);

        // ⚠️ FLAG PC: the real length of the frame just drawn. Purely reported -- nothing in
        // the pacing below reads it.
        if (mi64PerformanceFrequency > 0 && !mbTimingReset)
        {
            mfLastFrameSeconds = static_cast<f32>(static_cast<f64>(li64Time - mi64PrevFrameTime)
                                                  / static_cast<f64>(mi64PerformanceFrequency));
        }

        // ---- the first pass -------------------------------------------------------------
        // asm 0x828D77A0-D8. The console reaches this arm when
        // `miNumSimulationSteps + miPrevNumSimulationStepsRequired <= 0`, latches the clock
        // and returns the minimum WITHOUT accumulating -- there is no previous frame to
        // measure against yet.
        //
        // ⚠️ FLAG PC: the test is a latch here, not that sum.
        //   * At CONSOLE settings the two are the same predicate, provably: the sum starts
        //     at 0+0, every later frame adds miPrevNumSimulationStepsRequired which is at
        //     least liMinSimulationSteps >= 1, so the sum is 0 on the first pass and
        //     strictly positive on every pass after it. Same branch, same frames.
        //   * At liMinSimulationSteps == 0 the sum can stay 0 forever -- a frame that runs
        //     no steps adds nothing -- so the console's phrasing would take this arm on
        //     EVERY frame and the simulation would never start at all. The latch is what
        //     makes a zero-step frame representable.
        if (!mbPaused && mbTimingReset)
        {
            mbTimingReset                    = false;
            miPrevNumSimulationStepsRequired = liMinSimulationSteps;
            mi64PrevFrameTime                = li64Time;
            mfInterpolationAlpha             = 1.0f;
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
        if (mbDecoupledFromRenderRate)
        {
            // ⚠️ FLAG PC (decoupled mode). A step is owed only once a WHOLE step of real
            // time has gone unsimulated.
            //
            // WHY NOT THE CONSOLE FORMULA. The console's is
            //     min + ceil((debt - tolerance) / step)
            // which, with min >= 1, means "you are getting one step anyway; here is an EXTRA
            // one if you have fallen more than a tenth of a step behind". The tolerance is an
            // anti-thrash threshold on the extra step, and it is correct in that role. Drop
            // min to 0 and the same expression starts issuing the FIRST step a tenth of a
            // step early, so the simulation runs permanently AHEAD of the display: the newest
            // simulated state is then in the future, the leftover fraction is negative, and
            // the interpolation alpha pins at 0 -- which is exactly the 60 Hz judder the
            // decoupling exists to remove. floor() puts the simulation an honest [0,1) step
            // BEHIND, which is what makes the blend below meaningful.
            liNumSimulationStepsRequired =
                (li64TimeBehind > 0)
                    ? static_cast<s32>(li64TimeBehind / mi64SimulationTimeStepCycles)
                    : 0;
        }
        else if (li64TimeBehind > mi64FramerateToleranceCycles)
        {
            // asm 0x828D780C-38: ceil((debt - tolerance) / step), added to the minimum.
            const s64 li64Numerator =
                li64TimeBehind + mi64SimulationTimeStepCycles - mi64FramerateToleranceCycles - 1;
            liNumSimulationStepsRequired =
                liMinSimulationSteps + static_cast<s32>(li64Numerator / mi64SimulationTimeStepCycles);
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

        // ---- ⚠️ FLAG PC: the interpolation alpha ----------------------------------------
        // How much real time will STILL be unsimulated once this frame's steps have run,
        // expressed as a fraction of one step.
        //
        // Derived from the live fields rather than from li64TimeBehind, so that every
        // adjustment made above -- the backlog repair, the inserted extra updates -- is
        // already folded in:
        //     remainder = mi64TotalTime - step * (stepsSoFar + stepsAboutToRun)
        //
        // The render legs then draw lerp(previousTick, currentTick, alpha). That is a
        // constant ONE STEP of display latency (16.7 ms) in exchange for continuous motion
        // at any render rate -- the standard trade, and the only one available: the newest
        // simulated state is the furthest forward anything can be drawn without inventing
        // motion that has not been simulated.
        //
        // In console mode it is pinned at 1.0, which makes every downstream blend degenerate
        // to "use the current state" -- so that path renders exactly what it always did.
        if (!mbDecoupledFromRenderRate || mbPaused)
        {
            mfInterpolationAlpha = 1.0f;
        }
        else
        {
            const s64 li64Remainder =
                mi64TotalTime - mi64SimulationTimeStepCycles
                                    * (static_cast<s64>(miNumSimulationSteps)
                                       + static_cast<s64>(liNumSimulationStepsRequired));
            f32 lfAlpha = static_cast<f32>(static_cast<f64>(li64Remainder)
                                           / static_cast<f64>(mi64SimulationTimeStepCycles));
            if (lfAlpha < 0.0f)
                lfAlpha = 0.0f;
            if (lfAlpha > 1.0f)
                lfAlpha = 1.0f;
            mfInterpolationAlpha = lfAlpha;
        }

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
