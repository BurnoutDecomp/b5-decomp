#ifndef BRN_GUI_APT_RUNTIME_H
#define BRN_GUI_APT_RUNTIME_H

#include "types.hpp"






namespace CgsGui { class ViewModule; struct AptIm2dRenderBuffer; struct GuiEventLoadNotification; }

namespace BrnGui
{
    // Gui-owned Apt movie host.
    //
    // This is the public ownership boundary used by GuiModule and the renderer. The
    // remaining Apt load/tick/render implementation is private to BrnGuiAptRuntime.cpp
    // until AptDataHandler/ViewModule/resource ownership can be split into the final
    // reconstructed homes.
    class AptRuntimeHost
    {
    public:
        bool Prepare(CgsGui::ViewModule* lpViewModule);
        bool Prepare();
        void PlayMovie(const char* lpacMovieName, s32 liLevelNum);
        // The remaining shim-side per-frame drive (the title help-item defaults
        // retry). The movie TICK ownership moved to the real chain --
        // GuiModule::Update -> CgsGui::ViewModule::Update -> AptAux::Update ->
        // AptUpdateTarget (the engine frame pacer). Deleted with the component
        // shim.
        void UpdateShimResidue();
        void StopMovie();

        // ---- PC-minimal render wiring residue --------------------------------------
        // The movie RENDER ownership moved to the real chain -- GuiModule::Render ->
        // CgsGui::ViewModule::Render @0x82858810 -> RenderInternal @0x82858AF8 ->
        // AptAux::Render -> AptRenderTarget (the engine render walk). What remains
        // host-side is the WIRING the console gets from its renderer/IO chain:
        //   * the Apt Im2d command buffer the engine's render callbacks fill (owned by
        //     the bring-up until the GUI resource slice lands) -- GuiModule::Render
        //     publishes it into the view input buffer's renderer set each frame;
        //   * the non-null 3D-slot stand-in that keeps AptRenderHandler::Render's
        //     `mp3dRenderer != 0` assert quiet (the boot/title movies are 2D-only;
        //     becomes the real Im3d buffer when that slice lands);
        //   * the PC-platform dispatch leaf: freeze + flush the filled command buffer
        //     to D3D9 (Swap -> Clear -> Dispatch) after the view render returns -- the
        //     console's equivalent consumption is the render thread draining the
        //     buffers through the custom-renderer-manager bracket.
        CgsGui::AptIm2dRenderBuffer* GetAptRenderBuffer() const;
        void* Get3dRendererAssertSatisfier() const;
        void DispatchRenderResidue();

        // ---- the load-notification drain (the GuiResourceModule output-buffer stand-in) ----
        // Every bundle the host's [PC IO] leaf loads queues one GuiEventLoadNotification per
        // carried resource; GuiModule's frame bridge pops them here and posts each as a view
        // event (14), so the REAL CgsGui::ViewModule::ProcessIncomingLoadNotification
        // @0x8285BD30 performs every registration (AddAptData / LoadStringTable / AddFont).
        // Returns false when the ring is empty.
        bool PopPendingLoadNotification(CgsGui::GuiEventLoadNotification* lpOut);

        bool IsReady() const;
        bool IsMovieLive() const;
        bool IsMovieComposed() const;

        // RETIRED (2026-07-09, step 6): SetComponentViewState / SetComponentKeyValue --
        // the REAL component framework drives the clips (AddNewAptComponent +
        // UpdateComponent/UpdateAllComponents -> the movie AS).

    private:
        CgsGui::ViewModule* mpViewModule = nullptr;
    };

    // Interim renderer bridge, matching gpActiveMovieManager: GuiModule publishes
    // its owned AptRuntimeHost while prepared; BrnRendererModule renders through it.
    extern AptRuntimeHost* gpActiveAptRuntimeHost;

    // Phase 2: the host posts a movie-slot bundle load through the real GuiResourceModule
    // (defined in BrnGuiModule.cpp; forwards to gpActiveGuiModule->RequestAptMovieLoad).
    // Kept a free accessor so the AptRuntimeHost TU need not pull in the GuiModule layout.
    void RequestAptMovieLoadThroughModule(const char* lpacMovieName, s32 liType);
}

#endif
