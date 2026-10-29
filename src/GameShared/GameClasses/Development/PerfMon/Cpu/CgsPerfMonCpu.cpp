#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "rw/rwcore_structs.h"
#include <xtl.h>
#include <pix.h>
#include <ppcintrinsics.h>
#include <cstddef>
#include <cstring>

#ifdef AddMonitor
#undef AddMonitor
#endif

namespace CgsDev
{
    PerfMonCpuInstance* PerfMonCpu::mpMonitorsArray = 0;
    s32 PerfMonCpu::miMaxMonitorCount = 0;
    s32 PerfMonCpu::miMonitorCount = 0;
    PerfMonGameFrequency PerfMonCpu::meGameFrequency = E_PMF_60HZ;
    bool PerfMonCpu::mbResetAllValues = false;
    s32 PerfMonCpu::miNumIterations = 0;
    bool PerfMonCpu::mbIgnoreZeroCallsInAverage = false;
    s32 PerfMonCpu::miLibPerfMode = 0;

    namespace
    {
        // ARTIST83018F14, refreshed by StartProfiling's frequency query.
        f32 gfCounterScale = 0.0f;
        const char* const gpcSource = "..\\..\\..\\GameShared\\GameClasses\\Development/PerfMon/Cpu/CgsPerfMonCpu.cpp";
        const char* const gpcX360Source = "..\\..\\..\\GameShared\\GameClasses\\Development/PerfMon/Cpu/X360/CgsPerfMonCpuX360.cpp";
        const char* const gpcX360Header = "..\\..\\..\\GameShared\\GameClasses\\Development/PerfMon/Cpu/X360/CgsPerfMonCpuX360.h";

        void MonitorAssert(const char* lpcExpression, const char* lpcFile, s32 liLine)
        {
            Assert::BeginAssert();
            Assert::FireAssert(lpcExpression, lpcFile, liLine);
            Assert::EndAssert();
        }
    }

    // ARTIST82824B10. The supplied allocator owns the registry's five-lane resource.
    bool PerfMonCpu::Construct(s32 liMaxMonitors, rw::IResourceAllocator* lpAllocator)
    {
        CGS_STATIC_ASSERT(sizeof(PerfMonCpuInstance) == 96, "ARTIST monitor stride");
        CGS_STATIC_ASSERT(offsetof(PerfMonCpuInstance, macName) == 28, "ARTIST monitor name");
        CGS_STATIC_ASSERT(offsetof(PerfMonCpuInstance, mbActive) == 60, "ARTIST monitor active");
        CGS_STATIC_ASSERT(offsetof(PerfMonCpuInstance, mePage) == 64, "ARTIST monitor page");
        CGS_STATIC_ASSERT(offsetof(PerfMonCpuInstance, mfCurrentValue) == 68, "ARTIST monitor value");
        CGS_STATIC_ASSERT(offsetof(PerfMonCpuInstance, miOrigLibPerfTraceId) == 88, "ARTIST monitor trace");
        CGS_STATIC_ASSERT(sizeof(PerfMonCpuMonitorData) == 44, "ARTIST monitor snapshot");
        miMaxMonitorCount = liMaxMonitors;
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size = sizeof(PerfMonCpuInstance) * liMaxMonitors;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 16;
        mpMonitorsArray = static_cast<PerfMonCpuInstance*>(lpAllocator->Allocate(lDescriptor, 0).m_baseResources[0]);
        if (!mpMonitorsArray)
            MonitorAssert("mpMonitorsArray", gpcSource, 71);
        miMonitorCount = 0;
        meGameFrequency = E_PMF_60HZ;
        mbResetAllValues = true;
        miNumIterations = 1;
        mbIgnoreZeroCallsInAverage = false;
        std::memset(mpMonitorsArray, 0, sizeof(PerfMonCpuInstance) * miMaxMonitorCount);
        return true;
    }

    void PerfMonCpu::SetGameFrequency(PerfMonGameFrequency leFrequency) { meGameFrequency = leFrequency; }
    void PerfMonCpu::SetNumIterationsTaken(s32 liNumIterations) { miNumIterations = liNumIterations; }
    s32 PerfMonCpu::GetMonitorCount() { return miMonitorCount; }
    s32 PerfMonCpu::GetMaxMonitorCount() { return miMaxMonitorCount; }
    void PerfMonCpu::ResetValuesInActiveMonitors() { mbResetAllValues = true; }

    // ARTIST82817238 leaves the last displayed current value and active flag intact.
    void PerfMonCpu::PrepareActiveMonitorsForUpdate()
    {
        for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
        {
            PerfMonCpuInstance& lrInstance = mpMonitorsArray[liIndex];
            lrInstance.miFrameCounter = 0;
            lrInstance.mu64StartValue = 0;
            lrInstance.mu64Value = 0;
            lrInstance.mfAverageValue = 0.0f;
            lrInstance.mfMinMaxValue = 0.0f;
            lrInstance.miNumCalls = 0;
            lrInstance.mfAverageAccumulator = 0.0f;
            lrInstance.miMaxCalls = 0;
        }
    }

