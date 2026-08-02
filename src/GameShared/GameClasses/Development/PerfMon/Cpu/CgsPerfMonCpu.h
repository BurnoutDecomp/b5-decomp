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

// struct, not class: rw::IResourceAllocator's real home (vendor rwcore_structs.h:168) defines it
// as a STRUCT; a class-kind forward decl makes MSVC mangle this header's Construct differently in
// TUs that saw the real header (LNK2019 on the world-module mount when DebugManager began calling
// PerfMonCpu::Construct).
namespace rw { struct IResourceAllocator; }

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

        // ⚠️⚠️ THE 6-PARAMETER FORM IS A HEX-RAYS ARTEFACT, NOT A SECOND CONSOLE OVERLOAD.
        // SETTLED 2026-08-03 (VehicleManager layout wave). On the PPC ABI a float/double argument
        // consumes an integer-register SLOT as well as an FP register, so the real 5-argument call
        // `AddMonitor(name, page, min, budget, tag)` is emitted as `r3, r4, r5, f1, r7` -- **r6 is
        // never written**. Hex-Rays sees the hole and invents a 6th argument for it. Read directly
        // out of VehicleManager::Construct @0x8263B7C8, which issues thirty of these:
        //     lis/addi r3, <name> ; li r4, 0xC ; li r5, 0 ; fmr f1, f22 ; li r7, 1 ; bl AddMonitor
        // -- name, page 12, min 0, budget flt_82004A20, tag 1. No r6 anywhere in the function.
        // AddMonitor's own body @0x82824C30 reads r3/r4/r5/f1/r7 and never touches r6.
        //
        // ⇒ THE 5-PARAMETER FORM ABOVE IS THE CONSOLE'S SIGNATURE. New reconstructions -- notably
        // the ~55 call sites the PhysicsSimulationModule / VehicleManager constructor waves will
        // add -- must call THAT one, mapping the Hex-Rays 6-tuple as
        // (name, colour->page, minimum, budget, flags->tag) and dropping the phantom parent handle.
        //
        // ⚠️ It is NOT removed, and it is NOT undefined: a forwarder in WorldLinkStubs.cpp defines
        // it (`?AddMonitor@PerfMonCpu@CgsDev@@YAHPEBDHHNHH@Z`, present in Burnout_PC.map) and it is
        // REACHED ~27x at boot from the already-mounted CgsSceneManagerModule.cpp and
        // CgsNetworkImageConverter.cpp, whose committed call sites were transcribed in the 6-arg
        // shape. Deleting the declaration would break two mounted TUs for no gain. Retire it when
        // those call sites are re-transcribed to the 5-argument form.
        //
        // ⚠️ ODR HAZARD: GameSource/Effects/Particles/LionPerfMon.cpp re-declares this as a STATIC
        // MEMBER of a `class PerfMonCpu` rather than a free function in `namespace PerfMonCpu`.
        // Those mangle differently (`SA...` vs `YA...`), so its 22 calls can never bind to the
        // forwarder. That TU is not mounted; mounting it without fixing the declaration is an
        // instant LNK2019 x22.
        s32  AddMonitor(const char* lpcName, s32 liColour, s32 liMinimum, double lfCpuBudget, s32 liParentHandle, s32 liFlags);

        void StartMonitor(s32 liMonitorHandle);
        void StopMonitor(s32 liMonitorHandle);

        void SetNumIterationsTaken(s32 liNumIterations);

        s32            GetMonitorCount();
        s32            GetMaxMonitorCount();
        f32            GetMonitorTime(s32 liMonitorHandle);
        void           GetMonitorData(s32 liMonitorHandle, PerfMonCpuMonitorData* lpData);
        bool           IsMonitorOverBudget(s32 liMonitorHandle);
        PerfMonCpuPage GetMonitorPage(s32 liMonitorHandle);

        // X360 0x828172E8. Register every live monitor as a PIX named counter so the X360 PIX
        // profiler graphs the per-region time alongside the engine's own overlay. The X360 walks
        // the registry (giMonitorCount entries of stride sizeof(PerfMonCpuInstance)) and, for each
        // valid monitor, hands PIX the monitor's current millisecond value (mfCurrentValue) and its
        // name (macName). PIX is an X360-only SDK (the PPC PIXAddNamedCounter), so on PC this routes
        // through a no-op shim; the per-counter format string the X360 passed lived in rodata that
        // did not survive (FLAG: format string is a documented placeholder, not a recovered fact).
        void AddPIXCounters();
    }
}
