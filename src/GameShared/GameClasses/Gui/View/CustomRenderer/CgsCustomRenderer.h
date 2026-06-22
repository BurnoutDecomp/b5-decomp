#ifndef CGS_CUSTOM_RENDERER_H
#define CGS_CUSTOM_RENDERER_H

#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnGui::MainMapRenderer::SetRenderEnabled  @ 0x82C290D8
//
// CgsCustomRenderer.h homes the GUI custom-renderer base hierarchy. The in-scope
// ledger function is the SetRenderEnabled override emitted for the concrete
// renderer BrnGui::MainMapRenderer; its body simply stores the enabled flag into
// the inherited base member CgsGui::CustomRenderComponentInterface::mbRenderEnabled.
//
//   0x82C290D8  stb r4, 4(r3)   ; this->mbRenderEnabled = a2  (base member @ +0x04)
//   0x82C290DC  blr
//
// On X360 the base layout is { _vptr [+0x00]; bool mbRenderEnabled [+0x04]; ... },
// so the store at +0x04 is mbRenderEnabled (DWARF CgsCustomRenderer.h:182). The
// override carries no extra logic -- it is behaviourally the inline base setter
// (the DWARF CustomRenderComponentInterface::SetRenderEnabled).
//
// MINIMAL OWNING SLICE: the full full DWARF header pulls in CgsRenderTarget, the
// immediate-mode renderers, CgsGraphics::Camera, GuiEventQueueSmall, CgsID,
// TextRenderer and renderengine::Texture -- all uncommitted. Only the base member
// the ledger function touches (mbRenderEnabled) and the vtable-shaping virtuals are
// modelled here. FLAG: minimal-slice base; the render-target / im-renderer / event
// members and the resource-binding virtuals are intentionally OMITTED (uncommitted
// dependencies, none in scope).

namespace CgsGui
{
    // DWARF CgsCustomRenderer.h:95
    enum eCustomRenderLayer
    {
        E_CUSTOMRENDERLAYER_1 = 1,
        E_CUSTOMRENDERLAYER_2 = 2,

        E_CUSTOMRENDERLAYER_COUNT = 3
    };

    // DWARF CgsCustomRenderer.h:105 base interface.
    //   _vptr          [+0x00]
    //   mbRenderEnabled[+0x04]  (h:182)
    class CustomRenderComponentInterface
    {
    public:
        // h:307 -- inline base setter (DWARF-sourced ground truth). The
        // MainMapRenderer override @ 0x82C290D8 is behaviourally identical.
        virtual void SetRenderEnabled(bool lbRenderEnabled)
        {
            mbRenderEnabled = lbRenderEnabled;
        }

        // h:324
        bool GetRenderEnabled() const { return mbRenderEnabled; }

        // h:341 -- the DWARF returns the first layer unconditionally.
        virtual eCustomRenderLayer GetRenderLayer() const
        {
            return E_CUSTOMRENDERLAYER_1;
        }

    protected:
        bool mbRenderEnabled;   // [+0x04] h:182
    };
}

namespace BrnGui
{
    // DWARF BrnMainMapRenderer.h:52 -- MainMapRenderer : public CustomRenderComponentInterface.
    // MINIMAL SLICE: only modelled far enough to body the in-scope SetRenderEnabled
    // override; the full renderer state (fade/pulse/route members, particle systems,
    // texture-state resources) lives in BrnMainMapRenderer.* (out of scope here).
    class MainMapRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        // The ledger function @ 0x82C290D8. Sets the inherited base flag by name;
        // the guest store lands at +0x04 (the base mbRenderEnabled).
        virtual void SetRenderEnabled(bool lbRenderEnabled)
        {
            mbRenderEnabled = lbRenderEnabled;
        }
    };
}

#endif // CGS_CUSTOM_RENDERER_H
