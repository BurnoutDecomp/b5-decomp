#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"

#include <cstring>

#include "GameShared/GameClasses/Core/CgsAssert.h"

namespace CgsDev
{
    namespace
    {
        const rw::RGBA K_TOTALCOLOUR(255, 255, 255, 255);
        const rw::RGBA K_REMAINDERCOLOUR(255, 0, 0, 255);
    }

    PerfMonGpuInstance PerfMonGpu::maMonitors[KI_PERFMONGPUMAXMONITORS + 1];
    s32 PerfMonGpu::miMaxMonitor = 0;
    PerfMonGameFrequency PerfMonGpu::meGameFrequency = E_PMF_60HZ;
    bool PerfMonGpu::mbProfilingRunning = false;
    bool PerfMonGpu::mbActiveMonitor = false;
    s32 PerfMonGpu::miActiveMonitorID = -1;

    s32 PerfMonGpu::AddMonitor(const char* lpcString, const rw::RGBA& lrColour,
                               f32 lfGPUBudget)
    {
        CGS_ASSERT(miMaxMonitor < KI_PERFMONGPUMAXMONITORS,
                   "miMaxMonitor < KI_PERFMONGPUMAXMONITORS");
        CGS_ASSERT(lpcString, "lpcString != NULL");
        CGS_ASSERT(std::strlen(lpcString) <= KI_PERFMONGPUMAXSTRINGLENGTH - 1,
                   "strlen(lpcString) <= (KI_PERFMONGPUMAXSTRINGLENGTH-1)");

        for (s32 liIndex = 0; liIndex < miMaxMonitor; ++liIndex)
            CGS_ASSERT(std::strcmp(maMonitors[liIndex].macName, lpcString) != 0,
                       "strcmp(maMonitors[liCount].macName, lpcString) != 0");

        const s32 liMonitorId = miMaxMonitor;
        PerfMonGpuInstance& lrMonitor = maMonitors[liMonitorId];
        lrMonitor = PerfMonGpuInstance();
        std::strcpy(lrMonitor.macName, lpcString);
        lrMonitor.mColour = lrColour;
        lrMonitor.mfGPUBudget = lfGPUBudget;
        ++miMaxMonitor;
        return liMonitorId;
    }

    void PerfMonGpu::ReportMonitors(FPerfMonGpuReportCallback* lpfReportCallback,
                                    void* lpUserData, bool lbReportTotal)
    {
        f32 lfTotal = 0.0f;

        for (s32 liIndex = 0; liIndex < miMaxMonitor; ++liIndex)
        {
            const f32 lfValue = ComputeValueFromTimer(liIndex);
            const PerfMonGpuInstance& lrMonitor = maMonitors[liIndex];
            lfTotal += lfValue;

            PerfMonGpuMonitorData lParameters =
            {
                lrMonitor.macName,
                lfValue,
                lrMonitor.mColour,
                lfValue > lrMonitor.mfGPUBudget
            };
            lpfReportCallback(lParameters, lpUserData);
        }

        if (lbReportTotal)
        {
            const PerfMonGpuMonitorData lParameters =
            {
                "TOTAL MONITORED",
                lfTotal,
                K_TOTALCOLOUR,
                false
            };
            lpfReportCallback(lParameters, lpUserData);
        }

        const f32 lfRenderedTotal = ComputeValueFromTimer(KI_PERFMONGPUMAXMONITORS);
        const f32 lfRemainder = lfRenderedTotal - lfTotal;
        if (lfRemainder > 0.0f)
        {
            const PerfMonGpuMonitorData lParameters =
            {
                "UNMONITORED",
                lfRemainder,
                K_REMAINDERCOLOUR,
                false
            };
            lpfReportCallback(lParameters, lpUserData);
        }

        if (lbReportTotal)
        {
            const PerfMonGpuMonitorData lParameters =
            {
                "TOTAL RENDERED",
                lfRenderedTotal,
                K_TOTALCOLOUR,
                false
            };
            lpfReportCallback(lParameters, lpUserData);
        }
    }

    f32 PerfMonGpu::GetLastTotal()
    {
        f32 lfTotal = 0.0f;
        for (s32 liIndex = 0; liIndex < miMaxMonitor; ++liIndex)
            lfTotal += ComputeValueFromTimer(liIndex);
        return lfTotal;
    }

    f32 PerfMonGpu::GetLastMonitorValue(s32 liMonitorId)
    {
        CGS_ASSERT(liMonitorId < miMaxMonitor, "liMonitorId < miMaxMonitor");
        return ComputeValueFromTimer(liMonitorId);
    }
}
