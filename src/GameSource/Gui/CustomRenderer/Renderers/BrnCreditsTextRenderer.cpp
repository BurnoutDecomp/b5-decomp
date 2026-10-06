#include "BrnCreditsTextRenderer.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsID.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Language/CgsLanguageManager.h"
#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiResourceModuleIO.h"
#include "GameShared/GameClasses/Gui/View/CgsGuiViewModule.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/ImRenderBuffer/CgsIm2dRenderBuffer.h"
#include "pc/gcm/renderengine/renderstates.h"
#include <cmath>
#include <cstring>

namespace BrnGui {
namespace {
// ARTIST 82F2589C..82F258E4; names from DecFIGS BrnCreditsTextRenderer.cpp:23..40.
constexpr f32 KF_TEXTBOX_CENTRE_X=418.0f, KF_TEXTBOX_CENTRE_Y=386.0f;
constexpr f32 KF_TEXTBOX_WIDTH=500.0f, KF_TEXTBOX_HEIGHT=435.0f, KF_TEXTBOX_ANGLE=7.1f;
constexpr f32 KF_LARGE_TEXT_SIZE=37.0f, KF_SMALL_TEXT_SIZE=25.0f;
constexpr f32 KF_PARAGRAPH_SPACING0=-5.0f, KF_PARAGRAPH_SPACING1=25.0f;
constexpr f32 KF_SCROLL_START=-400.0f, KF_SCROLL_SPEED=62.0f;
constexpr f32 KF_FADE_BORDER=20.0f, KF_FADE_IN_START=-2.0f, KF_FADE_IN_SPEED=1.0f;
constexpr f32 KF_CREDITS_DROPSHADOW_X=2.0f, KF_CREDITS_DROPSHADOW_Y=3.0f;
constexpr f32 KF_DROPSHADOW_ALPHA=255.0f;
constexpr f32 KF_TEXTBOX_RIGHT=KF_TEXTBOX_WIDTH, KF_TEXTBOX_BOTTOM=KF_TEXTBOX_HEIGHT;
constexpr f32 KF_ONE=1.0f, KF_SHADOW_FADE_RAMP=-100.0f, KF_FADE_Y_MARGIN=100.0f;
constexpr f32 KF_FRAME_DT=0.016666668f;
}

void CreditsTextRenderer::Construct()
{
    CustomRenderComponentInterface::Construct();
    mpTitleFont.Clear();
    mpNormalFont.Clear();
    meCreditsType = E_CREDITS_TYPE_END;
    // Construct counts the 56 strings before the null in KAC_TESTSTRINGS
    // @82F258E8. Displayed names come from the localisation database.
    miNumStrings = 56;
    for (auto& lrParagraph : maParagraphs)
        lrParagraph = {};
}

bool CreditsTextRenderer::Prepare(CgsGui::GuiEventQueueSmall*,
                                  rw::IResourceAllocator* lpHeapAllocator,
                                  rw::IResourceAllocator*)
{
    // 82447198 stores r5 (the heap allocator, not the event queue).
    mpHeapAllocator = lpHeapAllocator;
    return true;
}

void CreditsTextRenderer::SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager)
{
    mpLanguageManager = lpLanguageManager;
}

CgsID CreditsTextRenderer::GetID() const
{
    return CgsIDCompress("CREDITS");
}

void CreditsTextRenderer::SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer)
{
    mpTextRenderer = lpTextRenderer;
}

void CreditsTextRenderer::SetRenderEnabled(bool lbRenderEnabled)
{
    mbRenderEnabled = lbRenderEnabled;

    if (lbRenderEnabled)
    {
        RecalculateParagraphs();
        mfScroll = KF_SCROLL_START;
        mfFade   = KF_FADE_IN_START;
    }
}

