#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"

// Exercises the in-scope ledger function BrnGui::MainMapRenderer::SetRenderEnabled
// (@ 0x82C290D8) -- the override that stores the enabled flag into the inherited
// base member CgsGui::CustomRenderComponentInterface::mbRenderEnabled (+0x04).

extern "C" int CgsCustomRenderer_embed_check()
{
    BrnGui::MainMapRenderer lRenderer;

    lRenderer.SetRenderEnabled(true);
    if (!lRenderer.GetRenderEnabled())
        return 1;

    lRenderer.SetRenderEnabled(false);
    if (lRenderer.GetRenderEnabled())
        return 2;

    // Base setter (DWARF-sourced inline) -- behaviourally identical.
    CgsGui::CustomRenderComponentInterface& lBase = lRenderer;
    lBase.SetRenderEnabled(true);
    if (!lBase.GetRenderEnabled())
        return 3;

    if (lBase.GetRenderLayer() != CgsGui::E_CUSTOMRENDERLAYER_1)
        return 4;

    return 0;
}
