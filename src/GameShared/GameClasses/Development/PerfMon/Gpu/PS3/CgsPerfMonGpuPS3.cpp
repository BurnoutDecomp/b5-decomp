// D3D9 needs USER32 types before RenderWare's kernel-only Windows include.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "pc/gcm/renderengine/GpuMonitorTiming.h"
#ifdef AddMonitor
#undef AddMonitor
#endif

#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT + CgsDev::Assert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h" // StrStream (runtime assert messages)

#include <cstddef>     // offsetof
#include <cstring>     // std::memset

// winspool.h (pulled in by Windows.h) macro-defines AddMonitor -> AddMonitorA, which would rename the
// class's registry entry point declared in CgsPerfMonGpu.h. We do not reference AddMonitor here, but
// drop the macro for hygiene (matching the Cpu sibling CgsPerfMonCpu.cpp).
#ifdef AddMonitor
#undef AddMonitor
#endif

// ARTIST CgsPerfMonGpuX360.cpp: the ledger retains the PS3 pathname.
// Registry, assertions, monitor state and refresh-percent conversion are the
// original engine code. D3D9 GPU timestamps replace X360 predicated callbacks;
// a completed packet is published at the original update-side Swap boundary.
// Pending/invalid packets retain the last complete sample, never CPU timings.

#include "pc/gcm/renderengine/device.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"
#include <mutex>
#include <cstdlib>
#include <cstdio>

namespace CgsDev
{
    namespace
    {
        // The GPU timestamp ring: the X360 ARTIST region [0x83018270, +0x0A78) memset in Construct.
        // KI_NUMBUFFERS is 5 on ARTIST - Swap advances the read index modulo 5 and the tick region is
        // 330 u64 = 5 * 66; the DecFIGS PS3 header spells KI_NUMBUFFERS=4 (a platform/merge-window delta,
        // the X360 asm arbitrates). KI_TIMERSPERBUFFER = 2*(KI_PERFMONGPUMAXMONITORS+1) = 66: one slot per
        // monitor (including the whole-frame "rendered total" slot 32) plus the parallel start-stamp half
        // the GPU callback keys against.
        const s32 KI_NUMBUFFERS       = 5;
        const s32 KI_TIMERSPERBUFFER  = 2 * (KI_PERFMONGPUMAXMONITORS + 1);

        struct PerfMonGpuBuffer
        {
            u64  mau64Timers[KI_NUMBUFFERS][KI_TIMERSPERBUFFER]; // +0x0000 GPU-latched tick costs, per read buffer
            bool mabUsedThisFrame[KI_PERFMONGPUMAXMONITORS];     // +0x0A50 monitor touched since StartProfiling
            s32  miCurrentReadBuffer;                            // +0x0A70 the buffer the CPU reads (lags the GPU)
            s32  miCurrentWriteBuffer;                           // +0x0A74 the buffer the GPU latches into
        };

        // Pin the byte layout to the X360 ARTIST globals (Construct memsets exactly 0x0A78 bytes; the
        // read/write indices sit at the tail of that region, the tick->percent factor sits just past it).
        static_assert(sizeof(PerfMonGpuBuffer) == 0x0A78,
                      "PerfMonGpuBuffer must match the X360 gValues region (2680 bytes)");
        static_assert(offsetof(PerfMonGpuBuffer, mabUsedThisFrame) == 0x0A50,
                      "mabUsedThisFrame must sit at +0x0A50");
        static_assert(offsetof(PerfMonGpuBuffer, miCurrentReadBuffer) == 0x0A70,
                      "miCurrentReadBuffer must sit at +0x0A70");
        // The X360 monitor-name records (unk_83019288) are stride 56 - the exact sizeof of the class's
        // PerfMonGpuInstance (macName[32] + RGBA + u64 + f32, 8-aligned). Pins the header to the asm.
        static_assert(sizeof(PerfMonGpuInstance) == 56,
                      "PerfMonGpuInstance must match the X360 monitor-record stride (56 bytes)");

        PerfMonGpuBuffer gBuffer;                 // X360 gValues @ 0x83018270
        f32              gfCyclesToPercent = 0.0f; // X360 flt_83018CE8: 100.0 / QPF, ticks -> percent-of-second

        // FLAG PC-platform leaf: bridge completed GPU-query packets across the
        // joined render/update boundary. The original callback828259B8 clears
        // 33 costs on frame begin, stores begins in slots33..65, and accumulates
        // end-begin in slots0..32. Native packets preserve those slot meanings.
        std::mutex gNativeSampleMutex;
        renderengine::GpuMonitorTimingPC::Result gNativeSample;
        u64 guConsumedNativeFrame = 0;

