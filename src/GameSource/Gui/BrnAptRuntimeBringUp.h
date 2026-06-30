#ifndef BRN_APT_RUNTIME_BRINGUP_H
#define BRN_APT_RUNTIME_BRINGUP_H

#include "types.hpp"

// =============================================================================
// BrnAptRuntimeBringUp -- the host bring-up + per-frame driver for the EATech Apt
// (ActionScript movie) runtime, wired into the ACTIVE BrnGui::GuiModule bridge so
// BootLegal's PlayAptMovie("Title_Screen02") (channel 41) is actually consumed and
// the engine is given the chance to load + tick + render that movie.
//
// CONTEXT: the Apt ENGINE links clean (0 unresolved) but its ORCHESTRATION layer
// (the X360 AptInit / AptAllocatorInitialize / AptUpdateInitialize / AptRender-
// Initialize / AptCreateTargetInstance / AptUpdate facade) is NOT reconstructed --
// those top-level routines exist only as comments. So THIS file IS that facade:
// it stands up the pieces that DO exist (the DOGMA + AptValueGC allocators, the
// AptActionInterpreter, AptAux::Construct, the AptTarget context) and drives them
// each frame, DEFENSIVELY -- every step null-checks, logs a [AptRT] probe, and
// bails cleanly (no crash) the instant it crosses an un-homed engine piece, so a
// single run-log reveals exactly how far the render path gets.
//
// All of the deeper engine plumbing (the async .apt streamer, AptCreateTarget-
// Instance, the render-tree walk that drives gAptFuncs.pfnDrawRenderingUnit) is
// un-homed; the matching steps below are // FLAG'd and bail. Reaching "the engine
// attempted to render N units into the flushed buffer" is the goal -- not a
// perfect-but-crashing path.
// =============================================================================

namespace BrnGui
{
    // ---- one-time runtime bring-up (idempotent) --------------------------------
    // Stand up the Apt allocator + interpreter + host callback table + render
    // handler (AptAux) + the per-thread AptTarget context. Returns true when the
    // bring-up reached a state where channel-41 routing is worth attempting.
    // Logs [AptRT] probes throughout. Safe to call every frame (no-op after the
    // first success). Called from GuiModule::Prepare / first Update.
    bool AptRuntimeBringUp();

    // ---- channel-41 consume: load Title_Screen02 -------------------------------
    // Handle a GuiEventPlayAptMovie (channel 41, type 18): record the movie name and
    // attempt to load it from GUIAPT\<NAME>.bundle through the homed Apt loader.
    // Defensive: bails (logs + returns) wherever the load path crosses an un-homed
    // piece. lpacMovieName is the movie name BootLegal posted ("Title_Screen02").
    void AptRuntimePlayMovie(const char* lpacMovieName, s32 liLevelNum);

    // ---- per-frame tick + render ----------------------------------------------
    // If a movie is loaded, advance its timeline + run its ActionScript (tick) and
    // drive the engine render dispatch so geometry fills the Im2dRenderBuffer.
    // Called from GuiModule::Update.
    void AptRuntimeUpdate();

    // ---- per-frame flush to D3D9 ----------------------------------------------
    // Flush the Apt render buffer (the one AptRenderHandler::Render fills) to the
    // screen via ImRenderBuffer<V>::Dispatch(). Called from the renderer hook
    // (BrnRendererModule::Render) each frame, alongside gpActiveMovieManager. A
    // null/empty buffer is a clean no-op.
    void AptRuntimeFlush();

    // True once AptRuntimeBringUp() has fully succeeded (the engine host is live).
    bool AptRuntimeIsReady();
}

#endif
