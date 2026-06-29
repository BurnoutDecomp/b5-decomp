#ifndef BRN_CREDITS_TEXT_RENDERER_H
#define BRN_CREDITS_TEXT_RENDERER_H

#include "types.hpp"
#include "BrnCommonTypes.h"                                             // Vector4 / CgsID
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsIm2dTransform.h" // CgsGraphics::Im2dTransform
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h"       // CgsGraphics::TextObject / TextRenderer / Im2dRenderBuffer
#include "GameShared/GameClasses/Fonts/CgsFont.h"                        // CgsResource::Font / SafeResourceHandle / CgsUtf8

// BrnGui::CreditsTextRenderer - the custom GUI renderer that scrolls the end / replay
// credits up the screen with a Y-based fade at top and bottom. It builds a paragraph
// list from the localisation database (CREDITS_TITLE_%d / CREDITS_DETAIL_%d, or the
// REPLAY_CREDITS_* keys), measures each paragraph with the font path, then each frame
// scrolls the list and draws the in-view paragraphs through an Im2d render buffer using
// two TextObjects (one styled for titles, one for detail body text).
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX (eight exported member functions):
//   CreditsTextRenderer::Construct             @ 0x8244EA38
//   CreditsTextRenderer::GetID                 @ 0x824471A0
//   CreditsTextRenderer::Prepare               @ 0x82447190
//   CreditsTextRenderer::SetTextRenderer       @ 0x824471B0
//   CreditsTextRenderer::SetRenderEnabled      @ 0x82457BB8
//   CreditsTextRenderer::Update                @ 0x824575F0
//   CreditsTextRenderer::RecalculateParagraphs @ 0x8244EAB0
//   CreditsTextRenderer::RenderComponent       @ 0x8245DE40
//
// LAYOUT: members are declared by name in the DWARF (BrnCreditsTextRenderer.h) order.
// Byte offsets are NOT load-bearing on the 64-bit host (the X360 pointers/handles widen),
// so no raw-offset casts are used; the bodies access members by name. The X360 guest
// offsets that pin the field order are noted in the comments. FLAG: the leading vtable
// slot is modelled as an opaque pointer (this renderer derives from the GUI custom-render
// base; only the members the eight bodied functions touch are needed here, so the base
// is folded to its leading { vtable; bool mbRenderEnabled } shape rather than pulling in
// the uncommitted CustomRenderComponentInterface dependency web).

namespace CgsLanguage { class LanguageManager; }
namespace renderengine { class TextureState; }

namespace BrnGui
{
    // The immediate-renderer set passed to RenderComponent. The X360 reads the leading
    // dword (the Im2d render buffer the credits draw into) and reaches the buffer's batch
    // API at buffer+4 (on the PC fold Im2dRenderBuffer == Im2d, so that is the Im2d
    // itself). Only the leading buffer pointer is in scope here.
    struct ImRendererSet
    {
        CgsGraphics::Im2dRenderBuffer* mpIm2dRenderBuffer; // guest +0x00
    };

    class CreditsTextRenderer
    {
    public:
        // BrnCreditsTextRenderer.h:60 -- paragraph-list capacity (DWARF KI_MAX_PARAGRAPHS).
        static const s32 KI_MAX_PARAGRAPHS = 500;

        // One scrolling paragraph: its rendered height, its scroll position, the localised
        // text, and whether it is a title (vs. detail body). DWARF BrnCreditsTextRenderer.h:52.
        // 16-byte stride (the Construct memset clears KI_MAX_PARAGRAPHS * 16 == 8000 bytes).
        struct ParagraphInfo
        {
            f32                         mfHeight;   // guest +0x00
            f32                         mfPosition; // guest +0x04
            const CgsResource::CgsUtf8* mpText;     // guest +0x08
            bool                        mbTitle;    // guest +0x0C
        };

