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
        // ---- the console layout (DWARF member order; X360 offsets pinned by the asm) -----
        // Base is game module + 0x9A0B50 (BrnGameModule::Construct @0x823C9EA8 forms it at
        // 0x823CAE2C for the Construct call, and UpdateFrameRateType @0x823BD0A8 reaches
        // meFrameRateManagerType at 0x9A0B88 immediately after it).
        //
        // Note: the DWARF marks these protected; they are surfaced here because the owning
        // BrnGameModule writes miPrevNumSimulationStepsRequired directly while iterating the
        // per-frame simulation catch-up loop (BrnGameModule::GameMain).
        EFrameRateManagerType meFramerateType;                  // +0x00
        s64                   mi64PrevFrameTime;                // +0x08
        s64                   mi64TotalTime;                    // +0x10
        s64                   mi64FramerateToleranceCycles;     // +0x18
        s64                   mi64SimulationTimeStepCycles;     // +0x20
        s32                   miPrevNumSimulationStepsRequired; // +0x28
        s32                   miNumSimulationSteps;             // +0x2C
        s32                   miNumExtraSimulationUpdatesRequested; // +0x30
        bool                  mbPaused;                         // +0x34
        bool                  mbIsSimulationRunningInRealTime;  // +0x35

        // ---- PC additions (past the end of the console object) ---------------------------
        // ⚠️ FLAG PC quality-of-life: these three exist only for the decoupled 60 Hz
        // simulation mode described on UpdatePostRenderWait. They are appended AFTER the
        // console members so the console prefix keeps its layout; nothing on this build
        // depends on sizeof(FrameRateManager) matching the console's 0x38.

        // The first-pass latch. The console spells this test as
        // `miNumSimulationSteps + miPrevNumSimulationStepsRequired <= 0` (asm 0x828D77A8);
        // see UpdatePostRenderWait for why that phrasing cannot survive a zero-step frame
        // and why this latch is exactly equivalent at console settings.
        bool                  mbTimingReset;

        // The fraction of a simulation step that has elapsed since the last tick, in [0,1].
        // Published to CgsSystem::FrameInterpolation by the game module each rendered frame.
        // Always 1.0 in console-locked pacing, which makes every blend a no-op.
        f32                   mfInterpolationAlpha;

        // Set when the caller asked for a minimum of zero simulation steps per rendered
        // frame, i.e. when the renderer is allowed to outrun the simulation.
        bool                  mbDecoupledFromRenderRate;

        // The platform counter frequency, latched once in Construct. The console converts
        // both Construct arguments to cycles and then never needs the frequency again; this
        // build also reports the real length of the last rendered frame, which does.
        s64                   mi64PerformanceFrequency;

        // Real wall-clock seconds between the last two UpdatePostRenderWait calls, i.e. the
        // length of the last rendered frame. For the PC bring-up stand-ins that advance
        // something per rendered frame and have no simulation tick to hang off. NEVER a
        // simulation timestep -- that is mfRate * mfScaleCurrent off the timers, always.
        f32                   mfLastFrameSeconds;

        // Both parameters are MILLISECONDS, per the DecFIGS DWARF signature
        // (CgsFrameRate.cpp:49 -- `Construct(float32_t lrSimulationTimeStep,
        // float32_t lrFramerateToleranceTimeMs)`), and the console passes
        // `simTimer.mfRate * simTimer.mfScaleCurrent` scaled by 1000 and by 100
        // respectively (BrnGameModule::Construct @0x823CAE3C-68, constants flt_82009E10 =
        // 1000.0 and flt_820049E0 = 100.0) -- i.e. one simulation step in ms, and a
        // tolerance of one TENTH of that.
        void Construct(f32 lrSimulationTimeStep, f32 lrFramerateToleranceTimeMs);
        void Destruct();
        bool Prepare();
        bool Release();
        void StartUpdateFrame(EFrameRateManagerType leType, bool lbPaused);
        s32  UpdatePostRenderWait(s32 liMinSimulationSteps, s32 liMaxSimulationSteps);
        void AttemptToInsertExtraSimulationUpdates(s32 liNumUpdates);
        void SetNumSimulationStepsAchieved(s32 liNumSteps);
        bool IsSimulationRunningInRealTime();

        // ⚠️ FLAG PC quality-of-life (no console counterpart).
        f32  GetInterpolationAlpha() const { return mfInterpolationAlpha; }
        bool IsDecoupledFromRenderRate() const { return mbDecoupledFromRenderRate; }
        f32  GetLastFrameSeconds() const { return mfLastFrameSeconds; }
    };
}