        void PublishNativeGpuSample(const renderengine::GpuMonitorTimingPC::Result& lrResult)
        {
            using namespace renderengine::GpuMonitorTimingPC;
            {
                std::lock_guard<std::mutex> lLock(gNativeSampleMutex);
                if (lrResult.status == Reset ||
                    (lrResult.status == Valid && lrResult.frame > gNativeSample.frame))
                    gNativeSample = lrResult;
            }
            // FLAG PC-platform leaf: opt-in witnesses for the real query path.
            static const bool sbTrace = std::getenv("BRN_GPU_MONITOR_TRACE") != nullptr;
            static u32 suValidTraces = 0, suFailureTraces = 0;
            if (!sbTrace || !CgsDev::Log::gpDebugPrint) return;
            if (lrResult.status == Valid && (suValidTraces++ < 8 || lrResult.frame % 120 == 0))
            {
                CgsDev::Log::gpDebugPrint->AppendFormat(
                    "[gpu-monitors] frame=%llu status=valid frequency=%llu used=0x%08X totalMs=%.6f",
                    static_cast<unsigned long long>(lrResult.frame),
                    static_cast<unsigned long long>(lrResult.frequency), lrResult.used,
                    double(lrResult.ticks[FrameTimer]) * 1000.0 / double(lrResult.frequency));
                for (unsigned i = 0; i < MaxMonitors; ++i)
                    if (lrResult.used & (u32(1) << i))
                        CgsDev::Log::gpDebugPrint->AppendFormat(" id%u=%.6f", i,
                            double(lrResult.ticks[i]) * 1000.0 / double(lrResult.frequency));
                *CgsDev::Log::gpDebugPrint << "\n";
            }
            else if (lrResult.status != Pending && lrResult.status != Valid && suFailureTraces++ < 8)
                CgsDev::Log::gpDebugPrint->AppendFormat("[gpu-monitors] frame=%llu status=%d\n",
                    static_cast<unsigned long long>(lrResult.frame), int(lrResult.status));
        }

        enum PerfMonGpuTimerOp
        {
            E_PMGTO_BEGINFRAME = 0,
            E_PMGTO_ENDFRAME = 1,
            E_PMGTO_STARTMONITOR = 2,
            E_PMGTO_STOPMONITOR = 3,
        };

        // FLAG PC-platform leaf: the original op/id command contexts feed
        // timestamp query markers at the same points in the D3D9 command stream.
        void InsertGpuTimingMarker(PerfMonGpuTimerOp leOp, s32 liMonitorId)
        {
            using namespace renderengine::GpuMonitorTimingPC;
            switch (leOp)
            {
            case E_PMGTO_BEGINFRAME: Begin(renderengine::gDevice, PublishNativeGpuSample); break;
            case E_PMGTO_ENDFRAME: gRecorder.End(); break;
            case E_PMGTO_STARTMONITOR: gRecorder.Start(static_cast<unsigned>(liMonitorId)); break;
            case E_PMGTO_STOPMONITOR: gRecorder.Stop(static_cast<unsigned>(liMonitorId)); break;
            }
        }

        // Build a runtime assert message ("<prefix><monitor name>\n") and fire it through the assert
        // front-end, reproducing the X360 StrStream-into-the-message-buffer path (StartMonitor 0x153,
        // StopMonitor 0x180). The name comes from the class-private monitor record, so the caller resolves
        // it and passes it in.
        void FirePerfMonGpuAssert(const char* lpcPrefix, const char* lpcName,
                                  const char* lpcFile, s32 liLine)
        {
            CgsDev::Assert::BeginAssert();
            char macMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            macMessage[0] = '\0';
            StrStream lStrStream(macMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE);
            lStrStream << lpcPrefix;
            lStrStream << (lpcName ? lpcName : "<NULLSTRING>");
            lStrStream << "\n";
            CgsDev::Assert::FireAssert(macMessage, lpcFile, liLine);
            CgsDev::Assert::EndAssert();
        }
    }

    bool PerfMonGpu::Construct()
    {
        renderengine::GpuMonitorTimingPC::DeviceReset();
        guConsumedNativeFrame = 0;
        miMaxMonitor       = 0;
        meGameFrequency    = E_PMF_60HZ;
        mbActiveMonitor    = false;
        mbProfilingRunning = false;
        miActiveMonitorID  = 0;

        std::memset(maMonitors, 0, sizeof(maMonitors));
        std::memset(&gBuffer, 0, sizeof(gBuffer));
        gBuffer.miCurrentReadBuffer  = 2;
        gBuffer.miCurrentWriteBuffer = 0;

        LARGE_INTEGER lFrequency;
        CGS_ASSERT(QueryPerformanceFrequency(&lFrequency),
                   "QueryPerformanceFrequency(&lFrequency)");
        gfCyclesToPercent = static_cast<f32>(100.0 / static_cast<double>(lFrequency.QuadPart));
        return true;
    }