        // E_CREDITS_TYPE (guest +0x20C4): which key set the paragraphs are built from. 0 =
        // end credits (CREDITS_TITLE_%d / CREDITS_DETAIL_%d), 1 = replay credits
        // (REPLAY_CREDITS_TITLE_%d / REPLAY_CREDITS_DETAIL_%d). Any other value asserts.
        enum ECreditsType
        {
            E_CREDITS_TYPE_END    = 0,
            E_CREDITS_TYPE_REPLAY = 1,
        };

        // 0x8244EA38 -- copy the default (invalid) font handle into both font handles, clear
        // the credits type + the paragraph count, count the entries of a static name table,
        // then zero the paragraph array.
        void Construct();

        // 0x82447190 -- store the supplied heap allocator and report success. (The two event/
        // resource-allocator args the X360 ignores are kept for the GUI prepare signature.)
        bool Prepare(rw::IResourceAllocator* lpHeapAllocator);

        // 0x824471A0 -- the renderer id ("CREDITS").
        CgsID GetID() const;

        // 0x824471B0 -- store the shared text renderer.
        void SetTextRenderer(CgsGraphics::TextRenderer* lpTextRenderer);

        // 0x82457BB8 -- store the enabled flag; when enabling, rebuild the paragraph list and
        // reset the scroll/fade to their start values.
        void SetRenderEnabled(bool lbRenderEnabled);

        // 0x824575F0 -- advance the scroll + fade-in while enabled, wrap the scroll once the
        // last paragraph leaves the top, and clamp the fade to 1.0. Also rebuilds the screen
        // transform from the credits text box (rotated + aspect-corrected).
        void Update();

        // 0x8245DE40 -- rebuild the paragraphs, then (if faded in) draw every in-view
        // paragraph twice: once as a dropped/offset shadow pass and once as the main pass,
        // each through TextRenderer::RenderStringFadingY so the top/bottom of the column fade.
        void RenderComponent(ImRendererSet* lpRendererSet);

    private:
        // 0x8244EAB0 -- (re)build the paragraph list from the localisation database: for each
        // index look up the title + detail strings, measure their line counts/heights with the
        // two TextObjects, and accumulate their scroll positions. Stops at the first missing
        // pair (end credits) / after the fixed replay set.
        void RecalculateParagraphs();

        void*                                      mpVtable;                         // guest +0x00
        bool                                       mbRenderEnabled;                  // guest +0x04 (base)
        rw::IResourceAllocator*                    mpHeapAllocator;                  // guest +0x08
        CgsGraphics::Im2dTransform                 mScreenTransform;                 // guest +0x10
        CgsGraphics::TextRenderer*                 mpTextRenderer;                   // guest +0x50
        CgsLanguage::LanguageManager*              mpLanguageManager;                // guest +0x54
        CgsResource::SafeResourceHandle<CgsResource::Font> mpNormalFont;             // guest +0x58
        CgsResource::SafeResourceHandle<CgsResource::Font> mpTitleFont;              // guest +0x60
        s32                                        miNumStrings;                     // guest +0x68 (paragraph count)
        ParagraphInfo                              maParagraphs[KI_MAX_PARAGRAPHS];  // guest +0x6C (KI_MAX_PARAGRAPHS * 16 == 8000)
        CgsGraphics::TextObject                    mTitleTextObject;                 // guest +0x1FAC
        CgsGraphics::TextObject                    mNormalTextObject;                // guest +0x2028
        f32                                        mfScroll;                         // guest +0x20A4
        f32                                        mfFade;                           // guest +0x20A8
        ECreditsType                               meCreditsType;                    // guest +0x20C4
        // Trailing background-mask state (DWARF mBackgroundMaskTextureStateResource /
        // mpBackgroundMaskTextureState). Not touched by any of the eight bodied functions
        // (the Prepare/Release/Destruct that set them are not exported in this TU); kept by
        // name so the object is the right shape.
        rw::Resource                               mBackgroundMaskTextureStateResource;
        renderengine::TextureState*                mpBackgroundMaskTextureState;
    };
}

#endif // BRN_CREDITS_TEXT_RENDERER_H
