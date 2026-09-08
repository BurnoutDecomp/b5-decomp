#include "GameShared/GameClasses/Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonCpu.h"

#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"  // DrawBox
#include "GameShared/GameClasses/Development/DebugSystem/Core/UI/CgsDebugUI.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"                                            // CGS_ASSERT (LogBufferEnable)

#include <cstdio>
#include <cstring>

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
        const f32 KAF_COLUMN_OFFSETS[6] = { 0.0f, 270.0f, 340.0f, 410.0f, 510.0f, 580.0f };
        const RGBA KC_WHITE       = 0xFFFFFFFFu;
        const RGBA KC_OVER_BUDGET = 0xFF6496DCu;
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
        maStringList[E_PMP_MAX + 1].miValue = 0;
        maStringList[E_PMP_MAX + 1].mpcName = nullptr;

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

    // ARTIST 0x82831DC8. Register the complete Paradise CPU-monitor menu surface.
    void DebugComponentPerfMonCpu::OnActivate()
    {
        RegisterVariable(&miCurrentPage, "Page");
        SetRange(&miCurrentPage, -1, E_PMP_MAX - 1);
        SetOptions(&miCurrentPage, maStringList);
        RegisterVariable(&mbDisplayAsGraph, "Draw As Graph");
        RegisterVariable(&PerfMonCpu::mbIgnoreZeroCallsInAverage,
                         "Don't avg zero-called perfmons");
        RegisterFunction(&DebugCallbackResetCounters, this, "Reset CPU Monitors");
        if (mpLogBuffer)
            RegisterFunction(&DebugCallbackDumpLogFile, this, "Dump CPU Log Files");
    }

    // ARTIST 0x8282CDA0. The UI callback defers reset until the perfmon frame boundary.
    void DebugComponentPerfMonCpu::Update()
    {
        if (mbResetMonitors)
        {
            PerfMonCpu::ResetValuesInActiveMonitors();
            mbResetMonitors = false;
        }
        LogBufferUpdate();
    }

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

    // ARTIST 0x82826368. Text table laid out from the DebugUI metrics and the six data-section
    // column offsets recovered from ARTIST (0,270,340,410,510,580).
    void DebugComponentPerfMonCpu::RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender )
    {
        if (miCurrentPage == -1)
            return;

        const DebugUI::Metrics& lrMetrics = GetUI().GetMetrics();
        const f32 lfFontSize = lrMetrics.mfTextSize;
        const f32 lfX = lrMetrics.mfScreenBorderLeft;
        f32 lfY = lrMetrics.mfScreenBorderTop;

        lpDebug2DRender->DrawText(maStringList[miCurrentPage + 1].mpcName,
                                  lfX + KAF_COLUMN_OFFSETS[0],
                                  lfY - lfFontSize * 1.5f, lfFontSize * 1.5f, KC_WHITE);
        lpDebug2DRender->DrawText("Current", lfX + KAF_COLUMN_OFFSETS[1],
                                  lfY - lfFontSize, lfFontSize, KC_WHITE);
        lpDebug2DRender->DrawText("Average", lfX + KAF_COLUMN_OFFSETS[2],
                                  lfY - lfFontSize, lfFontSize, KC_WHITE);
        lpDebug2DRender->DrawText("Min/Max", lfX + KAF_COLUMN_OFFSETS[3],
                                  lfY - lfFontSize, lfFontSize, KC_WHITE);
        lpDebug2DRender->DrawText("Calls", lfX + KAF_COLUMN_OFFSETS[4],
                                  lfY - lfFontSize, lfFontSize, KC_WHITE);
        lpDebug2DRender->DrawText("Max Calls", lfX + KAF_COLUMN_OFFSETS[5],
                                  lfY - lfFontSize, lfFontSize, KC_WHITE);

        const s32 liCount = PerfMonCpu::GetMonitorCount();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            if (PerfMonCpu::GetMonitorPage(liIndex) != miCurrentPage)
                continue;

            PerfMonCpuMonitorData lData;
            PerfMonCpu::GetMonitorData(liIndex, &lData);

            char acValue[32];
            const RGBA lNameColour = (lData.mfAverageValue > lData.mfCpuBudget)
                ? KC_OVER_BUDGET : KC_WHITE;
            lpDebug2DRender->DrawText(lData.mpcName, lfX + KAF_COLUMN_OFFSETS[0],
                                      lfY, lfFontSize, lNameColour);

            std::snprintf(acValue, sizeof(acValue), "%.03f", lData.mfCurrentValue);
            lpDebug2DRender->DrawText(acValue, lfX + KAF_COLUMN_OFFSETS[1], lfY, lfFontSize,
                                      lData.mfCurrentValue > lData.mfCpuBudget ? KC_OVER_BUDGET : KC_WHITE);
            std::snprintf(acValue, sizeof(acValue), "%.03f", lData.mfAverageValue);
            lpDebug2DRender->DrawText(acValue, lfX + KAF_COLUMN_OFFSETS[2], lfY, lfFontSize,
                                      lData.mfAverageValue > lData.mfCpuBudget ? KC_OVER_BUDGET : KC_WHITE);
            std::snprintf(acValue, sizeof(acValue), "%.03f", lData.mfMinMaxValue);
            lpDebug2DRender->DrawText(acValue, lfX + KAF_COLUMN_OFFSETS[3], lfY, lfFontSize,
                                      lData.mfMinMaxValue > lData.mfCpuBudget ? KC_OVER_BUDGET : KC_WHITE);
            std::snprintf(acValue, sizeof(acValue), "%d", lData.miNumCalls);
            lpDebug2DRender->DrawText(acValue, lfX + KAF_COLUMN_OFFSETS[4],
                                      lfY, lfFontSize, KC_WHITE);
            std::snprintf(acValue, sizeof(acValue), "%d", lData.miMaxCalls);
            lpDebug2DRender->DrawText(acValue, lfX + KAF_COLUMN_OFFSETS[5],
                                      lfY, lfFontSize, KC_WHITE);
            lfY += lfFontSize;
        }
    }

    // ARTIST 0x82826820. Paradise's graph is a single fixed strip for the selected page's
    // "Update cost" and "Render cost" monitors; it is not a history graph.
    void DebugComponentPerfMonCpu::RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRender )
    {
        if (miCurrentPage == -1)
            return;

        lpDebug2DRender->DrawFrame(32.0f, 408.0f, 608.0f, 412.0f,
                                   0xFF000000u, 2.0f);
        lpDebug2DRender->DrawBox(32.0f, 408.0f, mfMaxCpu * 2.8799999f,
                                 4.0f, 0xFF252525u);

        f32 lfX = 32.0f;
        f32 lfTotal = 0.0f;
        const s32 liCount = PerfMonCpu::GetMonitorCount();
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            if (PerfMonCpu::GetMonitorPage(liIndex) != miCurrentPage)
                continue;

            PerfMonCpuMonitorData lData;
            PerfMonCpu::GetMonitorData(liIndex, &lData);
            if (std::strcmp(lData.mpcName, "Update cost") != 0 &&
                std::strcmp(lData.mpcName, "Render cost") != 0)
                continue;

            const f32 lfWidth = lData.mfCurrentValue * 2.8800001f;
            lpDebug2DRender->DrawBox(lfX, 408.0f, lfWidth, 4.0f, 0xFFCC59CCu);
            lfX += lfWidth;
            lfTotal += lData.mfCurrentValue;
        }

        if (lfTotal > mfMaxCpu)
            mfMaxCpu = (lfTotal > 200.0f) ? 200.0f : lfTotal;
    }

    void DebugComponentPerfMonCpu::SetPageName(s32 liPage, const char* lpcPageName)
    {
        CGS_ASSERT(liPage >= 0 && liPage < E_PMP_MAX,
                   "liPage >= 0 && liPage < E_PMP_MAX");
        maStringList[liPage + 1].mpcName = lpcPageName;
    }

    void DebugComponentPerfMonCpu::GetCurrentPage(s32* lpiPage, char* lpcName, s32 liNameLen)
    {
        *lpiPage = miCurrentPage;
        if (miCurrentPage < 0 || miCurrentPage >= E_PMP_MAX)
        {
            lpcName[0] = '\0';
            return;
        }

        const char* lpcPageName = maStringList[miCurrentPage + 1].mpcName;
        CGS_ASSERT(static_cast<s32>(std::strlen(lpcPageName)) < liNameLen, "String too long");
        std::strncpy(lpcName, lpcPageName, static_cast<size_t>(liNameLen));
    }

    bool DebugComponentPerfMonCpu::SetFirstPage()
    {
        miCurrentPage = 0;
        return true;
    }

    bool DebugComponentPerfMonCpu::SetNextPage()
    {
        ++miCurrentPage;
        if (static_cast<u32>(miCurrentPage) > static_cast<u32>(E_PMP_MAX - 1))
        {
            SetNoPage();
            return false;
        }
        return true;
    }

    void DebugComponentPerfMonCpu::SetNoPage()
    {
        miCurrentPage = -1;
    }

    void DebugComponentPerfMonCpu::DebugCallbackResetCounters(void* lpUserData)
    {
        static_cast<DebugComponentPerfMonCpu*>(lpUserData)->mbResetMonitors = true;
    }

    void DebugComponentPerfMonCpu::DebugCallbackDumpLogFile(void* lpUserData)
    {
        char acLogFileName[] = "d:\\PM_Log";
        static_cast<DebugComponentPerfMonCpu*>(lpUserData)->LogBufferDump(acLogFileName);
    }

    // ARTIST 0x82826CB0. One frame record is a u16 monitor count followed by one 10-bit
    // fixed-point u16 value per registered monitor.
    void DebugComponentPerfMonCpu::LogBufferUpdate()
    {
        if (!mpLogBuffer)
            return;

        const s32 liCount = PerfMonCpu::GetMonitorCount();
        *mpau16LogBufferWritePtr++ = static_cast<u16>(liCount);
        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            PerfMonCpuMonitorData lData;
            PerfMonCpu::GetMonitorData(liIndex, &lData);
            *mpau16LogBufferWritePtr++ = static_cast<u16>(lData.mfCurrentValue * 1024.0f);
        }

        if (reinterpret_cast<u8*>(mpau16LogBufferWritePtr) >= static_cast<u8*>(mpLogBufferEnd))
        {
            mpau16LogBufferWritePtr = static_cast<u16*>(mpLogBuffer);
            mbLogBufferOverflow = true;
        }
    }

    // ARTIST 0x82826D70. Dump one CSV per perfmon page, preserving the circular-record order.
    void DebugComponentPerfMonCpu::LogBufferDump(char* lpcFileName)
    {
        if (!mpLogBuffer)
            return;

        for (s32 liPage = 0; liPage < E_PMP_MAX; ++liPage)
        {
            char acFileName[256];
            std::snprintf(acFileName, sizeof(acFileName), "%s_%s.csv", lpcFileName,
                          maStringList[liPage + 1].mpcName);
            std::FILE* lpFile = std::fopen(acFileName, "w");
            if (!lpFile)
                break;

            const s32 liMonitorCount = PerfMonCpu::GetMonitorCount();
            for (s32 liIndex = 0; liIndex < liMonitorCount; ++liIndex)
            {
                if (PerfMonCpu::GetMonitorPage(liIndex) == liPage)
                {
                    PerfMonCpuMonitorData lData;
                    PerfMonCpu::GetMonitorData(liIndex, &lData);
                    std::fprintf(lpFile, "%s, ", lData.mpcName);
                }
            }
            std::fprintf(lpFile, "\n");

            u16* lpRecord = mbLogBufferOverflow
                ? mpau16LogBufferWritePtr : static_cast<u16*>(mpLogBuffer);
            u16* const lpEndRecord = mpau16LogBufferWritePtr;
            do
            {
                const u16 luCount = *lpRecord++;
                for (u16 luIndex = 0; luIndex < luCount; ++luIndex)
                {
                    if (luIndex < static_cast<u16>(liMonitorCount) &&
                        PerfMonCpu::GetMonitorPage(luIndex) == liPage)
                    {
                        std::fprintf(lpFile, "%.03f, ",
                                     static_cast<f32>(lpRecord[luIndex]) * 0.0009765625f);
                    }
                }
                lpRecord += luCount;
                std::fprintf(lpFile, "\n");

                if (reinterpret_cast<u8*>(lpRecord) >= static_cast<u8*>(mpLogBufferEnd))
                {
                    CGS_ASSERT(mbLogBufferOverflow, "mbLogBufferOverflow");
                    lpRecord = static_cast<u16*>(mpLogBuffer);
                }
            }
            while (lpRecord != lpEndRecord);

            std::fclose(lpFile);
        }
    }
}
