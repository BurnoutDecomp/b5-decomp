#ifndef BRN_APT_RUNTIME_BRINGUP_H
#define BRN_APT_RUNTIME_BRINGUP_H

#include "types.hpp"

namespace CgsGraphics { struct Im2d; }   // the proven immediate-mode 2D renderer (the Apt render path)

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

    // ---- per-frame tick -------------------------------------------------------
    // If a movie is loaded, advance its timeline + run its ActionScript (tick). The
    // RENDER moved to AptRuntimeRender (the proven immediate-mode path); this only ticks.
    // Called from GuiModule::Update.
    void AptRuntimeUpdate();

    // ---- per-frame render to D3D9 (the PROVEN immediate-mode path) -------------
    // Draw the loaded movie's geometry directly through the game's working immediate-mode
    // 2D renderer (CgsGraphics::Im2d -- the SAME one the loading screen + debug HUD draw
    // through, which owns a battle-tested per-frame DrawPrimitiveUP submission). This
    // REPLACES the never-exercised raw ImRenderBuffer<V> Clear/BeginRendering/Swap/Dispatch
    // path (which AV'd on first use). Called from the renderer hook (BrnRendererModule::
    // Render) each frame with that module's mIm2dRenderer. Null/unresolved -> clean no-op.
    void AptRuntimeRender(CgsGraphics::Im2d* lpIm2d);

    // True once AptRuntimeBringUp() has fully succeeded (the engine host is live).
    bool AptRuntimeIsReady();

    // True once the title movie is loaded + instantiated (root CIH live, ticking).
    bool AptRuntimeIsMovieLive();

    // True once the movie has COMPOSED (the first paced tick ran frame-0's place
    // commands; the PLACE-named clips exist on the root display list) -- the host
    // equivalent of the GuiCache apt-component init handshake
    // (AreAllAptComponentsInitialised gates E_STAGE_FADE_IN on it).
    bool AptRuntimeIsMovieComposed();

    // ---- component view-state bridge (PC bring-up shim; FLAG) ------------------
    // The faithful path is GuiComponent::FillAptViewMessage -> AptAux::
    // UpdateFlashComponent -> AptCommunicator key-values -> the movie's AS
    // ("UpdateAll") applying the transition. That communicator glue is not wired
    // yet, so this shim reproduces the OBSERVABLE effect directly: find the placed
    // child clip whose PLACE instance name is lpacInstName (on the root movie's
    // display list) and gotoAndPlay its "<viewState>" frame label ("transin"/
    // "transout"/"visible"/"invisible"...). Returns true when a clip+label matched.
    bool AptRuntimeSetComponentViewState(const char* lpacInstName, const char* lpacViewState);

    // The faithful KEY dispatch of the component protocol (AddOutputAptViewState's
    // (key, value) pair): apt_Transition -> the paired-clip transition above;
    // apt_state -> gotoAndPlay(value) on the component's own clip (the B5MenuItem
    // Selected/Unselected/Disabled/Invisible state labels); apt_labeltxt -> set the
    // clip's nested 'label' dynamic-text field (SetTextValue + invalidate; the text
    // pipeline localises '$KEY' strings); apt_updatestate -> no-op trigger.
    bool AptRuntimeSetComponentKeyValue(const char* lpacInstName, const char* lpacKey,
                                        const char* lpacValue);
}

#endif