void CreditsTextRenderer::RecalculateParagraphs()
{

    mTitleTextObject.Construct(0, 0);
    mTitleTextObject.mpFont         = mpTitleFont;
    mTitleTextObject.mbAutosize     = false;
    mTitleTextObject.mfFontHeight = KF_LARGE_TEXT_SIZE;
    mTitleTextObject.mv2TopLeft.mX  = 0.0f;
    mTitleTextObject.mv2TopLeft.mY  = 0.0f;
    mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
    mTitleTextObject.mv2BottomRight.mY = KF_TEXTBOX_BOTTOM;
    mTitleTextObject.mbMultiLine    = 1;
    mTitleTextObject.mbWordWrap     = 1;
    mTitleTextObject.mbItalic       = 1;

    mNormalTextObject.Construct(0, 0);
    mNormalTextObject.mpFont        = mpNormalFont;
    mNormalTextObject.mbAutosize    = false;
    mNormalTextObject.mfFontHeight = KF_SMALL_TEXT_SIZE;
    mNormalTextObject.mv2TopLeft.mX = 0.0f;
    mNormalTextObject.mv2TopLeft.mY = 0.0f;
    mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
    mNormalTextObject.mv2BottomRight.mY = KF_TEXTBOX_BOTTOM;
    mNormalTextObject.mbMultiLine   = 1;
    mNormalTextObject.mbWordWrap    = 1;
    mNormalTextObject.mbItalic      = 1;

    s32 liCount = 0;
    f32 lfScrollPos = 0.0f;

    if (mpTextRenderer != 0 && mpLanguageManager != 0 &&
        !mpTitleFont.IsNull() && !mpNormalFont.IsNull())
    {
        const char* const lpcDetailFmt      = "CREDITS_DETAIL_%d";
        const char* const lpcTitleFmt       = "CREDITS_TITLE_%d";
        const char* const lpcReplayDetailFmt = "REPLAY_CREDITS_DETAIL_%d";
        const char* const lpcReplayTitleFmt  = "REPLAY_CREDITS_TITLE_%d";

        char lacTitleKey[128];
        char lacDetailKey[128];

        for (s32 liIndex = 0; ; )
        {

            const char* lpcTitleFmtSel;
            const char* lpcDetailFmtSel;
            if (meCreditsType == E_CREDITS_TYPE_END)
            {
                lpcTitleFmtSel  = lpcTitleFmt;
                lpcDetailFmtSel = lpcDetailFmt;
            }
            else if (meCreditsType == E_CREDITS_TYPE_REPLAY)
            {
                lpcTitleFmtSel  = lpcReplayTitleFmt;
                lpcDetailFmtSel = lpcReplayDetailFmt;
            }
            else
            {
                CGS_ASSERT(false, "Invalid type of credits to render : ");
                lpcTitleFmtSel  = lpcTitleFmt;
                lpcDetailFmtSel = lpcDetailFmt;
            }

            CgsCore::SPrintf(lacTitleKey, 128, lpcTitleFmtSel, liIndex);
            CgsCore::SPrintf(lacDetailKey, 128, lpcDetailFmtSel, liIndex);

            const CgsResource::CgsUtf8* lpTitleString =
                mpLanguageManager->FindString(lacTitleKey);
            const CgsResource::CgsUtf8* lpDetailString =
                mpLanguageManager->FindString(lacDetailKey);

            if (lpDetailString == 0)
            {

                if (meCreditsType != E_CREDITS_TYPE_REPLAY || liIndex >= 3)
                    break;
                ++liIndex;
                continue;
            }

            if (lpTitleString != 0)
            {
                maParagraphs[liCount].mfPosition = lfScrollPos;
                maParagraphs[liCount].mpText     = lpTitleString;
                maParagraphs[liCount].mbTitle    = true;

                mTitleTextObject.mpUtf8String = lpTitleString;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();

                const CgsResource::CgsUtf8* lpLine = 0;
                const u32 luLines =
                    mTitleTextObject.GetNumLinesAndStartLine(0, &lpLine) - 1;
                const f32 lfHeight = static_cast<f32>(luLines) * mTitleTextObject.mfFontHeight;
                maParagraphs[liCount].mfHeight = lfHeight;
                lfScrollPos += lfHeight + KF_PARAGRAPH_SPACING0;
                ++liCount;
            }

            maParagraphs[liCount].mfPosition = lfScrollPos;
            maParagraphs[liCount].mpText     = lpDetailString;
            maParagraphs[liCount].mbTitle    = false;

            mNormalTextObject.mpUtf8String = lpDetailString;
            if (mNormalTextObject.mbWordWrap == 1)
                mNormalTextObject.CalculateAutosizing();

            const CgsResource::CgsUtf8* lpLine2 = 0;
            const u32 luLines2 =
                mNormalTextObject.GetNumLinesAndStartLine(0, &lpLine2) - 1;
            const f32 lfHeight2 = static_cast<f32>(luLines2) * mNormalTextObject.mfFontHeight;
            maParagraphs[liCount].mfHeight = lfHeight2;
            lfScrollPos += lfHeight2 + KF_PARAGRAPH_SPACING1;
            ++liCount;

            ++liIndex;
        }
    }

    miNumStrings = liCount;
}