    // ARTIST82824C30. Assertions report errors without replacing the append policy.
    s32 PerfMonCpu::AddMonitor(const char* lpcString, PerfMonCpuPage lePage,
                               bool lbRecordMinNotMax, f32 lfCpuBudget, bool lbScaled)
    {
        if (!(miMonitorCount < miMaxMonitorCount))
            MonitorAssert("miMonitorCount < miMaxMonitorCount", gpcSource, 264);
        if (!lpcString)
            MonitorAssert("lpcString != NULL", gpcSource, 267);
        if (!(std::strlen(lpcString) <= KI_PERFMONCPU_MAXSTRINGLENGTH - 1))
            MonitorAssert("strlen(lpcString) <= (KI_PERFMONCPU_MAXSTRINGLENGTH-1)", gpcSource, 268);
        for (s32 liCount = 0; liCount < miMonitorCount; ++liCount)
            if (std::strcmp(mpMonitorsArray[liCount].macName, lpcString) == 0)
                MonitorAssert("strcmp(mpMonitorsArray[liCount].macName, lpcString) != 0", gpcSource, 274);
        PerfMonCpuInstance& lrInstance = mpMonitorsArray[miMonitorCount];
        std::memset(&lrInstance, 0, sizeof(lrInstance));
        if (std::strlen(lpcString) >= KI_PERFMONCPU_MAXSTRINGLENGTH)
        {
            Assert::BeginAssert();
            StrStream lMessage(Assert::gpcMessageBuffer, Assert::giMessageBufferSize);
            lMessage << "String too long: " << lpcString;
            Assert::FireAssert(Assert::gpcMessageBuffer,
                "..\\..\\..\\GameShared\\GameClasses\\Core/CgsStringUtils.h", 55);
            Assert::EndAssert();
        }
        std::strncpy(lrInstance.macName, lpcString, KI_PERFMONCPU_MAXSTRINGLENGTH);
        lrInstance.mePage = lePage;
        lrInstance.mbMinimum = lbRecordMinNotMax;
        lrInstance.mfCpuBudget = lfCpuBudget;
        lrInstance.mbScaled = lbScaled;
        lrInstance.miLibPerfTraceId = -1;
        lrInstance.miOrigLibPerfTraceId = -1;
        lrInstance.miLibPerfTraceId = lrInstance.miOrigLibPerfTraceId;
        return miMonitorCount++;
    }

    // ARTIST821F1198 checks the allocated capacity, rather than the registered count.
    void PerfMonCpu::StartMonitor(s32 liMonitorID)
    {
        if (!(liMonitorID >= 0 && liMonitorID < miMaxMonitorCount))
            MonitorAssert("liMonitorID >= 0 && liMonitorID < miMaxMonitorCount", gpcX360Header, 61);
        PerfMonCpuInstance& lrInstance = mpMonitorsArray[liMonitorID];
        if (lrInstance.mbActive)
        {
            Assert::BeginAssert();
            StrStream lMessage(Assert::gpcMessageBuffer, Assert::giMessageBufferSize);
            lMessage << "Monitor already started: " << lrInstance.macName << "\n";
            Assert::FireAssert(Assert::gpcMessageBuffer, gpcX360Header, 64);
            Assert::EndAssert();
        }
        LARGE_INTEGER lNow;
        QueryPerformanceCounter(&lNow);
        lrInstance.mbActive = true;
        lrInstance.mu64StartValue = static_cast<u64>(lNow.QuadPart);
        ++lrInstance.miNumCalls;
    }

    // ARTIST821F1308 performs no capacity/count guard on this path.
    void PerfMonCpu::StopMonitor(s32 liMonitorID)
    {
        PerfMonCpuInstance& lrInstance = mpMonitorsArray[liMonitorID];
        if (!lrInstance.mbActive)
            MonitorAssert("mpMonitorsArray[liMonitorID].mbActive == true", gpcX360Header, 97);
        LARGE_INTEGER lNow;
        QueryPerformanceCounter(&lNow);
        lrInstance.mbActive = false;
        lrInstance.mu64Value += static_cast<u64>(lNow.QuadPart) - lrInstance.mu64StartValue;
    }

    // ARTIST828251D0 queries the timer frequency on every profiling frame.
    void PerfMonCpu::StartProfiling()
    {
        if (mbResetAllValues)
        {
            PrepareActiveMonitorsForUpdate();
            for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
            {
                PerfMonCpuInstance& lrInstance = mpMonitorsArray[liIndex];
                lrInstance.mfMinMaxValue = lrInstance.mbMinimum ? 100.0f : 0.0f;
                lrInstance.miMaxCalls = 0;
            }
            mbResetAllValues = false;
        }
        for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
        {
            mpMonitorsArray[liIndex].mu64Value = 0;
            mpMonitorsArray[liIndex].miNumCalls = 0;
        }
        LARGE_INTEGER lFrequency;
        if (!QueryPerformanceFrequency(&lFrequency))
            MonitorAssert("QueryPerformanceFrequency(&lFrequency)", gpcX360Source, 87);
        const f32 lfFrequency = static_cast<f32>(static_cast<double>(static_cast<u64>(lFrequency.QuadPart)));
        gfCounterScale = 100.0f / lfFrequency;
    }

