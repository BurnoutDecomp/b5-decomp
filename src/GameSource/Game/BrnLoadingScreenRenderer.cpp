#include "GameSource/Game/BrnLoadingScreenRenderer.h"
#include "GameShared/GameClasses/System/CgsHardwareSku.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] bring-up traces
#include "GameSource/Resource/BrnResourceAllocator.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Timer/CgsTimeUtils.h"
#include <cstring>
#include <cmath>

// ARTIST owns the native loading path and layout. The four embedded tiled
// DXT5 arrays are linked from the original data; their symbols retain the
// DecFIGS names. The car staging region becomes the reusable scratch allocator.
namespace BrnGame
{
    extern u8 gpLoadingScreenTextureArrow[0x4000];
    extern u8 gpLoadingScreenTextureCar[0x200000];
    extern u8 gpLoadingScreenTextureText[0x40000];
    extern u8 gpLoadingScreenTextureBox[0x4000];
    bool gbUseRotatingLoadingArrow = true; // ARTIST byte_82F241FC
}

namespace
{
    CgsGraphics::Vector2 Point(f32 x, f32 y)
    {
        CgsGraphics::Vector2 point = { x, y };
        return point;
    }

    void SetScreenTransform(CgsGraphics::Im2d* renderer)
    {
        CgsGraphics::Im2dTransform transform;
        const rw::math::vpu::Vector4 origin = { -1.0f, 1.0f, 0.0f, 0.0f };
        const rw::math::vpu::Vector4 rightUp = { 0.0015625f, 0.0f, 0.0f, -0.0027777778f };
        const rw::math::vpu::Vector4 shift = { 0.0f, 0.0f, 0.0f, 0.0f };
        const rw::math::vpu::Vector4 scale = { 1.0f, 1.0f, 1.0f, 1.0f };
        transform.mOriginXYZ = origin;
        transform.mRightUp = rightUp;
        transform.mColourShift = shift;
        transform.mColourScale = scale;
        transform.TransformByAspectRatio();
        renderer->SetTransform(transform);
    }
    // Real loading-screen layout constants (.rdata @0x82F240xx, recovered from the XEX).
    const f32 KF_SCREEN_W = 1280.0f;
    const f32 KF_SCREEN_H = 720.0f;
    // Element half-sizes (the X360 Render halves these source values).
    const f32 KF_HALF_ARROW = 53.5f * 0.5f;   // flt_82F24078
    const f32 KF_HALF_TEXT_W = 427.0f * 0.5f;  // flt_82F24088
    const f32 KF_HALF_TEXT_H = 57.0f * 0.5f;   // flt_82F2408C
    const f32 KF_HALF_BOX = 51.0f * 0.5f;      // flt_82F24090/94
    // Centres (real .rdata). Arrow: flt_82F240C4/C8. Box: flt_82F240DC/E0.
    // Text: box centre + offset flt_82F240FC/24100 (220, -32) -> (920, 593).
    const f32 KF_ARROW_CX = 700.0f, KF_ARROW_CY = 625.0f;          // flt_82F240C4/C8
    const f32 KF_BOX_CX = 700.0f,   KF_BOX_CY = 625.0f;            // flt_82F240DC/E0
    const f32 KF_TEXT_CX = 700.0f + 220.0f, KF_TEXT_CY = 625.0f + (-32.0f);  // +flt_82F240FC/24100
    const f32 KF_TEXT_SCALE = 0.9f;            // flt_82F24104
    // Arrow spin: speed oscillates between MIN and MAX via sin(mfRotateSpeedInterp), which
    // advances at KF_ROTATE_ACCELERATION. Here MIN==MAX==550, so the speed is a constant
    // 550 deg/sec (the oscillation is a no-op, but kept faithful to the source).
    const f32 KF_ROTATE_MIN_SPEED    = 550.0f; // flt_82F240B8
    const f32 KF_ROTATE_MAX_SPEED    = 550.0f; // flt_82F240BC
    const f32 KF_ROTATE_ACCELERATION = 0.5f;   // flt_82F240C0
    const f32 KF_TWO_PI = 6.2831855f;
    // Fade: one mfFade drives two staged outputs. The bg ("black") fade completes by
    // KF_BLACK_FADE_POINT; the foreground alpha only starts at KF_ALPHA_FADE_POINT.
    const f32 KF_FADE_IN_SPEED    = 1.0f;       // flt_82F2410C
    const f32 KF_FADE_OUT_SPEED   = 2.0f;       // flt_82F24110
    const f32 KF_BLACK_FADE_POINT = 0.5f;       // flt_82F24114
    const f32 KF_ALPHA_FADE_POINT = 0.5f;       // flt_82F24118
    const f32 KF_DEG2RAD = 0.017453292f;
    const f32 KF_TILT_DEG = -8.0f;             // flt_82F240F8/24108 (box/text tilt)
    // UV extent = content / texture-logical-size (the engine's flt_82FAE5xx ratios).
    // Native D3D headers retain the 64x64 logical dimensions. Tiling allocation
    // granularity does not change these UVs (CRT initializers 82C4E320..E418).
    const f32 KF_ARROW_UV = (53.5f / 64.0f);
    const f32 KF_BOX_UV   = (51.0f / 64.0f);
    // Background: the car texture is 2048x1024 with the 1280x720 image in the top-left,
    // so UV = content/texture-size (flt_82FAE59C/5AC = flt_82F24080/A0, flt_82F24084/A4).
    const f32 KF_BG_U = 1280.0f / 2048.0f;     // 0.625
    const f32 KF_BG_V = 720.0f / 1024.0f;      // 0.703125
    // Text is 512-logical (no padding): U = flt_82F24088/A8, strip height = flt_82F2408C/AC.
    const f32 KF_TEXT_U = 427.0f / 512.0f;
    const f32 KF_TEXT_V_H = 57.0f / 512.0f;
    // CRT initializer 82C4E420 computes the language rows from original constants:
    // 28,28,428,128,328,228 divided by the 512-pixel texture height.
    const f32 KA_TEXT_STRIP_V[BrnGame::LoadingScreenRenderer::E_LOADINGLANGUAGE_COUNT] = {
        28.0f / 512.0f,    // E_LOADINGLANGUAGE_ENGLISH  -> strip 0
        28.0f / 512.0f,    // E_LOADINGLANGUAGE_JAPANESE -> (no JP strip; uses EN)
        428.0f / 512.0f,   // E_LOADINGLANGUAGE_FRENCH   -> strip 4
        128.0f / 512.0f,   // E_LOADINGLANGUAGE_GERMAN   -> strip 1 (LAEDT)
        328.0f / 512.0f,   // E_LOADINGLANGUAGE_SPANISH  -> strip 3
        228.0f / 512.0f,   // E_LOADINGLANGUAGE_ITALIAN  -> strip 2
    };

