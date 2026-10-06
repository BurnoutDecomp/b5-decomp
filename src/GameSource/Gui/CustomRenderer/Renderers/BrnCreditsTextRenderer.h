#pragma once
#include "GameShared/GameClasses/Gui/View/CustomRenderer/CgsCustomRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h"
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"
#include "GameShared/GameClasses/Fonts/CgsFont.h"
namespace renderengine { class TextureState; }
namespace BrnGui {
class CreditsTextRenderer : public CgsGui::CustomRenderComponentInterface {
public:
    static const s32 KI_MAX_PARAGRAPHS = 500;
    struct ParagraphInfo {
        f32 mfHeight;
        f32 mfPosition;
        const CgsResource::CgsUtf8* mpText;
        bool mbTitle;
    };
    enum ECreditsType { E_CREDITS_TYPE_END = 0, E_CREDITS_TYPE_REPLAY = 1 };
    void Construct() override;
    bool Prepare(CgsGui::GuiEventQueueSmall*, rw::IResourceAllocator*, rw::IResourceAllocator*) override;
    void RecvEvent(const CgsModule::Event*, s32) override;
    void Update() override;
    CgsID GetID() const override;
    s32 GetNumTextures() const override { return 1; } // ARTIST vtable820CF910 +28 ->82C296C8.
    virtual void SetTextRenderer(CgsGraphics::TextRenderer*);
    virtual void SetLanguageManager(CgsLanguage::LanguageManager*);
    void SetRenderEnabled(bool) override;
private:
    void RenderComponent(CgsGui::ImRendererSet*) override;
    void RecalculateParagraphs();
    rw::IResourceAllocator* mpHeapAllocator;
    CgsGraphics::Im2dTransform mScreenTransform;
    CgsGraphics::TextRenderer* mpTextRenderer;
    CgsLanguage::LanguageManager* mpLanguageManager;
    CgsResource::SafeResourceHandle<CgsResource::Font> mpNormalFont;
    CgsResource::SafeResourceHandle<CgsResource::Font> mpTitleFont;
    s32 miNumStrings;
    ParagraphInfo maParagraphs[KI_MAX_PARAGRAPHS];
    CgsGraphics::TextObject mTitleTextObject;
    CgsGraphics::TextObject mNormalTextObject;
    f32 mfScroll;
    f32 mfFade;
    rw::Resource mBackgroundMaskTextureStateResource;
    renderengine::TextureState* mpBackgroundMaskTextureState;
    ECreditsType meCreditsType;
};
}
