#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"

#include <Windows.h>   // QueryPerformanceCounter / QueryPerformanceFrequency (high-res timer)
#include <new>         // ::operator new[] (monitor array backing)

// winspool.h (pulled in by Windows.h) macro-defines AddMonitor -> AddMonitorA, which would rename our
// registry entry point and break the link against callers that don't include Windows.h. Drop the macro.
#ifdef AddMonitor
#undef AddMonitor
#endif

// CgsDev::PerfMonCpu - the CPU perfmon registry bodies. The frame's update/render spine brackets each
// region with StartMonitor/StopMonitor; the elapsed time per region is converted to milliseconds via
// the QPC frequency (the X360 read the PPC timebase - CgsTimeUtils.cpp sets the QPC precedent). The
// debug perfmon component reads mfCurrentValue / the snapshot per monitor to draw the overlay bars.

namespace CgsDev
{
    namespace PerfMonCpu
    {
        namespace
        {
            PerfMonCpuInstance* gpMonitorsArray   = nullptr;
            s32                 giMaxMonitorCount  = 0;
            s32                 giMonitorCount     = 0;
            s32                 giNumIterations    = 1;
            s64                 gi64TimerFrequency = 0;

            u64 GetTimerTicks()
            {
                LARGE_INTEGER liNow;
                QueryPerformanceCounter(&liNow);
                return static_cast<u64>(liNow.QuadPart);
            }

            f32 TicksToMs(u64 lu64Ticks)
            {
                if (gi64TimerFrequency == 0)
                    return 0.0f;
                return static_cast<f32>(static_cast<double>(lu64Ticks) * 1000.0 / static_cast<double>(gi64TimerFrequency));
            }

            bool IsValidHandle(s32 liHandle)
            {
                return gpMonitorsArray && liHandle >= 0 && liHandle < giMonitorCount;
            }
        }

        bool Construct(s32 liMaxMonitorCount, rw::IResourceAllocator* /*lpAllocator*/)
        {
            LARGE_INTEGER liFreq;
            QueryPerformanceFrequency(&liFreq);
            gi64TimerFrequency = liFreq.QuadPart;

            giMaxMonitorCount = liMaxMonitorCount;
            giMonitorCount    = 0;

            gpMonitorsArray = static_cast<PerfMonCpuInstance*>(
                ::operator new[](static_cast<size_t>(liMaxMonitorCount) * sizeof(PerfMonCpuInstance)));

            for (s32 liIndex = 0; liIndex < liMaxMonitorCount; ++liIndex)
            {
                PerfMonCpuInstance& lrInstance = gpMonitorsArray[liIndex];
                lrInstance.mu64StartValue      = 0;
                lrInstance.mu64Value           = 0;
                lrInstance.miFrameCounter      = 0;
                lrInstance.miNumCalls          = 0;
                lrInstance.miMaxCalls          = 0;
                lrInstance.macName[0]          = '\0';
                lrInstance.mbActive            = false;
                lrInstance.mbMinimum           = false;
                lrInstance.mbScaled            = false;
                lrInstance.mbLibPerfTagged     = false;
                lrInstance.mePage              = E_PMP_GENERAL;
                lrInstance.mfCurrentValue      = 0.0f;
                lrInstance.mfMinMaxValue       = 0.0f;
                lrInstance.mfAverageValue      = 0.0f;
                lrInstance.mfAverageAccumulator= 0.0f;
                lrInstance.mfCpuBudget         = 0.0f;
                lrInstance.miOrigLibPerfTraceId= 0;
                lrInstance.miLibPerfTraceId    = 0;
            }
            return true;
        }

        void Destruct()
        {
            if (gpMonitorsArray)
            {
                ::operator delete[](gpMonitorsArray);
                gpMonitorsArray = nullptr;
            }
            giMonitorCount    = 0;
            giMaxMonitorCount = 0;
        }

        s32 AddMonitor(const char* lpcName, PerfMonCpuPage lePage, bool lbMinimum, f32 lfCpuBudget, bool lbLibPerfTagged)
        {
            if (!gpMonitorsArray || giMonitorCount >= giMaxMonitorCount)
                return -1;

            const s32 liHandle = giMonitorCount++;
            PerfMonCpuInstance& lrInstance = gpMonitorsArray[liHandle];

            s32 liChar = 0;
            if (lpcName)
                for (; liChar < KI_PERFMONCPU_MAXSTRINGLENGTH - 1 && lpcName[liChar]; ++liChar)
                    lrInstance.macName[liChar] = lpcName[liChar];
            lrInstance.macName[liChar] = '\0';

            lrInstance.mePage          = lePage;
            lrInstance.mbMinimum       = lbMinimum;
            lrInstance.mfCpuBudget     = lfCpuBudget;
            lrInstance.mbLibPerfTagged = lbLibPerfTagged;
            lrInstance.mbActive        = true;
            return liHandle;
        }

        void StartMonitor(s32 liMonitorHandle)
        {
            if (!IsValidHandle(liMonitorHandle))
                return;
            gpMonitorsArray[liMonitorHandle].mu64StartValue = GetTimerTicks();
        }

