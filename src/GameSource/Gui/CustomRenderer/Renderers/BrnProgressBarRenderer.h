#ifndef BRN_PROGRESS_BAR_RENDERER_H
#define BRN_PROGRESS_BAR_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                                  // Vector4, CgsID
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h" // CgsGui::CustomRenderComponentInterface (real base)
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3d.h"            // CgsGraphics::Im3dRenderBufferUntex

// Forward declarations for pointer-only parameter types (their real homes are out of scope
// for this TU; only opaque pointers cross the in-scope interface):
namespace CgsModule { struct Event; }                 // RecvEvent's event base (empty marker)
namespace rw        { struct IResourceAllocator; }    // Prepare's cached allocator

// BrnGui::ProgressBarRenderer - the boot/loading "progress bar" custom HUD renderer.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::ProgressBarRenderer::Construct        @ 0x82446C00  (EXECUTED in the boot trace)
//   BrnGui::ProgressBarRenderer::GetID            @ 0x82446E50
//   BrnGui::ProgressBarRenderer::Prepare          @ 0x82446C48
//   BrnGui::ProgressBarRenderer::Release          @ 0x82446CF8
//   BrnGui::ProgressBarRenderer::RecvEvent        @ 0x82446DA0
//   BrnGui::ProgressBarRenderer::RenderQuadUntex  @ 0x8245C828
//
// Layout (DWARF BrnProgressBarRenderer.h) sits on the real CgsGui::CustomRenderComponentInterface
// base { _vptr [+0x00]; bool mbRenderEnabled [+0x04] }; the renderer adds, in order:
//   mePrepareStage         [guest +0x08]  EPrepareStage    (Prepare's one-shot guard)
//   meReleaseStage         [guest +0x0C]  EReleaseStage    (Release's one-shot guard)
//   mpHeapAllocator        [guest +0x10]  rw::IResourceAllocator*  (cached in Prepare)
//   mRenderProgressBarEvent[guest +0x14]  cached progress-bar event (one leading float)
// On the 64-bit host gate the byte offsets widen (pointer 4->8), so the guest offsets are
// documented but not load-bearing; members are accessed BY NAME.

namespace BrnGui
{
    // The cached "render progress bar" GUI event payload. The X360 ARTIST DWARF
    // (BrnGuiEventTypeDefs.h:2566) attests `GuiEventRenderProgressBar : GuiEvent<223>` with a
    // single `float32_t mProgressPercent`, and in the ARTIST build the GuiEvent<N> base carries
    // NO leading data words (DWARF: `struct GuiEvent<N> : Event {}` -- empty), so the payload's
    // mProgressPercent sits at offset 0 of the event. This is provable from the ARTIST asm:
    //   - Construct stores -1.0f directly at the member base (the cached "no value yet" sentinel);
    //   - RecvEvent copies the incoming event's FIRST dword into the member's leading float.
    // FLAG: build-version layout. The committed shared CgsGui::GuiEvent<N> (CgsGuiEvent.h) models
    // a 12-byte {muHeader0/muEventType/muHeader2} header from a different interpretation; that
    // would place mProgressPercent at +0x0C and contradict the ARTIST asm's offset-0 copy. To
    // stay faithful to the ARTIST build without disturbing that shared type, the in-scope cached
    // event is modelled here as the single float the asm actually touches.
    struct GuiEventRenderProgressBar
    {
        f32 mfProgressPercent;  // DWARF BrnGuiEventTypeDefs.h:2568 (ARTIST: at event offset 0)
    };

    class ProgressBarRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // DWARF BrnProgressBarRenderer.h:49 -- Prepare's one-shot stage guard.
        enum EPrepareStage
        {
            E_PREPARESTAGE_START = 0,
            E_PREPARESTAGE_DONE  = 1,
        };

        // DWARF BrnProgressBarRenderer.h:55 -- Release's one-shot stage guard.
        enum EReleaseStage
        {
            E_RELEASESTAGE_START = 0,
            E_RELEASESTAGE_DONE  = 1,
        };

        // ---- CustomRenderComponentInterface overrides (vtable order per DWARF) ----------------
        // The base models the lifecycle interface with opaque void*/s32 parameters (the real
        // GuiEventQueueSmall / rw::IResourceAllocator / CgsModule::Event types are uncommitted in
        // the base's slice); these overrides keep the base's binding signatures and recover the
        // real DWARF parameter types in comments.
        virtual void Construct();                                                  // @ 0x82446C00
        virtual bool Prepare(void* lpEventQueueSmall, void* lpResourceAllocatorA,  // @ 0x82446C48
                             void* lpResourceAllocatorB);
        virtual bool Release();                                                    // @ 0x82446CF8
        virtual void RecvEvent(const void* lpEvent, s32 liEventType);              // @ 0x82446DA0

        // DWARF `virtual CgsID GetID() const` @ 0x82446E50. The base's component-id slot is typed
        // u32 in its minimal slice; the ARTIST function returns the full 64-bit CgsID, so the
        // faithful signature is declared here. FLAG: return widened to CgsID vs the base's u32
        // GetComponentID slot -- the byte-for-byte vtable index is not load-bearing on the host.
        virtual CgsID GetID() const;

        // DWARF BrnProgressBarRenderer.h:264 -- draws one untextured coloured quad into the 3D
        // untextured immediate-render buffer. Non-virtual helper (called by RenderComponent).
        //   lrBounds = screen-rectangle bounds (x=left, y=top, z=right, w=bottom)
        //   lrColour = the quad colour as floats in [0,1] (packed to RGBA8 per vertex)
        void RenderQuadUntex(CgsGraphics::Im3dRenderBufferUntex* lpBuffer,
                             const Vector4& lrBounds,
                             const Vector4& lrColour);

    private:
        EPrepareStage           mePrepareStage;          // guest +0x08
        EReleaseStage           meReleaseStage;          // guest +0x0C
        rw::IResourceAllocator* mpHeapAllocator;         // guest +0x10
        GuiEventRenderProgressBar mRenderProgressBarEvent; // guest +0x14
    };
}

#endif // BRN_PROGRESS_BAR_RENDERER_H
