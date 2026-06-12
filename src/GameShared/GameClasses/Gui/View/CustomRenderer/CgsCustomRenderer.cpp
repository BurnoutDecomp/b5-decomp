#include "types.hpp"

// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x82C290D8
//   (BrnGui::MainMapRenderer::SetRenderEnabled)
//
// NOTE: this function is keyed to CgsCustomRenderer.h by DecFIGS file attribution,
// but its identity is BrnGui::MainMapRenderer::SetRenderEnabled (a derived custom
// renderer). Behaviour-faithful to the X360 pseudocode:
//     *(this + 4) = a2;             // store the enabled flag at offset 4
//     return this;

namespace BrnGui
{
    struct MainMapRenderer
    {
        u32  muPad0;            // [0x00] opaque (vtable etc.)
        bool mbRenderEnabled;   // [0x04] render-enabled flag

        MainMapRenderer* SetRenderEnabled(bool lbRenderEnabled);
    };

    MainMapRenderer* MainMapRenderer::SetRenderEnabled(bool lbRenderEnabled)
    {
        mbRenderEnabled = lbRenderEnabled;
        return this;
    }
}