    void PerfMonGpu::Swap()
    {
        CGS_ASSERT(!mbProfilingRunning, "mbProfilingRunning == false");
        const s32 liPreviousRead = gBuffer.miCurrentReadBuffer;
        gBuffer.miCurrentReadBuffer = (liPreviousRead + 1) % KI_NUMBUFFERS;
        // FLAG PC-platform leaf: D3D9 completion is asynchronous. Retain the
        // previous complete snapshot when a newer packet is still pending;
        // do not expose an incomplete ring slot as a zero-cost GPU frame.
        std::memcpy(gBuffer.mau64Timers[gBuffer.miCurrentReadBuffer],
                    gBuffer.mau64Timers[liPreviousRead], sizeof(gBuffer.mau64Timers[0]));
        std::lock_guard<std::mutex> lLock(gNativeSampleMutex);
        if (gNativeSample.status == renderengine::GpuMonitorTimingPC::Reset)
        {
            std::memset(gBuffer.mau64Timers, 0, sizeof(gBuffer.mau64Timers));
            guConsumedNativeFrame = 0;
            gNativeSample.status = renderengine::GpuMonitorTimingPC::Pending;
        }
        else if (gNativeSample.status == renderengine::GpuMonitorTimingPC::Valid
            && gNativeSample.frame > guConsumedNativeFrame)
        {
            gBuffer.miCurrentWriteBuffer = gBuffer.miCurrentReadBuffer;
            std::memcpy(gBuffer.mau64Timers[gBuffer.miCurrentReadBuffer],
                        gNativeSample.ticks, sizeof(gNativeSample.ticks));
            // The GPU's frequency belongs to these timestamps. QPF is only
            // the original initial factor, before the first complete sample.
            gfCyclesToPercent = static_cast<f32>(100.0 / double(gNativeSample.frequency));
            guConsumedNativeFrame = gNativeSample.frame;
        }
    }

    void PerfMonGpu::StartProfiling()
    {
        for (s32 liIndex = 0; liIndex < KI_PERFMONGPUMAXMONITORS; ++liIndex)
            gBuffer.mabUsedThisFrame[liIndex] = false;

        InsertGpuTimingMarker(E_PMGTO_BEGINFRAME, 0);
        mbProfilingRunning = true;
    }

    void PerfMonGpu::StopProfiling()
    {
        InsertGpuTimingMarker(E_PMGTO_ENDFRAME, 0);
        mbProfilingRunning = false;
    }

    void PerfMonGpu::StartMonitor(s32 liMonitorId)
    {
        CGS_ASSERT(liMonitorId >= 0, "liMonitorID >= 0");
        CGS_ASSERT(liMonitorId < KI_PERFMONGPUMAXMONITORS,
                   "liMonitorID < KI_PERFMONGPUMAXMONITORS");
        if (mbActiveMonitor)
            FirePerfMonGpuAssert("Monitor is already started: ",
                                 maMonitors[miActiveMonitorID].macName, __FILE__, __LINE__);
        CGS_ASSERT(!gBuffer.mabUsedThisFrame[liMonitorId],
                   "gValues.mabUsedThisFrame[liMonitorID] == false");
        CGS_ASSERT(mbProfilingRunning, "mbProfilingRunning == true");

        miActiveMonitorID = liMonitorId;
        mbActiveMonitor   = true;
        gBuffer.mabUsedThisFrame[liMonitorId] = true;
        InsertGpuTimingMarker(E_PMGTO_STARTMONITOR, liMonitorId);
    }

    void PerfMonGpu::StopMonitor(s32 liMonitorId)
    {
        CGS_ASSERT(liMonitorId >= 0, "liMonitorID >= 0");
        CGS_ASSERT(liMonitorId < KI_PERFMONGPUMAXMONITORS,
                   "liMonitorID < KI_PERFMONGPUMAXMONITORS");
        if (!mbActiveMonitor)
            FirePerfMonGpuAssert("Monitor is not started: ",
                                 maMonitors[liMonitorId].macName, __FILE__, __LINE__);
        CGS_ASSERT(mbProfilingRunning, "mbProfilingRunning == true");

        mbActiveMonitor = false;
        InsertGpuTimingMarker(E_PMGTO_STOPMONITOR, liMonitorId);
    }

    f32 PerfMonGpu::ComputeValueFromTimer(s32 liMonitorId)
    {
        f32 lfRefreshRate;
        switch (meGameFrequency)
        {
        case E_PMF_50HZ:
            lfRefreshRate = 50.0f;
            break;
        case E_PMF_30HZ:
            lfRefreshRate = 29.97f;
            break;
        case E_PMF_25HZ:
            lfRefreshRate = 25.0f;
            break;
        case E_PMF_60HZ:
            lfRefreshRate = 59.94f;
            break;
        default:
            CGS_ASSERT(false, "invalid FPS, assuming 60hz");
            lfRefreshRate = 59.94f;
            break;
        }

        const u64 lu64Cost = gBuffer.mau64Timers[gBuffer.miCurrentReadBuffer][liMonitorId];
        return static_cast<f32>(static_cast<double>(lu64Cost)) * gfCyclesToPercent * lfRefreshRate;
    }
}
