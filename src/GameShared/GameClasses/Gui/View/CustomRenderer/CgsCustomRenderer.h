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

        // ---- ADDITIVE GROW (FLAG): the polymorphic component lifecycle/dispatch
        // interface the GUI CustomRendererManager drives every renderer through. The
        // X360 manager (BrnGui::CustomRendererManager, GameSource/Gui/CustomRenderer/
        // BrnCustomRenderer.cpp) calls these through the component-pointer array via the
        // renderer vtables, at these slots (byte offsets observed in the manager asm):
        //   +0x00 Construct, +0x04 Prepare, +0x08 Release, +0x0C Destruct,
        //   +0x10 GetComponentTexture, +0x14 RecvEvent, +0x18 Update,
        //   +0x1C SetRenderEnabled (above), +0x24 GetComponentID,
        //   +0x28 GetNumTexturesForComponent.
        // Modelled as NON-pure virtuals (empty/neutral defaults) so the existing
        // concrete minimal-slice renderers (e.g. BrnGui::MainMapRenderer) stay
        // instantiable. The original game vtable slot numbers are NOT load-bearing on
        // the 64-bit host gate (overrides bind by signature); declared here in the
        // observed relative order for honesty. FLAG: grown interface -- the full
        // parameter types (rw::IResourceAllocator, ImRendererSet, renderengine::Texture,
        // CgsModule::Event, CgsID) are uncommitted, so opaque pointers / s32 stand in.
        virtual void Construct()                                  {}
        virtual bool Prepare(void* lpResourceAllocator, void* lpA, void* lpB)
        {
            (void)lpResourceAllocator; (void)lpA; (void)lpB; return true;
        }
        virtual bool Release()                                    { return true; }
        virtual void Destruct()                                   {}
        // +0x10: fetch a texture for this component (texture index, the asserted out/shader
        // pointer, renderer set). Returns an opaque texture pointer. The manager passes its
        // own a3/a4/a5 straight through (asm: (*(comp+16))(comp, a3, a4, a5)).
        virtual void* GetComponentTexture(s32 liTextureIndex, void* lpiShaderProgram,
                                          void* lpRendererSet)
        {
            (void)liTextureIndex; (void)lpiShaderProgram; (void)lpRendererSet; return 0;
        }
        // +0x14: receive a module event (event pointer, event type id).
        virtual void RecvEvent(const void* lpEvent, s32 liEventType)
        {
            (void)lpEvent; (void)liEventType;
        }
        // +0x18: per-frame update.
        virtual void Update()                                     {}
        // +0x24: the component's CgsID (returned as u32; the real CgsID is uncommitted).
        virtual u32  GetComponentID() const                       { return 0; }
        // +0x28: number of textures this component exposes.
        virtual s32  GetNumTexturesForComponent() const           { return 0; }

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
