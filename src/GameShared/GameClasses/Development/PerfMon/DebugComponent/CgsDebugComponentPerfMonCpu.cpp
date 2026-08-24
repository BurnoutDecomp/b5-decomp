#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // DrawBox
#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT (LogBufferEnable)

// CgsDev::DebugComponentPerfMonCpu - the CPU performance overlay bodies. RenderHUD draws one row per
// registered CPU monitor (the on-screen "debug squares"): a dark track the full bar width, with a
// coloured value bar over it whose length is the monitor's current time scaled to mfMaxCpu - green
// under its CPU budget, red over. The data comes from the PerfMonCpu registry (GetMonitorCount /
// GetMonitorData / IsMonitorOverBudget). Drawn in the loading-screen Im2d pixel space (1280x720).
//
// BOUNDED: the graph mode, page navigation, the per-monitor text labels (the deferred font path), and
// the reset/dump/tracing callbacks are the perfmon follow-on; the bar overlay needs none of them.
// Construct is the faithful bring-up (PS3 DecFIGS @0xB21C44; the X360 body @0x8282CC98 is not in the
// export set, but its caller/wrapper shape is identical): it initialises the full overlay state AND
// brings up the PerfMonCpu registry itself (PerfMonCpu::Construct(count, GetAllocator())) - the
// registry construction lives HERE on the console, not in DebugManager::Construct.

namespace CgsDev
{
    // Per-monitor row height in the overlay (pixels). The exact X360 value is data-section TBD; a
    // 12px row stacks the ~38 game monitors within the loading-screen height.
    const f32 DebugComponentPerfMonCpu::KF_COLORBARHEIGHT = 12.0f;

    namespace
    {
        // Overlay layout in the 1280x720 Im2d pixel space.
        const f32 KF_BAR_X         = 40.0f;    // left edge of the bars
        const f32 KF_BAR_TOP       = 60.0f;    // top of the first row
        const f32 KF_BAR_MAX_WIDTH = 220.0f;   // bar width at full scale (mfMaxCpu)

        // Packed RGBA (u32 -> RGBA8 {r,g,b,a} via Debug2DImmediateRender::AddVertex's reinterpret;
        // little-endian, so 0xAABBGGRR). All full-alpha (opaque) so they show regardless of blend state.
        const RGBA KC_TRACK = 0xFF202020u;   // dark row track   (r=32,g=32,b=32)
        const RGBA KC_UNDER = 0xFF00FF00u;   // under budget     (green)
        const RGBA KC_OVER  = 0xFF0000FFu;   // over budget      (red)
    }

    // Faithful port of PS3 DecFIGS Construct @0xB21C44 (X360 wrapper @0x8282CC98, called by
    // DebugManager::Construct with (count, logBuffer, logBufferSize)):
    //   - page list: maStringList[0] = {-1, "None"}, then [i+1] = {i, "Unused"} for the 24 pages
    //     (SetPageName renames the live ones later);
    //   - overlay state cleared (mfMaxCpu starts at 0 - the bar scale is grown by Update);
    //   - trace-mode option list {0,"Trace"} / {1,"XBPerf"} / terminator;
    //   - the PerfMonCpu REGISTRY constructed here, over the debug allocator
    //     (Internal::DebugInternal::GetAllocator());
    //   - the optional CPU-trace log buffer wired via LogBufferEnable (else mpLogBuffer = NULL;
    //     mpLogBufferEnd is only ever set by LogBufferEnable, as on the console).
    void DebugComponentPerfMonCpu::Construct( s16 liMaxMonitors, void* lpLogBuffer, u32 luLogBufferSize )
    {
        miCurrentPage = -1;
        maStringList[0].miValue = -1;
        maStringList[0].mpcName = "None";
        for (s32 liPage = 0; liPage < E_PMP_MAX; ++liPage)
        {
            maStringList[liPage + 1].miValue = liPage;
            maStringList[liPage + 1].mpcName = "Unused";
        }

        mfMaxCpu              = 0.0f;
        mbResetMonitors       = false;
        mbDisplayAsGraph      = false;
        mbEnableTracing       = false;
        miTraceMode           = 0;
        maTraceModeList[0].miValue = 0;
        maTraceModeList[0].mpcName = "Trace";
        maTraceModeList[1].miValue = 1;
        maTraceModeList[1].mpcName = "XBPerf";
        maTraceModeList[2].miValue = 0;
        maTraceModeList[2].mpcName = nullptr;
        mbLogBufferOverflow     = false;
        mpau16LogBufferWritePtr = nullptr;

        PerfMonCpu::Construct(liMaxMonitors, GetAllocator());

        if (lpLogBuffer)
            LogBufferEnable(lpLogBuffer, static_cast<s32>(luLogBufferSize));
        else
            mpLogBuffer = nullptr;
    }

