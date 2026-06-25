#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/Development/PerfMon/CgsPerfMon.h"

namespace CgsDev
{
    static const s32 KI_PERFMONGPUMAXMONITORS = 32;
    static const s32 KI_PERFMONGPUMAXSTRINGLENGTH = 32;

    struct PerfMonGpuMonitorData
    {
        const char* mpcName;
        f32 mfCurrentValue;
        rw::RGBA mColour;
        bool mbOverBudget;
    };

    typedef void FPerfMonGpuReportCallback(const PerfMonGpuMonitorData& lrParameters,
                                           void* lpUserData);

    struct PerfMonGpuInstance
    {
        char macName[KI_PERFMONGPUMAXSTRINGLENGTH];
        rw::RGBA mColour;
        u64 mu64Counter;
        f32 mfGPUBudget;
    };

    class PerfMonGpu
    {
    public:
        static bool Construct();
        static void Destruct();
        static void SetGameFrequency(PerfMonGameFrequency leFrequency);
        static void Swap();
        static void StartProfiling();
        static void StopProfiling();

        static void ReportMonitors(FPerfMonGpuReportCallback* lpfReportCallback,
                                   void* lpUserData, bool lbReportTotal);
        static f32 GetLastTotal();
        static f32 GetLastMonitorValue(s32 liMonitorId);
        static s32 AddMonitor(const char* lpcString, const rw::RGBA& lrColour,
                              f32 lfGPUBudget);
        static void StartMonitor(s32 liMonitorId);
        static void StopMonitor(s32 liMonitorId);

    private:
        static f32 ComputeValueFromTimer(s32 liMonitorId);

        static PerfMonGpuInstance maMonitors[KI_PERFMONGPUMAXMONITORS + 1];
        static s32 miMaxMonitor;
        static PerfMonGameFrequency meGameFrequency;
        static bool mbProfilingRunning;
        static bool mbActiveMonitor;
        static s32 miActiveMonitorID;
    };
}
