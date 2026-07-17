#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Fonts/CgsUnicode.h"            // CgsUnicode::CgsUtf8 (Prepare/SetText/GetText)
#include "GameShared/GameClasses/Graphics/Font/CgsFontRenderer.h" // CgsGraphics::TextObject (the embedded text object)

// Pointer-only collaborators (forward-declared to avoid a transitive header cascade;
// only pointers/references are used here):
struct RGBA;                          // alternate-text-colour table referent (opaque pass-through)
struct AptAllocateStringParameters;   // filled by the caller, consumed by Prepare
namespace CgsGui      { struct FontCollection; }
namespace CgsLanguage { class  LanguageManager; }

namespace CgsGui
{
    // Resolve the language manager the Apt render handler holds, reached the way the console
    // Prepare does: through the AptAux singleton (AptAuxPointer::mpAptAuxInst -> mRenderHandler
    // -> mpLanguageManager). The full AptAux / AptRenderHandler layout (CgsAptRenderHandler.h)
    // embeds a 256-slot opaque pool CgsAptString stand-in, so that header CANNOT be pulled into
    // this REAL-CgsAptString TU (an ODR clash on CgsGui::CgsAptString -- see CgsAptRenderHandler.h
    // line 181). This thin accessor is bodied in CgsAptAux.cpp (which owns those types) so Prepare
    // can reach the manager without the cascade. Asserts the singleton + handler are live.
    CgsLanguage::LanguageManager* GetAptRenderHandlerLanguageManager();
}

// CgsGui::CgsAptString - the apt/Flash text-object wrapper a BrnFlapt::TextFieldRef
// reaches through. It EMBEDS a CgsGraphics::TextObject (mTextObject) as its first member
// and adds the per-string alternate-text-colour table the pool was built with.
//
// LAYOUT (recovered cross-build): the PS3 External ELF names every field access as
// `a1->mTextObject.<field>` (CgsAptString::Prepare @0x5C6B20, Reset @ its Reset()), and
// the embedded TextObject spans the leading 0x7C bytes (its cached text pointer is at
// TextObject +0x74); CgsAptString then stores mpAlternateTextColours @ +0x7C and
// miNumAlternateColours @ +0x80 (the two words Reset/Construct feed to
// TextObject::Construct). X360 GetText @0x8246D9A0 reads the leading font-handle pair
// (mTextObject.mpFont +0x00/+0x04) and the cached text pointer (+0x74), asserting the
// font handle is no longer the empty/default sentinel before handing the text back.
//
// Members are accessed BY NAME; the x64 layout differs in pointer width (the embedded
// SafeResourceHandle<Font> widens), so no byte-size static_assert is asserted (the X360
// offsets are recorded in comments per the project's x64 semantic-parity rule).
namespace CgsGui
{
    class CgsAptString
    {
    public:
        // The cached resolved text handle GetText hands back. The X360 returns a 32-bit
        // pointer it caches in the embedded TextObject (mTextObject.mpUtf8String @ +0x74);
        // modelled as the same UTF-8 pointer here.
        typedef const CgsUnicode::CgsUtf8* TextHandle;

        // Text-effect mode passed to Prepare (DWARF CgsAptString.h:66). The flapt text
        // path requests E_EFFECT_NONE.
        enum ETextEffects
        {
            E_EFFECT_NONE       = 0,
            E_EFFECT_DROPSHADOW = 1,
            E_EFFECT_GRADIENT   = 2,
            E_EFFECT_EMBOSSED   = 3,
            E_EFFECT_ALL        = 4
        };

        // @ 0x82851EF8 (X360 Construct) / PS3 Reset @ its Reset(): construct the embedded
        // text object from the cached alternate-colour table (mpAlternateTextColours /
        // miNumAlternateColours). DWARF CgsAptString.h:81. The colour table is taken as the
        // opaque global RGBA* every caller (the flapt text field / the render-handler pool)
        // hands in; it is forwarded to TextObject::Construct (which packs it as RGBA8 words).
        void Construct(const RGBA* lpColours, s32 liNumColours);

        // PS3 Reset(): re-construct the embedded text object from the cached alt-colour
        // table. (Prepare calls this first.) Same body as the console Reset.
        void Reset();

        // @ 0x82854A40 - lay the parametrised string (lpParams->szString) out into
        // lpStringBuffer using lpFonts, the requested effect, and a size scale, and
        // cache the result on this string object. DWARF CgsAptString.h:91.
        void Prepare(const FontCollection* lpFonts,
                     AptAllocateStringParameters* lpParams,
                     CgsUnicode::CgsUtf8* lpStringBuffer,
                     ETextEffects eEffect,
                     f32 lfSizeScale);

        // @ 0x8246D9A0 - assert the object has been set up (its embedded font handle is no
        // longer the un-set-up sentinel pair) and return the cached text handle.
        TextHandle GetText() const;

        // @ 0x82855648 - re-point the cached text at lpNewText and re-measure. Unless
        // lbAlreadyLocalised, a "$"/"~"-prefixed string is first resolved through the
        // apt render handler's language manager (falling back to the raw text when the
        // id is unknown); non-"~" results are then copied into the caller's persistent
        // lpcStringBuffer (256-byte cap). Asserts the resolved text is valid UTF-8 and
        // that the font has been set up, recalculating autosizing after every text /
        // width store, exactly as the X360 body does. DWARF CgsAptString.h:98.
        void SetText(const CgsUnicode::CgsUtf8* lpNewText,
                     CgsUnicode::CgsUtf8* lpcStringBuffer,
                     bool lbAlreadyLocalised);

    public:
        // The embedded text object. Its leading SafeResourceHandle<Font> (mpFont +0x00/+0x04)
        // is the identity/type-tag pair GetText asserts against; its cached text pointer
        // (mpUtf8String +0x74) is the handle GetText returns. [c:+0x00 .. +0x7B]
        CgsGraphics::TextObject mTextObject;

        // The alternate-text-colour table the pool was built with; Reset/Construct feed it to
        // TextObject::Construct. DWARF CgsAptString.h:106/107. Held as the opaque global RGBA*
        // the callers pass (reinterpret-cast to the TextObject's RGBA8 word type on forward).
        const RGBA* mpAlternateTextColours;   // [c:+0x7C]
        s32         miNumAlternateColours;    // [c:+0x80]
    };
}