    // Faithful port of X360 LogBufferEnable @0x82826C20: wire the CPU-trace log region. The usable
    // size is the supplied size minus the registry's max monitor count (asserted positive); the
    // write cursor starts at the buffer base.
    void DebugComponentPerfMonCpu::LogBufferEnable( void* lpBuffer, s32 liSize )
    {
        if (!lpBuffer)
        {
            mpLogBuffer = nullptr;
            return;
        }

        const s32 liLogBufferSize = liSize - PerfMonCpu::GetMaxMonitorCount();
        CGS_ASSERT(liLogBufferSize > 0, "liLogBufferSize > 0");

        mpLogBuffer             = lpBuffer;
        mpLogBufferEnd          = static_cast<u8*>(lpBuffer) + liLogBufferSize;
        mpau16LogBufferWritePtr = static_cast<u16*>(lpBuffer);
        mbLogBufferOverflow     = false;
    }

    void DebugComponentPerfMonCpu::Destruct() {}

    // X360 OnActivate (CgsDebugComponentPerfMonCpu.cpp:136, bounded): nothing extra to bring up for the
    // bar overlay (the registry is always live); the page reset / trace setup is the follow-on.
    void DebugComponentPerfMonCpu::OnActivate() {}

    // X360 Update (CgsDebugComponentPerfMonCpu.cpp:216, bounded): the registry computes each monitor's
    // current time in StopMonitor, so the overlay has nothing to advance per frame yet (the trace/log
    // update + page input is the follow-on).
    void DebugComponentPerfMonCpu::Update() {}

    // X360 RenderHUD (CgsDebugComponentPerfMonCpu.cpp:246): draw the active page. Bounded to the table
    // (bar) view; the graph view is the follow-on.
    void DebugComponentPerfMonCpu::RenderHUD( Debug2DImmediateRender* lpDebug2DRender )
    {
        if (!lpDebug2DRender)
            return;

        if (mbDisplayAsGraph)
            RenderPerformanceGraph( lpDebug2DRender );
        else
            RenderPerformanceTable( lpDebug2DRender );
    }

    // X360 RenderPerformanceTable (CgsDebugComponentPerfMonCpu.cpp:271, bounded): one row per monitor -
    // a dark full-width track + a coloured value bar (current time / mfMaxCpu), red over budget else
    // green. The per-monitor text label is the deferred font path.
    void DebugComponentPerfMonCpu::RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender )
    {
        const s32 liCount   = PerfMonCpu::GetMonitorCount();
        const f32 lfMaxCpu  = (mfMaxCpu > 0.0f) ? mfMaxCpu : 33.3f;
        const f32 lfRowH    = KF_COLORBARHEIGHT;
        const f32 lfBarH    = lfRowH - 2.0f;   // 2px gap between rows

        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            PerfMonCpuMonitorData lData;
            PerfMonCpu::GetMonitorData( liIndex, &lData );

            const f32 lfY = KF_BAR_TOP + static_cast<f32>(liIndex) * lfRowH;

            // The row track (always drawn, so the overlay is visible even before a region has timed).
            lpDebug2DRender->DrawBox( KF_BAR_X, lfY, KF_BAR_MAX_WIDTH, lfBarH, KC_TRACK );

            // The value bar.
            f32 lfFraction = lData.mfCurrentValue / lfMaxCpu;
            if (lfFraction < 0.0f) lfFraction = 0.0f;
            if (lfFraction > 1.0f) lfFraction = 1.0f;
            const f32 lfWidth = KF_BAR_MAX_WIDTH * lfFraction;

            if (lfWidth > 0.0f)
            {
                const RGBA lColour = PerfMonCpu::IsMonitorOverBudget( liIndex ) ? KC_OVER : KC_UNDER;
                lpDebug2DRender->DrawBox( KF_BAR_X, lfY, lfWidth, lfBarH, lColour );
            }
        }
    }

    // X360 RenderPerformanceGraph (CgsDebugComponentPerfMonCpu.cpp:440): the history/graph view (the
    // monitors plotted over time). Deferred - the bounded overlay uses the table (bar) view; defined as
    // an empty body so RenderHUD's table/graph switch links. The graph plotting is the perfmon follow-on.
    void DebugComponentPerfMonCpu::RenderPerformanceGraph( Debug2DImmediateRender* /*lpDebug2DRender*/ )
    {
    }

    // --- perfmon follow-on (declared, deferred): graph view, page navigation, the reset/dump/tracing
    // debug callbacks, and the CPU-trace log buffer. None is needed for the bar overlay; bodies land
    // with the perfmon menu + trace reconstruction. They are intentionally left undefined (not called
    // and non-virtual, so the link stays closed). ---
}
