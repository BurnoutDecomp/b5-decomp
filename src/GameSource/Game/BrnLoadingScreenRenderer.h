#pragma once

#include "types.hpp"
#include "SDKs/RenderEngineClub/MAIN/components/src/texture2d.h"
#include "GameShared/GameClasses/Memory/CgsLinearMalloc.h"
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsBasic2dColouredTexturedVertex.h"

namespace CgsGraphics { struct Im2d; }

// BrnGame::LoadingScreenRenderer - draws the boot/loading screen (animated arrow over
// a background, plus a fading black overlay) on its own render path so it can animate
// while the main thread streams. ARTIST fixes the 0x60-byte native layout;
// DecFIGS supplies names. Its PS3-only disk-error member is not present on X360.
namespace BrnGame
{
    enum ELoadingScreenCommand
    {
        E_LSC_NONE           = 0,
        E_LSC_SHOW           = 1,
        E_LSC_HIDE           = 2,
        E_LSC_SHOWSAVELOADBG = 3,
        E_LSC_BLACKFADEIN    = 4,
        E_LSC_BLACKFADEOUT   = 5,
        E_LSC_COUNT          = 6,
    };

    struct LoadingScreenRenderer
    {
        enum ELoadingLanguage
        {
            E_LOADINGLANGUAGE_ENGLISH  = 0,
            E_LOADINGLANGUAGE_JAPANESE = 1,
            E_LOADINGLANGUAGE_FRENCH   = 2,
            E_LOADINGLANGUAGE_GERMAN   = 3,
            E_LOADINGLANGUAGE_SPANISH  = 4,
            E_LOADINGLANGUAGE_ITALIAN  = 5,
            E_LOADINGLANGUAGE_COUNT    = 6,
        };

        void Construct();
        void AddCommand(ELoadingScreenCommand leCommand);
        CgsMemory::LinearMalloc* GetReusableDataAllocator() { return &mReusableDataBuffer; }
        void RenderForeground(CgsGraphics::Im2d* lpIm2dRenderBuffer);
        void RenderBackground(CgsGraphics::Im2d* lpIm2dRenderBuffer);
        bool IsRenderingLoadingScreen() { return mbVisible; }
        // [loading-fade seat 2026-08-27] the foreground hide-fade-in-flight face the movie
        // presentation gate reads (mbVisible stays true through the whole 0.5s hide fade;
        // the save/load-background mode is excluded so an in-game video over the dimmed
        // backdrop is never held). ADDITIVE accessor over existing members.
        bool IsForegroundHideFadePending() const { return mbVisible && !mbRenderInBackground; }

    private:
        renderengine::Texture2D* SetupLoadingScreenTexture(f32 lfWidth, f32 lfHeight,
                                                           void* lpcData, s32 liDataSize);
        CgsGraphics::Vector2 RotatePointAroundAngle(const CgsGraphics::Vector2& lPoint,
                                                    f32 lfSin, f32 lfCos);
        void Render(CgsGraphics::Im2d* lpIm2dRenderBuffer);
        void RenderBlackOverlay(CgsGraphics::Im2d* lpIm2dRenderBuffer);

        renderengine::Texture2D* mpArrowTexture;
        renderengine::Texture2D* mpCarTexture;
        renderengine::Texture2D* mpTextTexture;
        renderengine::Texture2D* mpBoxTexture;
        ELoadingLanguage         meLanguage;
        bool                     mbVisible;
        bool                     mbHiding;
        bool                     mbBlackOverlayVisible;
        bool                     mbBlackOverlayHiding;
        u64                      muLastTime;
        f32                      mfArrowRotation;
        f32                      mfArrowTranslation;
        f32                      mfArrowDirection;
        f32                      mfFade;
        bool                     mbRenderInBackground;
        f32                      mfBlackOverlayFade;
        bool                     mbKillBlackOverlayWhenDone;
        CgsMemory::LinearMalloc  mReusableDataBuffer;
        f32                      mfTimeStep;
        f32                      mfRotateSpeedInterp;
    };
    CGS_STATIC_ASSERT((sizeof(LoadingScreenRenderer) == 0x60), "ARTIST loading renderer");
}
