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
    void Construct() ;
    bool Prepare(CgsGui::GuiEventQueueSmall*, rw::IResourceAllocator*, rw::IResourceAllocator*) ;
    void RecvEvent(const CgsModule::Event*, s32) ;
    void Update() ;
    CgsID GetID() const ;
    s32 GetNumTextures() const  { return 1; } // ARTIST vtable820CF910 +28 ->82C296C8.
    virtual void SetTextRenderer(CgsGraphics::TextRenderer*);
    virtual void SetLanguageManager(CgsLanguage::LanguageManager*);
    void SetRenderEnabled(bool) ;
private:
    void RenderComponent(CgsGui::ImRendererSet*) ;
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