    // ARTIST82825350: each conversion/multiply/divide rounds to single precision.
    void PerfMonCpu::StopProfiling()
    {
        LARGE_INTEGER lNow;
        QueryPerformanceCounter(&lNow);
        f32 lfGameFrequency;
        switch (meGameFrequency)
        {
        case E_PMF_60HZ: lfGameFrequency = 59.94f; break;
        case E_PMF_50HZ: lfGameFrequency = 50.0f; break;
        case E_PMF_30HZ: lfGameFrequency = 29.97f; break;
        case E_PMF_25HZ: lfGameFrequency = 25.0f; break;
        default:
            MonitorAssert("invalid game frequency", gpcX360Source, 137);
            lfGameFrequency = 59.94f;
            break;
        }
        const f32 lfInverseIterations = 1.0f / static_cast<f32>(miNumIterations);
        for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
        {
            PerfMonCpuInstance& lrInstance = mpMonitorsArray[liIndex];
            if (lrInstance.miNumCalls > lrInstance.miMaxCalls)
                lrInstance.miMaxCalls = lrInstance.miNumCalls;
            const f32 lfTicks = static_cast<f32>(static_cast<double>(lrInstance.mu64Value));
            const f32 lfTime = lfTicks * gfCounterScale;
            f32 lfCurrent = lfTime * lfGameFrequency;
            if (lrInstance.mbScaled)
            {
                lrInstance.miNumCalls /= miNumIterations;
                lfCurrent *= lfInverseIterations;
            }
            lrInstance.mfCurrentValue = lfCurrent;
            lrInstance.mfAverageAccumulator += lrInstance.mfCurrentValue;
            if (!mbIgnoreZeroCallsInAverage || lrInstance.mfCurrentValue > 0.0f)
                ++lrInstance.miFrameCounter;
            if (lrInstance.miFrameCounter > 0)
                lrInstance.mfAverageValue = lrInstance.mfAverageAccumulator / static_cast<f32>(lrInstance.miFrameCounter);
            const f32 lfDifference = lrInstance.mfMinMaxValue - lrInstance.mfCurrentValue;
            lrInstance.mfMinMaxValue = static_cast<f32>(lrInstance.mbMinimum
                ? __fsel(lfDifference, lrInstance.mfCurrentValue, lrInstance.mfMinMaxValue)
                : __fsel(lfDifference, lrInstance.mfMinMaxValue, lrInstance.mfCurrentValue));
        }
    }

    // ARTIST82824FF8. Trace values are zero in this build, independently of tag state.
    void PerfMonCpu::GetMonitorData(s32 liMonitorID, PerfMonCpuMonitorData* lpResult)
    {
        if (!lpResult)
            MonitorAssert("lpResult", gpcSource, 315);
        if (!(liMonitorID >= 0 && liMonitorID < miMonitorCount))
            MonitorAssert("liMonitorID >= 0 && liMonitorID < miMonitorCount", gpcSource, 316);
        const PerfMonCpuInstance& lrInstance = mpMonitorsArray[liMonitorID];
        lpResult->mpcName = lrInstance.macName;
        lpResult->mfCurrentValue = lrInstance.mfCurrentValue;
        lpResult->mfAverageValue = lrInstance.mfAverageValue;
        lpResult->mfMinMaxValue = lrInstance.mfMinMaxValue;
        lpResult->miMaxCalls = lrInstance.miMaxCalls;
        lpResult->mfCpuBudget = lrInstance.mfCpuBudget;
        lpResult->miNumCalls = lrInstance.miNumCalls;
        lpResult->mbTraced = false;
        lpResult->mfCurrentTraceValue = 0.0f;
        lpResult->mfAverageTraceValue = 0.0f;
        lpResult->mfMinMaxTraceValue = 0.0f;
    }

    // ARTIST82825148 (export hole), raw instructions attest the page-filtered callback.
    void PerfMonCpu::ReportMonitors(FPerfMonCpuReportCallback* lpDisplayCallback,
                                    void* lpUserData, PerfMonCpuPage lePage)
    {
        for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
            if (lePage == E_PMP_MAX || mpMonitorsArray[liIndex].mePage == lePage)
            {
                PerfMonCpuMonitorData lCallbackParameters;
                GetMonitorData(liIndex, &lCallbackParameters);
                lpDisplayCallback(lCallbackParameters, lpUserData);
            }
    }

    PerfMonCpuPage PerfMonCpu::GetMonitorPage(s32 liMonitorHandle)
    { return mpMonitorsArray[liMonitorHandle].mePage; }

    // ARTIST828172E8 uses the real SDK named-counter API with the monitor name as its format.
    void PerfMonCpu::AddPIXCounters()
    {
        for (s32 liIndex = 0; liIndex < miMonitorCount; ++liIndex)
            PIXAddNamedCounter(mpMonitorsArray[liIndex].mfCurrentValue, mpMonitorsArray[liIndex].macName);
    }
}