    // Build a textured quad (4 verts, triangle-strip order TL,TR,BL,BR) from 4 logical
    // 1280x720 points + a UV rect + colour. ImRenderer<V>::Render does the single
    // logical->backbuffer map, so DO NOT scale here (that double-scales: stretched + off
    // screen).
    void EmitQuad(CgsGraphics::Im2d* lpIm2d,
                  CgsGraphics::Vector2 lTL, CgsGraphics::Vector2 lTR,
                  CgsGraphics::Vector2 lBL, CgsGraphics::Vector2 lBR,
                  f32 lfU0, f32 lfV0, f32 lfU1, f32 lfV1,
                  CgsGraphics::RGBA8 lColour)
    {
        CgsGraphics::Basic2dColouredTexturedVertex laVerts[4];
        const CgsGraphics::Vector2 laPos[4] = { lTL, lBL, lTR, lBR };
        const f32 laUV[4][2] = { {lfU0,lfV0}, {lfU0,lfV1}, {lfU1,lfV0}, {lfU1,lfV1} };
        for (s32 i = 0; i < 4; ++i)
        {
            laVerts[i].mv2Pos = laPos[i];
            laVerts[i].mv2Tex0UV.x = laUV[i][0];
            laVerts[i].mv2Tex0UV.y = laUV[i][1];
            laVerts[i].mv4Colour = lColour;
        }
        lpIm2d->Render(static_cast<renderengine::PrimitiveType>(6), laVerts, 4);
    }

