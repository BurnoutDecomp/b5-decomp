#include "GameSource/Game/BrnLoadingScreenRenderer.h"
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2d.h"
#include "GameShared/GameClasses/Development/Log/CgsLog.h"   // [diag] bring-up traces
#include "pc/gcm/renderengine/device.h"   // gDisplayWidth/Height

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

// Reconstructed from BURNOUT_X360_ARTIST.XEX with the DecFIGS DWARF
// (BrnLoadingScreenRenderer.cpp) as the structural authority. The loading screen draws
// through the engine's standard Im2d: a full-screen background plus the box, text and a
// spinning arrow, each as a (rotated) textured quad, with a separate black-overlay fade.
// The X360/PS3 emit this as hand-VMX immediate-mode batches; this is the equivalent
// clean logical C++. Layout/animation constants are the real .rdata values recovered
// from the XEX. Textures are baked into the build and loaded from external .dds files.

namespace CgsSystem
{
    u32 GetSystemTimerBaseTime();
    u32 GetSystemTimerFrequency();
    namespace HardwareSku { s32 FindLanguage(); }
}

namespace
{
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
    // arrow/box are 64-logical in a 128-physical tiled surface, so * 64/128.
    const f32 KF_ARROW_UV = (53.5f / 64.0f) * (64.0f / 128.0f);    // flt_82F2407C/9C, /physical
    const f32 KF_BOX_UV   = (51.0f / 64.0f) * (64.0f / 128.0f);    // flt_82F24094/B4, /physical
    // Background: the car texture is 2048x1024 with the 1280x720 image in the top-left,
    // so UV = content/texture-size (flt_82FAE59C/5AC = flt_82F24080/A0, flt_82F24084/A4).
    const f32 KF_BG_U = 1280.0f / 2048.0f;     // 0.625
    const f32 KF_BG_V = 720.0f / 1024.0f;      // 0.703125
    // Text is 512-logical (no padding): U = flt_82F24088/A8, strip height = flt_82F2408C/AC.
    const f32 KF_TEXT_U = 427.0f / 512.0f;
    const f32 KF_TEXT_V_H = 57.0f / 512.0f;
    // Per-language strip V-offset (the runtime flt_82FAE970[] table). Recovered from the
    // text texture's strip layout: 5 strips at rows 29/128/229/329/429, ordered
    // EN, DE, IT, ES, FR (top to bottom); Japanese reuses the English strip.
    const f32 KA_TEXT_STRIP_V[BrnGame::LoadingScreenRenderer::E_LOADINGLANGUAGE_COUNT] = {
        29.0f / 512.0f,    // E_LOADINGLANGUAGE_ENGLISH  -> strip 0
        29.0f / 512.0f,    // E_LOADINGLANGUAGE_JAPANESE -> (no JP strip; uses EN)
        429.0f / 512.0f,   // E_LOADINGLANGUAGE_FRENCH   -> strip 4
        128.0f / 512.0f,   // E_LOADINGLANGUAGE_GERMAN   -> strip 1 (LAEDT)
        329.0f / 512.0f,   // E_LOADINGLANGUAGE_SPANISH  -> strip 3
        229.0f / 512.0f,   // E_LOADINGLANGUAGE_ITALIAN  -> strip 2
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
        const CgsGraphics::Vector2 laPos[4] = { lTL, lTR, lBL, lBR };
        const f32 laUV[4][2] = { {lfU0,lfV0}, {lfU1,lfV0}, {lfU0,lfV1}, {lfU1,lfV1} };
        for (s32 i = 0; i < 4; ++i)
        {
            laVerts[i].mv2Pos = { laPos[i].x, laPos[i].y };
            laVerts[i].mv2Tex0UV = { laUV[i][0], laUV[i][1] };
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
        renderengine::Texture2D::Parameters lParams = {};
        lParams.muWidth = static_cast<u32>(lfWidth + 0.5f);
        lParams.muHeight = static_cast<u32>(lfHeight + 0.5f);
        lParams.muDepth = 1;
        lParams.muNumLevels = 1;
        lParams.muFormat = 340;

        renderengine::Texture2D::ResourceDescriptor lDesc;
        renderengine::Texture2D::GetResourceDescriptor(&lDesc, &lParams);
        renderengine::Texture2D* lpTexture = renderengine::Texture2D::Initialize(&lDesc, &lParams);

        renderengine::Texture::LockInfo lLocked;
        renderengine::Texture::Lock(lpTexture, 0, 0, 0, &lLocked);
        if (lLocked.mpBits != nullptr && lpcData != nullptr)
        {
            std::memcpy(lLocked.mpBits, lpcData, static_cast<size_t>(liDataSize));
        }
        renderengine::Texture::Unlock(lpTexture, &lLocked);
        return lpTexture;
    }

    // Load a baked loading-screen texture from an external .dds. The 128-byte DDS header
    // (4-byte 'DDS ' magic + DDS_HEADER) carries the dimensions and pixel format, so the
    // size/format come from the file rather than a hand-rolled prefix. Only uncompressed
    // 32-bit A8R8G8B8 is handled here (the pixel bytes are B,G,R,A, which is exactly the
    // texture upload format); the header is read as 32 little-endian u32 words:
    //   [0]='DDS ' [3]=dwHeight [4]=dwWidth [21]=ddspf.dwFourCC [22]=ddspf.dwRGBBitCount
    static renderengine::Texture2D* LoadFromDDS(LoadingScreenRenderer& lRenderer,
                                                renderengine::Texture2D* (LoadingScreenRenderer::*lpSetup)(f32, f32, void*, s32),
                                                const char* lpacPath)
    {
        std::FILE* lpFile = std::fopen(lpacPath, "rb");
        if (lpFile == nullptr) { return nullptr; }

        u32 luHeader[32] = {};   // 'DDS ' magic + the 124-byte DDS_HEADER
        if (std::fread(luHeader, sizeof(u32), 32, lpFile) != 32 || luHeader[0] != 0x20534444u)
        {
            std::fclose(lpFile);
            return nullptr;
        }

        const u32 luH = luHeader[3];   // dwHeight
        const u32 luW = luHeader[4];   // dwWidth
        const u32 luFourCC = luHeader[21];
        const u32 luBitCount = luHeader[22];
        if (luFourCC != 0u || luBitCount != 32u)   // not uncompressed 32-bit RGBA
        {
            std::fclose(lpFile);
            return nullptr;
        }

        const s32 liBytes = static_cast<s32>(luW * luH * 4u);
        void* lpData = std::malloc(static_cast<size_t>(liBytes));
        if (lpData != nullptr) { std::fread(lpData, 1u, static_cast<size_t>(liBytes), lpFile); }
        std::fclose(lpFile);
        if (lpData == nullptr) { return nullptr; }

        renderengine::Texture2D* lpTex = (lRenderer.*lpSetup)(static_cast<f32>(luW), static_cast<f32>(luH), lpData, liBytes);
        std::free(lpData);
        return lpTex;
    }

    // @ 0x823CE208 - create the textures, the re-usable scratch buffer, pick the language.
    void LoadingScreenRenderer::Construct()
    {
        mpArrowTexture = LoadFromDDS(*this, &LoadingScreenRenderer::SetupLoadingScreenTexture, "loadingscreen/arrow.dds");
        mpCarTexture   = LoadFromDDS(*this, &LoadingScreenRenderer::SetupLoadingScreenTexture, "loadingscreen/car.dds");
        mpTextTexture  = LoadFromDDS(*this, &LoadingScreenRenderer::SetupLoadingScreenTexture, "loadingscreen/text.dds");
        mpBoxTexture   = LoadFromDDS(*this, &LoadingScreenRenderer::SetupLoadingScreenTexture, "loadingscreen/box.dds");
        mpDiskErrorTexture = nullptr;

        static u8 saReusableData[0x200000];   // 2 MB scratch (X360 reused the car staging region)
        mReusableDataBuffer.Construct();
        mReusableDataBuffer.Create(saReusableData, sizeof(saReusableData));

        mbVisible = false;
        mbHiding = false;
        mbBlackOverlayVisible = false;
        mbBlackOverlayHiding = false;
        mbRenderInBackground = false;
        mfArrowRotation = 0.0f;
        mfArrowTranslation = 0.0f;
        // FLAG: ARTIST 0x823CE208 stores 1.0 (flt_82001C98) to mfArrowDirection (this+0x28);
        // mfFade is this+0x2C per the Render disasm at 0x823E7CE4. DecFIGS confirms
        // *(this+48)=1.0. Direction is the +/-1 sign of the arrow-translation bounce; 0.0f
        // would freeze the bounce. (Was 0.0f.)
        mfArrowDirection = 1.0f;
        mfFade = 0.0f;
        mfBlackOverlayFade = 0.0f;
        mfTimeStep = 0.0f;
        mfRotateSpeedInterp = 0.0f;
        muLastTime = 0;
        meLanguage = static_cast<ELoadingLanguage>(CgsSystem::HardwareSku::FindLanguage());
    }

    // @ 0x823AAD48 - map a command onto the visibility / black-overlay state.
    void LoadingScreenRenderer::AddCommand(ELoadingScreenCommand leCommand)
    {
        // PC bring-up trace: each command edge (the console's dispatch-slot observable).
        if (leCommand != E_LSC_NONE)
        {
            char lacMsg[64];
            std::snprintf(lacMsg, sizeof(lacMsg), "[LoadScreen] AddCommand %d\n", leCommand);
            CgsDev::Log::WriteToLog(lacMsg);
        }
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

        // ARTIST 0x823E7B28: the renderer returns before beginning an Im2d pass
        // when the loading screen's visible byte (this+0x14) is clear.
        if (!mbVisible)
        {
            return;
        }

        lpIm2d->BeginRendering();

        Im2dTransform lScreenXForm;
        lScreenXForm.TransformByAspectRatio();
        lpIm2d->SetTransform(lScreenXForm);
        lpIm2d->SetState(static_cast<const BlendState*>(nullptr));

        // --- Fade: one mfFade, two staged outputs (exactly as the source) -------------
        // mfFade ramps IN at KF_FADE_IN_SPEED, OUT at KF_FADE_OUT_SPEED, clamped to
        // lfFadeMax. X360 0x823E79C8: in save/load-background mode the cap is
        // KF_BLACK_FADE_POINT * 0.35 (= 0.175), which dims the background art to 35%
        // (0.175 / 0.5) with the foreground spinner/text alpha held at 0 - the dimmed
        // backdrop the autosave prompt draws over. The bg ("black") fade completes by
        // KF_BLACK_FADE_POINT, while the foreground alpha only begins at
        // KF_ALPHA_FADE_POINT - so the background reveals from black first, then the
        // spinner + text fade in. Element colours match the source: bg = (b,b,b,b);
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
        const RGBA8 lBgCol = { luBlack, luBlack, luBlack, luBlack };
        const RGBA8 lFgCol = { luBlack, luBlack, luBlack, luFgA };
        Vector2 laC[4];

        // Background / car: full-screen quad, sampling only the 1280x720 image region of
        // the 2048x1024 texture; greys up from black during the first fade stage.
        lpIm2d->SetTexture(mpCarTexture);
        EmitQuad(lpIm2d, {0.0f,0.0f}, {KF_SCREEN_W,0.0f}, {0.0f,KF_SCREEN_H}, {KF_SCREEN_W,KF_SCREEN_H},
                 0.0f, 0.0f, KF_BG_U, KF_BG_V, lBgCol);

        // Spinning arrow - the loading indicator. byte_82F241FC == 1 selects the arrow
        // over the alternate box indicator, so the box texture is intentionally not drawn.
        // The spin speed oscillates between KF_ROTATE_MIN/MAX_SPEED (here equal -> constant
        // 550 deg/s); advance the angle (wrapping at 180) and draw it rotated.
        mfRotateSpeedInterp += KF_ROTATE_ACCELERATION * mfTimeStep;
        if (mfRotateSpeedInterp > KF_TWO_PI) { mfRotateSpeedInterp -= KF_TWO_PI; }
        const f32 lfSpeed = (KF_ROTATE_MAX_SPEED - KF_ROTATE_MIN_SPEED)
                            * (std::sin(mfRotateSpeedInterp) + 1.0f) * 0.5f + KF_ROTATE_MIN_SPEED;
        mfArrowRotation += lfSpeed * mfTimeStep;
        if (mfArrowRotation > 180.0f) { mfArrowRotation -= 360.0f; }
        lpIm2d->SetTexture(mpArrowTexture);
        RotatedCorners(laC, KF_ARROW_CX, KF_ARROW_CY, KF_HALF_ARROW, KF_HALF_ARROW, mfArrowRotation * KF_DEG2RAD);
        EmitQuad(lpIm2d, laC[0], laC[1], laC[2], laC[3], 0.0f, 0.0f, KF_ARROW_UV, KF_ARROW_UV, lFgCol);

        // Text bar (tilted): sample only the current language's V-strip (flt_82FAE970).
        const s32 liLang = (meLanguage >= 0 && meLanguage < E_LOADINGLANGUAGE_COUNT) ? meLanguage : 0;
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
        Im2dTransform lScreenXForm;
        lScreenXForm.TransformByAspectRatio();
        lpIm2d->SetTransform(lScreenXForm);
        lpIm2d->SetState(static_cast<const BlendState*>(nullptr));

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
        EmitQuad(lpIm2d, {0.0f,0.0f}, {KF_SCREEN_W,0.0f}, {0.0f,KF_SCREEN_H}, {KF_SCREEN_W,KF_SCREEN_H},
                 0.0f, 0.0f, KF_BG_U, KF_BG_V, lBlack);
        lpIm2d->EndRendering();
    }

    // @ 0x823EDE18 - foreground path: advance the frame timestep, draw the loading
    // visuals (if not backgrounded and no overlay), then update the black overlay.
    void LoadingScreenRenderer::RenderForeground(CgsGraphics::Im2d* lpIm2d)
    {
        const u32 luFirstTime = CgsSystem::GetSystemTimerBaseTime();
        const u32 luSecondTime = CgsSystem::GetSystemTimerBaseTime();
        const u32 luDelta = luSecondTime - static_cast<u32>(muLastTime);
        muLastTime = luFirstTime;
        const u32 luFreq = CgsSystem::GetSystemTimerFrequency();
        f32 lfStep = (luFreq != 0u) ? static_cast<f32>(luDelta) / static_cast<f32>(luFreq) : 0.0f;
        if (lfStep > 0.1f) { lfStep = 0.1f; }
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

    // @ 0x823AAD48 (RenderDiskErrorMessage) - draw the disk-error texture full screen.
    void LoadingScreenRenderer::RenderDiskErrorMessage(CgsGraphics::Im2d* lpIm2d)
    {
        using namespace CgsGraphics;
        lpIm2d->BeginRendering();
        lpIm2d->SetState(static_cast<const BlendState*>(nullptr));
        lpIm2d->SetTexture(mpDiskErrorTexture);
        const RGBA8 lWhite = { 255, 255, 255, 255 };
        EmitQuad(lpIm2d, {0.0f,0.0f}, {KF_SCREEN_W,0.0f}, {0.0f,KF_SCREEN_H}, {KF_SCREEN_W,KF_SCREEN_H},
                 0.0f, 0.0f, 1.0f, 1.0f, lWhite);
        lpIm2d->EndRendering();
    }
}
