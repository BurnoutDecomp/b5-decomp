#ifndef BRN_IN_GAME_MESSAGE_RENDERER_H
#define BRN_IN_GAME_MESSAGE_RENDERER_H

#include "types.hpp"

// BrnGui::InGameMessageRenderer - the custom HUD renderer that draws the in-game
// message banner (e.g. event prompts, takedown text). Holds a back-pointer to the
// shared language manager so it can localise / re-layout the message.
//
// Reconstructed from BURNOUT_X360_ARTIST.XEX:
//   BrnGui::InGameMessageRenderer::SetLanguageManager @ 0x82443FB0
//
// MINIMAL-SLICE class: only SetLanguageManager is in scope, so this models ONLY the
// one member it writes -- the language-manager pointer the guest stores at +0x1388.
// The full renderer object (its CustomRenderComponentInterface base, glyph/layout
// state, the Im2d draw machinery, etc.) is uncommitted and intentionally OMITTED;
// nothing in scope references it. FLAG: minimal-slice class. The exact +0x1388 byte
// offset is not load-bearing on the 64-bit host (pointer widening), so the member is
// declared by name, not at a raw offset.

namespace CgsLanguage { class LanguageManager; }

namespace BrnGui
{
    class InGameMessageRenderer
    {
    public:
        // 0x82443FB0 -- store the supplied language manager, then, if the active
        // language is the wide-glyph language (id 16), nudge the message banner's
        // on-screen position globals (X = 28.0, Y = 629.0). Asserts the pointer is
        // non-null. Returns the X360 r3 result (the EndAssert pointer on the assert
        // path, else `this`); kept as void* to mirror the guest's _DWORD* return.
        void* SetLanguageManager(CgsLanguage::LanguageManager* lpLanguageManager);

    private:
        // Guest +0x1388 (dword 1250): the shared language manager back-pointer.
        CgsLanguage::LanguageManager* mpLanguageManager;
    };
}

#endif