    // The four corners of a centre+half-size box, rotated by angleRad, in TL,TR,BL,BR order.
    void RotatedCorners(CgsGraphics::Vector2 (&laOut)[4], f32 lfCx, f32 lfCy,
                        f32 lfHalfW, f32 lfHalfH, f32 lfAngleRad)
    {
        const f32 lfSin = std::sin(lfAngleRad);
        const f32 lfCos = std::cos(lfAngleRad);
        const f32 laHx[4] = { -lfHalfW, lfHalfW, -lfHalfW, lfHalfW };
        const f32 laHy[4] = { -lfHalfH, -lfHalfH, lfHalfH, lfHalfH };
        for (s32 i = 0; i < 4; ++i)
        {
            laOut[i].x = lfCx + laHx[i] * lfCos - laHy[i] * lfSin;
            laOut[i].y = lfCy + laHx[i] * lfSin + laHy[i] * lfCos;
        }
    }
}

namespace BrnGame
{
    // @ 0x823C6B08 - create a 2D texture sized W x H and upload the supplied pixel data.
    renderengine::Texture2D* LoadingScreenRenderer::SetupLoadingScreenTexture(
        f32 lfWidth, f32 lfHeight, void* lpcData, s32 liDataSize)
    {
        renderengine::Texture2D::Parameters lParams;
        lParams.muFlags = 0;
        lParams.muWidth = static_cast<u32>(lfWidth + 0.5f);
        lParams.muHeight = static_cast<u32>(lfHeight + 0.5f);
        lParams.muNumLevels = 1;
        lParams.meFormat = renderengine::PIXELFORMAT_DXT5;

        const rw::ResourceDescriptor lDesc = renderengine::Texture2D::GetResourceDescriptor(lParams);
        rw::IResourceAllocator* lpAllocator = BrnResource::GetEmbeddedTextureAllocator();
        const rw::Resource lResource = lpAllocator->DoAllocate(lDesc, 0);
        renderengine::Texture2D* lpTexture = renderengine::Texture2D::Initialize(lResource, lParams);
        renderengine::Texture::Locked lLocked;
        lpTexture->Lock(0, 0, 0, lLocked);
        XMemCpy(lLocked.mpPixelData, lpcData, static_cast<size_t>(liDataSize));
        lpTexture->Unlock(lLocked);
        return lpTexture;
    }

