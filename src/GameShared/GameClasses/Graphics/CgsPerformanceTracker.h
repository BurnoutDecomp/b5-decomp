#pragma once

#include "types.hpp"

// CgsPerformanceTracker.h - the graphics performance counters (DWARF struct
// CgsGraphics::PerformanceTracker, CgsPerformanceTracker.h:30): a struct of static members only.
//
// X360 attestation gate: ARTIST keeps only the four lifecycle hooks, each an out-of-line call to
// the shared empty body 0x8284CB38 (a lone blr the linker ICF-folded). The DecFIGS PS3 build calls
// the same four from the same positions through its TOC stubs, and their PS3 bodies are empty too
// (0xAAF2A8 Startup, 0xAAF2AC Shutdown, 0xAAF2CC BeginFrame, 0xAAF2D0 EndFrame):
//   Startup     - BrnRendererModule::Construct     (ARTIST bl at 0x8240A7A0; PS3 stub 0x12250)
//   Shutdown    - BrnRendererModule::Destruct      (ARTIST bl at 0x82408AE4; PS3 stub 0x10820)
//   BeginFrame  - BrnRendererModule::StartOfFrame  (ARTIST bl at 0x823FC2A4; PS3 stub 0x11720)
//   EndFrame    - BrnRendererModule::SwapBuffers   (ARTIST bl at 0x823FC6B0; PS3 stub 0x11E50)
// The counter API (GetCounter, AdjustCounter, TraceSummary, ...) has no X360 ledger row and stays out.
namespace CgsGraphics
{
    struct PerformanceTracker
    {
        static void Startup();    // CgsPerformanceTracker.h:124
        static void Shutdown();   // CgsPerformanceTracker.h:125

        static void BeginFrame(); // CgsPerformanceTracker.h:127
        static void EndFrame();   // CgsPerformanceTracker.h:128
    };
}
