#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT + CgsDev::Assert front-end
#include "GameShared/GameClasses/Development/CgsStrStream.h" // StrStream (runtime assert messages)

#include <Windows.h>   // QueryPerformanceFrequency / LARGE_INTEGER: the X360 read the PPC timebase,
                       // which does not port; QPC is the PC precedent (CgsTimeUtils.cpp / the Cpu perfmon).
#include <cstddef>     // offsetof
#include <cstring>     // std::memset

// winspool.h (pulled in by Windows.h) macro-defines AddMonitor -> AddMonitorA, which would rename the
// class's registry entry point declared in CgsPerfMonGpu.h. We do not reference AddMonitor here, but
// drop the macro for hygiene (matching the Cpu sibling CgsPerfMonCpu.cpp).
#ifdef AddMonitor
#undef AddMonitor
#endif

// CgsDev::PerfMonGpu - the GPU-side perf-monitor bodies (X360 CgsPerfMonGpuX360.cpp; the ledger path
// carries the PS3 name, but the ARTIST image compiled the X360 variant - the assert file strings say so).
// The frame's render spine brackets each GPU region with StartMonitor/StopMonitor and the whole frame
// with StartProfiling/StopProfiling; ComputeValueFromTimer converts a monitor's recorded GPU tick-cost
// into a percentage-of-refresh value the overlay draws (ReportMonitors / GetLast* in CgsPerfMonGpu.cpp).
//
// The X360 timed the GPU by hardware PREDICATION: SetPredication + a command-buffer callback
// (D3DDevice::InsertCallback) latched the GPU's timestamp into a multi-buffered ring as the GPU reached
// that marker; the CPU reads a buffer that lags a couple of frames (miCurrentReadBuffer starts at 2,
// Swap advances it). Predicated GPU timing is an X360-D3D mechanism with no PC-D3D11 equivalent - the
// same platform-leaf situation as the Cpu perfmon's PIX counters - so on the PC target the bracket is a
// documented no-op that preserves the exact call shape and context values from the ARTIST asm; the ring
// buffer is filled by the real GPU on console. The GPU-side timestamp callback (sub_828259B8) did not
// survive as a named function and is never invoked by the PC shim, so it is an empty documented
// placeholder rather than a guessed body.

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

        // The context word the X360 packed for the timing callback: op in the high 16 bits, monitor id in
        // the low 16 (0 for the frame-level begin/end). Names inferred from the call sites; the numeric
        // codes are from the ARTIST asm.
        enum PerfMonGpuTimerOp
        {
            E_PMGTO_BEGINFRAME   = 0,
            E_PMGTO_ENDFRAME     = 1,
            E_PMGTO_STARTMONITOR = 2,
            E_PMGTO_STOPMONITOR  = 3,
        };

        // The X360 GPU-command-buffer callback (sub_828259B8): on console the GPU invokes this as it
        // reaches the inserted marker and latches its current timestamp into gBuffer keyed by the context.
        // Its body did not survive as a named function and the PC shim never invokes it.
        // FLAG PC-platform leaf: unrecovered X360 GPU-timestamp callback; no-op on the PC target.
        void PerfMonGpuTimingCallback(u32 /*luContext*/)
        {
        }

        // The X360 timing bracket: SetPredication(0x3FFFFFFF); InsertCallback(0, sub_828259B8, context);
        // SetPredication(0). Predicated GPU timing has no PC-D3D11 equivalent, so this is a no-op that
        // preserves the call shape, the packed context, and the callback registration from the asm.
        // FLAG PC-platform leaf: X360 D3DDevice predicated-timing callbacks.
        void InsertGpuTimingMarker(PerfMonGpuTimerOp leOp, s32 liMonitorId)
        {
            const u32 luContext = (static_cast<u32>(leOp) << 16) |
                                  (static_cast<u32>(liMonitorId) & 0xFFFF);
            // Register the GPU callback with the packed context, exactly as the ARTIST asm hands it to
            // D3DDevice_InsertCallback; the PC target has no GPU command buffer to run it against.
            void (*lpfCallback)(u32) = &PerfMonGpuTimingCallback;
            (void)lpfCallback;
            (void)luContext;
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
        gBuffer.miCurrentReadBuffer = (gBuffer.miCurrentReadBuffer + 1) % KI_NUMBUFFERS;
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