    // ARTIST 823CE208. Preserve the exact initialization stores: this object is
    // statically zeroed before Construct; the function does not reset every field.
    void LoadingScreenRenderer::Construct()
    {
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Sizes: Arrow=" << 0x4000
                << ", Car=" << 0x200000 << ", Text=" << 0x40000 << ", Box=" << 0x4000 << "\n";
        mpArrowTexture = SetupLoadingScreenTexture(64.0f, 64.0f, gpLoadingScreenTextureArrow, 0x4000);
        mpCarTexture = SetupLoadingScreenTexture(2048.0f, 1024.0f, gpLoadingScreenTextureCar, 0x200000);
        mpTextTexture = SetupLoadingScreenTexture(512.0f, 512.0f, gpLoadingScreenTextureText, 0x40000);
        mpBoxTexture = SetupLoadingScreenTexture(64.0f, 64.0f, gpLoadingScreenTextureBox, 0x4000);
        mReusableDataBuffer.Construct();
        mReusableDataBuffer.Create(gpLoadingScreenTextureCar, 0x200000);
        if (CgsDev::Message::gxMessageFilterFlags & 1)
            *CgsDev::Log::gpDebugPrint << "Created re-usable data buffer of " << 0x200000 << " bytes\n";
        mbVisible = false;
        switch (CgsSystem::HardwareSku::FindLanguage())
        {
        case 7: case 8: meLanguage = E_LOADINGLANGUAGE_ENGLISH; break;
        case 10: meLanguage = E_LOADINGLANGUAGE_FRENCH; break;
        case 11: meLanguage = E_LOADINGLANGUAGE_GERMAN; break;
        case 15: meLanguage = E_LOADINGLANGUAGE_ITALIAN; break;
        case 16: meLanguage = E_LOADINGLANGUAGE_JAPANESE; break;
        case 22: meLanguage = E_LOADINGLANGUAGE_SPANISH; break;
        default:
            CgsDev::Assert::BeginAssert();
            {
                CgsDev::StrStream message(CgsDev::Assert::gpcMessageBuffer, CgsDev::Assert::giMessageBufferSize);
                message << "Language does not match SKU\n";
                CgsDev::Assert::FireAssert(CgsDev::Assert::gpcMessageBuffer,
                    "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Game/BrnLoadingScreenRenderer.cpp", 245);
            }
            CgsDev::Assert::EndAssert();
            break;
        }
        mbRenderInBackground = false;
        mbBlackOverlayHiding = false;
        mbBlackOverlayVisible = false;
        mbKillBlackOverlayWhenDone = false;
        mfBlackOverlayFade = 0.0f;
        mfArrowTranslation = 0.0f;
        mfRotateSpeedInterp = 0.0f;
        mfArrowDirection = 1.0f;
    }

    // @ 0x823AAD48 - map a command onto the visibility / black-overlay state.
    void LoadingScreenRenderer::AddCommand(ELoadingScreenCommand leCommand)
    {
        switch (leCommand)
        {
        case E_LSC_SHOW:
            mbVisible = true; mbHiding = false;
            mbRenderInBackground = false; mbKillBlackOverlayWhenDone = true;
            break;
        case E_LSC_HIDE:
            mbHiding = true;
            break;
        case E_LSC_SHOWSAVELOADBG:
            mbRenderInBackground = true; mbVisible = true; mbHiding = false;
            mbKillBlackOverlayWhenDone = true;
            break;
        case E_LSC_BLACKFADEIN:
            mfBlackOverlayFade = 1.2f; mbBlackOverlayVisible = true;
            mbBlackOverlayHiding = true; mbKillBlackOverlayWhenDone = false;
            break;
        case E_LSC_BLACKFADEOUT:
            mfBlackOverlayFade = 0.0f; mbBlackOverlayHiding = false;
            mbBlackOverlayVisible = true; mbKillBlackOverlayWhenDone = false;
            break;
        default:
            break;
        }
    }

    // @ BrnLoadingScreenRenderer.cpp - rotate a point about the origin by (sin, cos).
    CgsGraphics::Vector2 LoadingScreenRenderer::RotatePointAroundAngle(
        const CgsGraphics::Vector2& lPoint, f32 lfSin, f32 lfCos)
    {
        CgsGraphics::Vector2 lResult;
        lResult.x = lPoint.x * lfCos - lPoint.y * lfSin;
        lResult.y = lPoint.x * lfSin + lPoint.y * lfCos;
        return lResult;
    }

