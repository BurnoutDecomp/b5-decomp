#pragma once

#include "types.hpp"

// CgsDev::PerfMonCpu - the CPU performance-monitor registry. Each timed region of the frame is
// bracketed by StartMonitor/StopMonitor on an int handle returned by AddMonitor; the registry
// accumulates the elapsed time per monitor, and the debug perfmon component (DebugComponentPerfMonCpu)
// renders the per-monitor bars from it. Reconstructed from the DecFIGS DWARF data model
// (PerfMonCpuInstance / PerfMonCpuMonitorData / PerfMonCpuPage, Development/PerfMon/Cpu/CgsPerfMonCpu.h)
// + the project's QueryPerformanceCounter timer - the X360 read the PPC timebase, which does not port,
// and CgsTimeUtils.cpp already sets the QPC precedent.
//
// INCREMENTAL: the bar overlay needs each monitor's current per-region time + name + budget, so those
// are modelled fully; the running average is a simple cumulative mean, and the min/max, page grouping,
// libperf tracing, and iteration scaling are modelled minimally (the trace/page/libperf surface is the
// perfmon follow-on). PerfMonCpu is modelled as a namespace over file-static singleton state (matching
// the existing call sites PerfMonCpu::StartMonitor; the X360 spells it a struct of all-static members,
// which is semantically identical).

namespace rw { class IResourceAllocator; }

namespace CgsDev
{
    // X360 CgsPerfMonCpu.h:40.
    static const s32 KI_PERFMONCPU_MAXSTRINGLENGTH = 32;

    // The page a monitor is grouped under in the overlay (X360 CgsPerfMonCpu.h:47, E_PMP_GENERAL..23,
    // E_PMP_MAX=24). Only the general page + the count are needed for the bounded bar overlay.
    enum PerfMonCpuPage
    {
        E_PMP_GENERAL = 0,
        E_PMP_MAX     = 24,
    };

    // One registered CPU monitor (X360 CgsPerfMonCpu.h:118). mu64StartValue / mu64Value hold raw timer
    // ticks (the start stamp + the last region's elapsed ticks); mfCurrentValue is that elapsed time in
    // milliseconds - what the overlay draws as a bar.
    struct PerfMonCpuInstance
    {
        u64            mu64StartValue;
        u64            mu64Value;
        s32            miFrameCounter;
        s32            miNumCalls;
        s32            miMaxCalls;
        char           macName[KI_PERFMONCPU_MAXSTRINGLENGTH];
        bool           mbActive;
        bool           mbMinimum;
        bool           mbScaled;
        bool           mbLibPerfTagged;
        PerfMonCpuPage mePage;
        f32            mfCurrentValue;
        f32            mfMinMaxValue;
        f32            mfAverageValue;
        f32            mfAverageAccumulator;
        f32            mfCpuBudget;
        s32            miOrigLibPerfTraceId;
        s32            miLibPerfTraceId;
    };

    // The per-monitor snapshot the overlay / report callback reads (X360 CgsPerfMonCpu.h:86).
    struct PerfMonCpuMonitorData
    {
        const char* mpcName;
        f32         mfCurrentValue;
        f32         mfAverageValue;
        f32         mfMinMaxValue;
        f32         mfCpuBudget;
        s32         miNumCalls;
        s32         miMaxCalls;
        bool        mbTraced;
        f32         mfCurrentTraceValue;
        f32         mfAverageTraceValue;
        f32         mfMinMaxTraceValue;
    };

    namespace PerfMonCpu
    {
        // Allocate the monitor array (X360 Construct(maxCount, allocator)). The allocator is threaded
        // through but unused - the backing comes from the global heap, the same shortcut the debug
        // pools take (CgsDebugCollections.cpp); the faithful rw-allocator path is the allocator follow-on.
        bool Construct(s32 liMaxMonitorCount, rw::IResourceAllocator* lpAllocator);
        void Destruct();

        // Register a monitor and return its handle (a 0-based index into the registry). StartMonitor /
        // StopMonitor / GetMonitorData take that handle. Returns -1 if the registry is full/unbuilt.
        s32  AddMonitor(const char* lpcName, PerfMonCpuPage lePage, bool lbMinimum, f32 lfCpuBudget, bool lbLibPerfTagged);

        void StartMonitor(s32 liMonitorHandle);
        void StopMonitor(s32 liMonitorHandle);

        void SetNumIterationsTaken(s32 liNumIterations);

        s32            GetMonitorCount();
        s32            GetMaxMonitorCount();
        f32            GetMonitorTime(s32 liMonitorHandle);
        void           GetMonitorData(s32 liMonitorHandle, PerfMonCpuMonitorData* lpData);
        bool           IsMonitorOverBudget(s32 liMonitorHandle);
        PerfMonCpuPage GetMonitorPage(s32 liMonitorHandle);
    }
}