void CreditsTextRenderer::RenderComponent(CgsGui::ImRendererSet* lpRendererSet)
{
    RecalculateParagraphs();

    if (mfFade <= 0.0f)
        return;

    CgsGraphics::Im2dRenderBuffer* lpBuffer = lpRendererSet->mpIm2dRenderBuffer;

    lpBuffer->BeginRendering();
    lpBuffer->SetTransform(mScreenTransform);

    for (s32 liPara = 0; liPara < miNumStrings; ++liPara)
    {
        const ParagraphInfo& lrPara = maParagraphs[liPara];
        if (lrPara.mfPosition <= (mfScroll + KF_TEXTBOX_BOTTOM) &&
            (lrPara.mfPosition + lrPara.mfHeight) >= mfScroll)
        {
            const f32 lfTopY = (lrPara.mfPosition - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
            CgsGraphics::TextObject* lpObject;
            if (lrPara.mbTitle)
            {
                mTitleTextObject.mv2TopLeft.mX  = KF_CREDITS_DROPSHADOW_X;
                mTitleTextObject.mv2TopLeft.mY  = lfTopY;
                mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT + KF_CREDITS_DROPSHADOW_X;
                mTitleTextObject.mv2BottomRight.mY =
                    ((lrPara.mfPosition + lrPara.mfHeight) - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
                mTitleTextObject.mpUtf8String   = lrPara.mpText;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();

                const u32 luAlpha = static_cast<u32>(KF_DROPSHADOW_ALPHA * mfFade + 0.5f);
                mTitleTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(luAlpha << 24);
                lpObject = &mTitleTextObject;
            }
            else
            {
                mNormalTextObject.mv2TopLeft.mX  = KF_CREDITS_DROPSHADOW_X;
                mNormalTextObject.mv2TopLeft.mY  = lfTopY;
                mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT + KF_CREDITS_DROPSHADOW_X;
                mNormalTextObject.mv2BottomRight.mY =
                    ((lrPara.mfPosition + lrPara.mfHeight) - mfScroll) + KF_CREDITS_DROPSHADOW_Y;
                mNormalTextObject.mpUtf8String   = lrPara.mpText;
                if (mNormalTextObject.mbWordWrap == 1)
                    mNormalTextObject.CalculateAutosizing();
                const u32 luAlpha = static_cast<u32>(KF_DROPSHADOW_ALPHA * mfFade + 0.5f);
                mNormalTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(luAlpha << 24);
                lpObject = &mNormalTextObject;
            }

            mpTextRenderer->RenderStringFadingY(lpBuffer, *lpObject, 0.0f, KF_SHADOW_FADE_RAMP,
                                                KF_TEXTBOX_BOTTOM + KF_FADE_Y_MARGIN,
                                                KF_TEXTBOX_BOTTOM);
        }
    }

    for (s32 liPara = 0; liPara < miNumStrings; ++liPara)
    {
        const ParagraphInfo& lrPara = maParagraphs[liPara];
        if (lrPara.mfPosition <= (mfScroll + KF_TEXTBOX_BOTTOM) &&
            (lrPara.mfPosition + lrPara.mfHeight) >= mfScroll)
        {
            CgsGraphics::TextObject* lpObject;
            if (lrPara.mbTitle)
            {
                mTitleTextObject.mv2TopLeft.mY  = lrPara.mfPosition - mfScroll;
                mTitleTextObject.mv2TopLeft.mX  = 0.0f;
                mTitleTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
                mTitleTextObject.mv2BottomRight.mY =
                    (lrPara.mfPosition + lrPara.mfHeight) - mfScroll;
                mTitleTextObject.mpUtf8String   = lrPara.mpText;
                if (mTitleTextObject.mbWordWrap == 1)
                    mTitleTextObject.CalculateAutosizing();

                const u32 luAlpha = static_cast<u32>(mfFade * 255.0f + 0.5f) & 0xFF;
                const u32 luBrightness = 255u;
                mTitleTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(
                    (luAlpha << 24) | (luBrightness * 0x010101u));
                lpObject = &mTitleTextObject;
            }
            else
            {
                mNormalTextObject.mv2TopLeft.mY  = lrPara.mfPosition - mfScroll;
                mNormalTextObject.mv2TopLeft.mX  = 0.0f;
                mNormalTextObject.mv2BottomRight.mX = KF_TEXTBOX_RIGHT;
                mNormalTextObject.mv2BottomRight.mY =
                    (lrPara.mfPosition + lrPara.mfHeight) - mfScroll;
                mNormalTextObject.mpUtf8String   = lrPara.mpText;
                if (mNormalTextObject.mbWordWrap == 1)
                    mNormalTextObject.CalculateAutosizing();
                const u32 luAlpha = static_cast<u32>(mfFade * 255.0f + 0.5f) & 0xFF;
                const u32 luBrightness = 255u;
                mNormalTextObject.mTextColour = static_cast<CgsGraphics::RGBA>(
                    (luAlpha << 24) | (luBrightness * 0x010101u));
                lpObject = &mNormalTextObject;
            }

            mpTextRenderer->RenderStringFadingY(lpBuffer, *lpObject, 0.0f, KF_FADE_BORDER,
                                                KF_TEXTBOX_BOTTOM - KF_FADE_BORDER,
                                                KF_TEXTBOX_BOTTOM);
        }
    }

    lpBuffer->EndRendering();
}

// ARTIST RecvEvent @824571B8: native load notifications provide both fonts
// and the background mask; 587 selects end or replay credits.
void CreditsTextRenderer::RecvEvent(const CgsModule::Event* lpEvent, s32 liEventType)
{
    CGS_ASSERT(lpEvent != nullptr, " null event passed ");
    if (liEventType == 14)
    {
        const auto* lpNotification =
            static_cast<const CgsGui::GuiEventLoadNotification*>(lpEvent);
        CgsResource::SafeResourceHandle<CgsResource::Font> lFont;
        lFont.mpResourceMemory = lpNotification->mResourceHandle.mpResourceMemory;
        lFont.mpSourceEntry = lpNotification->mResourceHandle.mpSourceEntry;
        CGS_ASSERT(lFont.mpResourceMemory && *static_cast<void**>(lFont.mpResourceMemory),
                   "Invalid resource data sent AboveCarRenderer::RecvEvent");
        if (lpNotification->meRequestType == 16)
        {
            CGS_ASSERT(!lFont.IsNull(), "lpFont != CgsResource::NULLResourceHandle");
            const char* lpcFamily = lFont->macTypefaceFamilyName;
            if (_strnicmp(lpcFamily, "B5EAConDisS", 11) == 0)
                mpNormalFont = lFont;
            if (mpNormalFont.IsNull())
            {
                const char* lpcDefault = mpLanguageManager->GetDefaultFont();
                if (_strnicmp(lpcFamily, lpcDefault, std::strlen(lpcDefault)) == 0)
                    mpNormalFont = lFont;
            }
            if (mpTitleFont.IsNull() && _strnicmp(lpcFamily, "machinestd-bold", 15) == 0)
                mpTitleFont = lFont;
        }
        else if (lpNotification->meRequestType == 11 && lpNotification->muLoadRequestId == 203)
        {
            renderengine::TextureState::Parameters lParameters = {};
            lParameters.muAddressU = 2;
            lParameters.muAddressV = 2;
            lParameters.muMagFilter = 1;
            lParameters.muMinFilter = 1;
            lParameters.muMipFilter = 2;
            lParameters.muMaxAnisotropy = 13;
            lParameters.muField10 = 1;
            lParameters.mu8Field43 = 1;
            lParameters.mu8Field44 = 1;
            lParameters.mpTexture = *static_cast<renderengine::Texture**>(lFont.mpResourceMemory);
            CGS_ASSERT(lParameters.mpTexture != nullptr, "lpTexture!=NULL");
            u32 lauDescriptor[10];
            renderengine::TextureState::GetResourceDescriptor(lauDescriptor);
            rw::ResourceDescriptor lDescriptor;
            for (u32 lu = 0; lu < rw::KU_RESOURCE_LANE_COUNT; ++lu)
            {
                lDescriptor.m_baseResourceDescriptors[lu].m_size = lauDescriptor[2 * lu];
                lDescriptor.m_baseResourceDescriptors[lu].m_alignment = lauDescriptor[2 * lu + 1];
            }
            mBackgroundMaskTextureStateResource = mpHeapAllocator->DoAllocate(lDescriptor, nullptr);
            mpBackgroundMaskTextureState = renderengine::TextureState::Initialize(
                &mBackgroundMaskTextureStateResource, &lParameters);
        }
    }
    else if (liEventType == 587)
        meCreditsType = static_cast<ECreditsType>(*reinterpret_cast<const s32*>(lpEvent));
}

void CreditsTextRenderer::Update()
{
    // Scalar form of 824575F0..82457B14: centre the box, rotate it, then
    // convert logical pixels to NDC. The CRT at 82C51DEC/82C51FA8/82C51FD0
    // fills degrees-to-radians, 2/1280 and -2/720. The final basis/origin
    // agree with evaluation of the original instructions, including VMX.
    const f32 lfAngle = KF_TEXTBOX_ANGLE * 0.01745329238474369f;
    const f32 lfSin = std::sin(lfAngle);
    const f32 lfCos = std::cos(lfAngle);
    mScreenTransform.mRightUp = {lfCos / 640.0f, lfSin / 360.0f,
                                 lfSin / 640.0f, -lfCos / 360.0f};
    mScreenTransform.mOriginXYZ = {
        (KF_TEXTBOX_CENTRE_X - 640.0f - 0.5f * (KF_TEXTBOX_WIDTH * lfCos + KF_TEXTBOX_HEIGHT * lfSin)) / 640.0f,
        -(KF_TEXTBOX_CENTRE_Y - 360.0f + 0.5f * (KF_TEXTBOX_WIDTH * lfSin - KF_TEXTBOX_HEIGHT * lfCos)) / 360.0f,
        0.0f, 0.0f};
    mScreenTransform.mColourShift = {0.0f, 0.0f, 0.0f, 0.0f};
    mScreenTransform.mColourScale = {1.0f, 1.0f, 1.0f, 1.0f};
    mScreenTransform.TransformByAspectRatio();
    if (mbRenderEnabled)
    {
        mfScroll += KF_SCROLL_SPEED * KF_FRAME_DT;
        mfFade += KF_FADE_IN_SPEED * KF_FRAME_DT;
    }
    // 82457B6C/70 address paragraph N-1's position/height. The old
    // reconstruction mistook byte offsets for array indices N+6 and N.
    if (miNumStrings > 0)
    {
        const auto& lrLast = maParagraphs[miNumStrings - 1];
        if (mfScroll > lrLast.mfPosition + lrLast.mfHeight + KF_TEXTBOX_HEIGHT)
            mfScroll = -KF_TEXTBOX_HEIGHT;
    }
    if (mfFade > KF_ONE)
        mfFade = KF_ONE;
}
}