    // @ 0x823E79C8 - draw the background, the box/text panels and the spinning arrow.
    void LoadingScreenRenderer::Render(CgsGraphics::Im2d* lpIm2d)
    {
        using namespace CgsGraphics;

        // ARTIST 823E871C: the hidden path refreshes the timer and clears the
        // old foreground fade/rotation before returning without an Im2d pass.
        if (!mbVisible)
        {
            muLastTime = CgsSystem::GetSystemTimerBaseTime64();
            mfArrowRotation = 0.0f;
            mbHiding = false;
            mfFade = 0.0f;
            return;
        }

        lpIm2d->BeginRendering();

        SetScreenTransform(lpIm2d);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpRasterizerState_CullNone);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpBlendState_Standard);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpDepthStencilState_ZBufferOff);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpSamplerState_Linear_MipNearest_ClampUV);

        // --- Fade: one mfFade, two staged outputs (exactly as the source) -------------
        // mfFade ramps IN at KF_FADE_IN_SPEED, OUT at KF_FADE_OUT_SPEED, clamped to
        // lfFadeMax. X360 0x823E79C8: in save/load-background mode the cap is
        // KF_BLACK_FADE_POINT * 0.35 (= 0.175), which dims the background art to 35%
        // (0.175 / 0.5) with the foreground spinner/text alpha held at 0 - the dimmed
        // backdrop the autosave prompt draws over. The bg ("black") fade completes by
        // KF_BLACK_FADE_POINT, while the foreground alpha only begins at
        // KF_ALPHA_FADE_POINT - so the background reveals from black first, then the
        // spinner + text fade in. ARTIST 823E7D98..823E7E58 retains alpha 255
        // while inserting the three brightness bytes: bg = (b,b,b,255).
        // foreground = (b,b,b,fg) i.e. white once b==1, alpha-gated by the fg fade.
        const f32 lfFadeMax = mbRenderInBackground ? (KF_BLACK_FADE_POINT * 0.35f) : 1.0f;
        if (mbHiding)
        {
            mfFade -= mfTimeStep * KF_FADE_OUT_SPEED;
            if (mfFade < 0.0f) { mfFade = 0.0f; mbVisible = false; mbHiding = false; }
        }
        else if (mbVisible)
        {
            mfFade += mfTimeStep * KF_FADE_IN_SPEED;
            if (mfFade > lfFadeMax) { mfFade = lfFadeMax; }
        }
        const f32 lfBlackFade = (mfFade >= KF_BLACK_FADE_POINT)
                                ? 1.0f : (mfFade / KF_BLACK_FADE_POINT);
        const f32 lfFGAlphaFade = (mfFade >= KF_ALPHA_FADE_POINT)
                                ? ((mfFade - KF_ALPHA_FADE_POINT) / (1.0f - KF_ALPHA_FADE_POINT)) : 0.0f;
        const u8 luBlack = static_cast<u8>(lfBlackFade * 255.0f + 0.5f);
        const u8 luFgA   = static_cast<u8>(lfFGAlphaFade * 255.0f + 0.5f);
        const RGBA8 lBgCol = { luBlack, luBlack, luBlack, 255 };
        const RGBA8 lFgCol = { luBlack, luBlack, luBlack, luFgA };
        Vector2 laC[4];

        // Background / car: full-screen quad, sampling only the 1280x720 image region of
        // the 2048x1024 texture; greys up from black during the first fade stage.
        lpIm2d->SetTexture(mpCarTexture);
        EmitQuad(lpIm2d, Point(0.0f,0.0f), Point(KF_SCREEN_W,0.0f), Point(0.0f,KF_SCREEN_H), Point(KF_SCREEN_W,KF_SCREEN_H),
                 0.0f, 0.0f, KF_BG_U, KF_BG_V, lBgCol);

        // ARTIST submits both letterbox strips even for the original 1280-wide
        // background, where each strip has zero height.
        const RGBA8 lBlack = { 0, 0, 0, 255 };
        EmitQuad(lpIm2d, Point(0.0f,0.0f), Point(KF_SCREEN_W,0.0f), Point(0.0f,0.0f), Point(KF_SCREEN_W,0.0f),
                 0.0f, 0.0f, 0.0f, 0.0f, lBlack);
        EmitQuad(lpIm2d, Point(0.0f,KF_SCREEN_H), Point(KF_SCREEN_W,KF_SCREEN_H), Point(0.0f,KF_SCREEN_H), Point(KF_SCREEN_W,KF_SCREEN_H),
                 0.0f, 0.0f, 0.0f, 0.0f, lBlack);
        if (mbRenderInBackground)
        {
            lpIm2d->EndRendering();
            return;
        }

        if (gbUseRotatingLoadingArrow)
        {
        // The native toggle selects rotating versus translated-arrow animation.
        mfRotateSpeedInterp += KF_ROTATE_ACCELERATION * mfTimeStep;
        if (mfRotateSpeedInterp > KF_TWO_PI) { mfRotateSpeedInterp -= KF_TWO_PI; }
        const f32 lfSpeed = (KF_ROTATE_MAX_SPEED - KF_ROTATE_MIN_SPEED)
                            * (std::sin(mfRotateSpeedInterp) + 1.0f) * 0.5f + KF_ROTATE_MIN_SPEED;
        mfArrowRotation += lfSpeed * mfTimeStep;
        if (mfArrowRotation > 180.0f) { mfArrowRotation -= 360.0f; }
        lpIm2d->SetTexture(mpArrowTexture);
        RotatedCorners(laC, KF_ARROW_CX, KF_ARROW_CY, KF_HALF_ARROW, KF_HALF_ARROW, mfArrowRotation * KF_DEG2RAD);
        EmitQuad(lpIm2d, laC[0], laC[1], laC[2], laC[3], 0.0f, 0.0f, KF_ARROW_UV, KF_ARROW_UV, lFgCol);
        }
        else
        {
            lpIm2d->SetTexture(mpBoxTexture);
            RotatedCorners(laC, KF_BOX_CX, KF_BOX_CY, KF_HALF_BOX, KF_HALF_BOX, KF_TILT_DEG * KF_DEG2RAD);
            EmitQuad(lpIm2d, laC[0], laC[1], laC[2], laC[3], 0.0f, 0.0f, KF_BOX_UV, KF_BOX_UV, lFgCol);

            lpIm2d->SetTexture(mpArrowTexture);
            mfArrowTranslation += (mfArrowDirection * mfTimeStep) * 30.0f;
            if (mfArrowTranslation > 10.0f)
            {
                mfArrowDirection = -mfArrowDirection;
                mfArrowTranslation = 10.0f - (mfArrowTranslation - 10.0f);
            }
            if (mfArrowTranslation < 0.0f)
            {
                mfArrowTranslation = -mfArrowTranslation;
                mfArrowDirection = -mfArrowDirection;
            }
            const f32 lfAngle = KF_TILT_DEG * KF_DEG2RAD;
            const f32 lfSin = static_cast<f32>(std::sin(lfAngle));
            const f32 lfCos = static_cast<f32>(std::cos(lfAngle));
            const f32 lfOffsetY = 5.0f - mfArrowTranslation;
            RotatedCorners(laC, KF_BOX_CX, KF_BOX_CY, KF_HALF_ARROW, KF_HALF_ARROW, lfAngle);
            for (u32 i = 0; i != 4; ++i)
            {
                laC[i].x -= lfSin * lfOffsetY;
                laC[i].y += lfCos * lfOffsetY;
            }
            // ARTIST's alternate arrow intentionally uses the box UV extent.
            EmitQuad(lpIm2d, laC[0], laC[1], laC[2], laC[3], 0.0f, 0.0f, KF_BOX_UV, KF_BOX_UV, lFgCol);
        }

        // Text bar (tilted): sample only the current language's V-strip (flt_82FAE970).
        const s32 liLang = meLanguage;
        const f32 lfTextV0 = KA_TEXT_STRIP_V[liLang];
        lpIm2d->SetTexture(mpTextTexture);
        RotatedCorners(laC, KF_TEXT_CX, KF_TEXT_CY,
                       KF_HALF_TEXT_W * KF_TEXT_SCALE, KF_HALF_TEXT_H * KF_TEXT_SCALE, KF_TILT_DEG * KF_DEG2RAD);
        EmitQuad(lpIm2d, laC[0], laC[1], laC[2], laC[3],
                 0.0f, lfTextV0, KF_TEXT_U, lfTextV0 + KF_TEXT_V_H, lFgCol);

        lpIm2d->EndRendering();
    }

    // @ 0x823E8750 - the fading full-screen black overlay.
    void LoadingScreenRenderer::RenderBlackOverlay(CgsGraphics::Im2d* lpIm2d)
    {
        using namespace CgsGraphics;
        if (!mbBlackOverlayVisible) { return; }

        lpIm2d->BeginRendering();
        SetScreenTransform(lpIm2d);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpRasterizerState_CullNone);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpBlendState_Standard);
        lpIm2d->SetState(ImRendererBase::mgStateLibrary.mpDepthStencilState_ZBufferOff);

        bool lbHideAfterDraw = false;
        if (mbBlackOverlayHiding)
        {
            mfBlackOverlayFade -= mfTimeStep * 2.0f;
            if (mfBlackOverlayFade < 0.0f) { mfBlackOverlayFade = 0.0f; mbBlackOverlayHiding = false; lbHideAfterDraw = true; }
        }
        else
        {
            mfBlackOverlayFade += mfTimeStep * 2.0f;
            if (mfBlackOverlayFade >= 3.0f && mbKillBlackOverlayWhenDone) { lbHideAfterDraw = true; }
        }
        if (lbHideAfterDraw) { mbBlackOverlayVisible = false; }

        f32 lfAlpha = mfBlackOverlayFade; if (lfAlpha > 1.0f) { lfAlpha = 1.0f; } if (lfAlpha < 0.0f) { lfAlpha = 0.0f; }
        const RGBA8 lBlack = { 0, 0, 0, static_cast<u8>(lfAlpha * 255.0f + 0.5f) };
        lpIm2d->SetTexture(mpCarTexture);
        EmitQuad(lpIm2d, Point(0.0f,0.0f), Point(KF_SCREEN_W,0.0f), Point(0.0f,KF_SCREEN_H), Point(KF_SCREEN_W,KF_SCREEN_H),
                 0.0f, 0.0f, KF_BG_U, KF_BG_V, lBlack);
        lpIm2d->EndRendering();
    }

    // @ 0x823EDE18 - foreground path: advance the frame timestep, draw the loading
    // visuals (if not backgrounded and no overlay), then update the black overlay.
    void LoadingScreenRenderer::RenderForeground(CgsGraphics::Im2d* lpIm2d)
    {
        const u64 luFirstTime = CgsSystem::GetSystemTimerBaseTime64();
        const u64 luSecondTime = CgsSystem::GetSystemTimerBaseTime64();
        const u64 luDelta = luSecondTime - muLastTime;
        muLastTime = luFirstTime;
        const u64 luFrequency = CgsSystem::GetSystemTimerFrequency64();
        f32 lfStep = static_cast<f32>((luDelta * 10000) / luFrequency) * 0.0001f;
        if (lfStep > 0.1f) lfStep = 0.1f;
        mfTimeStep = lfStep;

        if (!mbRenderInBackground && !mbBlackOverlayVisible)
        {
            Render(lpIm2d);
        }
        RenderBlackOverlay(lpIm2d);
    }

    // @ 0x823EDEB8 - background path: draw the loading visuals when running as a layer.
    void LoadingScreenRenderer::RenderBackground(CgsGraphics::Im2d* lpIm2d)
    {
        if (mbRenderInBackground && !mbBlackOverlayVisible)
        {
            Render(lpIm2d);
        }
    }

}