        void StopMonitor(s32 liMonitorHandle)
        {
            if (!IsValidHandle(liMonitorHandle))
                return;

            PerfMonCpuInstance& lrInstance = gpMonitorsArray[liMonitorHandle];
            const u64 lu64Elapsed = GetTimerTicks() - lrInstance.mu64StartValue;

            lrInstance.mu64Value      = lu64Elapsed;
            lrInstance.mfCurrentValue = TicksToMs(lu64Elapsed);
            ++lrInstance.miNumCalls;

            lrInstance.mfAverageAccumulator += lrInstance.mfCurrentValue;
            lrInstance.mfAverageValue = (lrInstance.miNumCalls > 0)
                ? lrInstance.mfAverageAccumulator / static_cast<f32>(lrInstance.miNumCalls)
                : lrInstance.mfCurrentValue;
            if (lrInstance.mfCurrentValue > lrInstance.mfMinMaxValue)
                lrInstance.mfMinMaxValue = lrInstance.mfCurrentValue;
        }

        void SetNumIterationsTaken(s32 liNumIterations)
        {
            giNumIterations = liNumIterations;
        }

        s32 GetMonitorCount()    { return giMonitorCount; }
        s32 GetMaxMonitorCount() { return giMaxMonitorCount; }

        f32 GetMonitorTime(s32 liMonitorHandle)
        {
            return IsValidHandle(liMonitorHandle) ? gpMonitorsArray[liMonitorHandle].mfCurrentValue : 0.0f;
        }

        void GetMonitorData(s32 liMonitorHandle, PerfMonCpuMonitorData* lpData)
        {
            if (!lpData)
                return;

            if (!IsValidHandle(liMonitorHandle))
            {
                lpData->mpcName             = "";
                lpData->mfCurrentValue      = 0.0f;
                lpData->mfAverageValue      = 0.0f;
                lpData->mfMinMaxValue       = 0.0f;
                lpData->mfCpuBudget         = 0.0f;
                lpData->miNumCalls          = 0;
                lpData->miMaxCalls          = 0;
                lpData->mbTraced            = false;
                lpData->mfCurrentTraceValue = 0.0f;
                lpData->mfAverageTraceValue = 0.0f;
                lpData->mfMinMaxTraceValue  = 0.0f;
                return;
            }

            PerfMonCpuInstance& lrInstance = gpMonitorsArray[liMonitorHandle];
            lpData->mpcName             = lrInstance.macName;
            lpData->mfCurrentValue      = lrInstance.mfCurrentValue;
            lpData->mfAverageValue      = lrInstance.mfAverageValue;
            lpData->mfMinMaxValue       = lrInstance.mfMinMaxValue;
            lpData->mfCpuBudget         = lrInstance.mfCpuBudget;
            lpData->miNumCalls          = lrInstance.miNumCalls;
            lpData->miMaxCalls          = lrInstance.miMaxCalls;
            lpData->mbTraced            = lrInstance.mbLibPerfTagged;
            lpData->mfCurrentTraceValue = 0.0f;
            lpData->mfAverageTraceValue = 0.0f;
            lpData->mfMinMaxTraceValue  = 0.0f;
        }

        bool IsMonitorOverBudget(s32 liMonitorHandle)
        {
            if (!IsValidHandle(liMonitorHandle))
                return false;
            PerfMonCpuInstance& lrInstance = gpMonitorsArray[liMonitorHandle];
            return lrInstance.mfCpuBudget > 0.0f && lrInstance.mfCurrentValue > lrInstance.mfCpuBudget;
        }

        PerfMonCpuPage GetMonitorPage(s32 liMonitorHandle)
        {
            return IsValidHandle(liMonitorHandle) ? gpMonitorsArray[liMonitorHandle].mePage : E_PMP_GENERAL;
        }

        namespace
        {
            // PIX (Performance Investigator for Xbox) named-counter sink. The X360 build linked the
            // console PIX SDK (PIXAddNamedCounter pushed a labelled scalar into the PIX timeline);
            // there is no PIX on PC, so this is a no-op shim that preserves the call shape. FLAG:
            // the X360 passed a printf-style label format whose rodata string was not recovered;
            // KAC_PIX_COUNTER_FORMAT below is a documented placeholder, not an X360 fact.
            const char* KAC_PIX_COUNTER_FORMAT = "%s"; // FLAG: placeholder (unrecovered rodata)

            void PIXAddNamedCounter(f32 /*lfValue*/, const char* /*lpcFormat*/, const char* /*lpcName*/)
            {
                // No PIX profiler on the PC target: intentionally empty.
            }
        }

        // X360 0x828172E8. Walk the live registry and emit one PIX named counter per monitor.
        void AddPIXCounters()
        {
            const s32 liCount = giMonitorCount;
            if (!gpMonitorsArray || liCount <= 0)
                return;

            for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
            {
                PerfMonCpuInstance& lrInstance = gpMonitorsArray[liIndex];
                // X360 emits a counter for EVERY monitor up to giMonitorCount. The asm's
                // `addic. r4,r11,0x1C; beq` only computes &macName (entry+0x1C) and sets the
                // condition on that address -- never zero for a real slot, so the branch is
                // never taken (no active/sentinel skip). Emit unconditionally to match.
                PIXAddNamedCounter(lrInstance.mfCurrentValue, KAC_PIX_COUNTER_FORMAT, lrInstance.macName);
            }
        }
    }
}
