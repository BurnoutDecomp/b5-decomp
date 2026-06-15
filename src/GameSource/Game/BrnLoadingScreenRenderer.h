#pragma once

#include "types.hpp"
#include "pc/gcm/renderengine/texture.h"

namespace CgsGraphics { struct Im2d; }

// BrnGame::LoadingScreenRenderer - draws the boot/loading screen (animated arrow over
// a background, plus a fading black overlay) on its own render path so it can animate
// while the main thread streams. Layout/enum from the DecFIGS DWARF
// (BrnLoadingScreenRenderer.h); the LinearMalloc scratch buffer is reconstructed as a
// minimal stand-in (it is not touched by the functions in this TU).
namespace BrnGame
{
    // Minimal stand-in for the X360 LinearMalloc scratch allocator member (unused by
    // this TU's functions; full layout not recovered).
    struct LinearMalloc
    {
        void* mpBase;
        u32   muSize;
        u32   muOffset;
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

        void AddCommand(s32 liCommand);
        void RenderBackground();
        void RenderForeground(CgsGraphics::Im2d* lpIm2d);
        void RenderBlackOverlay(CgsGraphics::Im2d* lpIm2d);
        renderengine::Texture* SetupLoadingScreenTexture(f32 lfWidth, f32 lfHeight, s32 liUnused0,
                                                         s32 liUnused1, s32 liUnused2,
                                                         const void* lpPixelData, u32 luDataSize);
        void Render();   // (separate TU; declared so the in-flow renderers can call it)

    private:
        renderengine::Texture2D* mpArrowTexture;
        renderengine::Texture2D* mpCarTexture;
        renderengine::Texture2D* mpTextTexture;
        renderengine::Texture2D* mpBoxTexture;
        ELoadingLanguage         meLanguage;
        renderengine::Texture2D* mpDiskErrorTexture;
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
        LinearMalloc             mReusableDataBuffer;
        f32                      mfTimeStep;
        f32                      mfRotateSpeedInterp;
    };
}
