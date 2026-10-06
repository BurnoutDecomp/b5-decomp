#pragma once

#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"

namespace BrnGui
{
    // ARTIST behavior and DecFIGS BrnBlackBarRenderer.h declaration shape.
    class BlackBarRenderer : public CgsGui::CustomRenderComponentInterface
    {
    public:
        void Construct() override;
        void RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType) override;
        CgsID GetID() const override;
        s32 GetNumTextures() const override { return 1; } // ARTIST vtable820CEB08 +28 ->82C296C8.

    private:
        void RenderComponent(CgsGui::ImRendererSet* lpRendererSet) override;

        f32 mfBarHeight;
        bool mbStartedGame;
    };
}
