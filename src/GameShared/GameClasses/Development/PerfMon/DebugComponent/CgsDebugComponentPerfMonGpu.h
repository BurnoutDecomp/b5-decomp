#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"                                                       // Vector2 (rw::math::vpu::Vector2)
#include "GameShared/GameClasses/Development/DebugSystem/Core/CgsDebugComponent.h" // DebugComponent (base)
#include "GameShared/GameClasses/Development/PerfMon/Gpu/CgsPerfMonGpu.h"          // PerfMonGpuMonitorData, PerfMonGpu

// CgsDev::DebugComponentPerfMonGpu - the on-screen GPU performance overlay: the debug component whose
// RenderHUD draws either a table (RenderPerformanceTable: one labelled cost row per registered GPU
// monitor) or a graph (RenderPerformanceGraph: the monitors plotted as stacked bars over a %-budget
// scale). The per-monitor rows/bars are emitted by ReportMonitors walking the PerfMonGpu registry and
// calling back into RenderTableCallback / RenderGraphCallback. Recovered from the DecFIGS DWARF
// (Development/PerfMon/DebugComponent/CgsDebugComponentPerfMonGpu.h) + the Feb-2007 partial header.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   GetName @ 0x828177F0 -> "GPU Monitors"; GetPath @ 0x82817800 -> "Core/Debug/Performance".
//
// Derives from DebugComponent (vptr@+0, base size 0xC), which places the members below at their
// X360-attested offsets: mbVisible @+0xC, mbDisplayAsGraph @+0xD, mfMaxGpu @+0x10 (verified in
// OnActivate @0x8282F488, RenderHUD @0x8282F520, and the mfMaxGpu clamp in RenderPerformanceGraph
// @0x8282CEA0 which loads/stores *(this+0x10)).
//
// The overlay's layout metrics (row/text height, first draw origin) come from the debug UI's screen
// Metrics (X360: mpInstance->mpUI(+0x140)->Metrics @ +0x5C/+0x70/+0x74) via DebugManager::GetInstance().

namespace CgsDev
{
    struct Debug2DImmediateRender;

    class DebugComponentPerfMonGpu : public DebugComponent
    {
    public:
        virtual void RenderHUD( Debug2DImmediateRender* lpDebug2DRender );   // @0x8282F520

    protected:
        virtual const char* GetName() const { return "GPU Monitors"; }
        virtual const char* GetPath() const { return "Core/Debug/Performance"; }
        virtual bool        IsSimple() const { return false; }
        virtual void        OnActivate();                                    // @0x8282F488

        void RenderPerformanceTable( Debug2DImmediateRender* lpDebug2DRender );   // @0x8282CDF0
        void RenderPerformanceGraph( Debug2DImmediateRender* lpDebug2DRender );   // @0x8282CEA0

        // Per-monitor report callbacks. STATIC: passed to PerfMonGpu::ReportMonitors as plain
        // FPerfMonGpuReportCallback*; the owning component is threaded via RenderCallbackParameter.
        static void RenderTableCallback( const PerfMonGpuMonitorData& lrData, void* lpUserData );   // @0x82826FC8
        static void RenderGraphCallback( const PerfMonGpuMonitorData& lrData, void* lpUserData );   // @0x8281F250

        // User-data threaded through PerfMonGpu::ReportMonitors into the per-monitor callbacks: the
        // owning component, the target renderer, and the running draw cursor / accumulated total.
        // (DWARF CgsDebugComponentPerfMonGpu.h:96.)
        class RenderCallbackParameter
        {
        public:
            DebugComponentPerfMonGpu* mpThis;    // +0x00
            Debug2DImmediateRender*   mpRender;  // +0x04
            f32                       mfDrawX;   // +0x08
            f32                       mfDrawY;   // +0x0C
            f32                       mfTotalGpu;// +0x10
        };

        bool mbVisible;         // +0x0C  (registered "Draw GPU monitors")
        bool mbDisplayAsGraph;  // +0x0D  (registered "Draw As Graph")
        f32  mfMaxGpu;          // +0x10  (graph %-budget auto-scale, clamped [floor,200])
    };
}
