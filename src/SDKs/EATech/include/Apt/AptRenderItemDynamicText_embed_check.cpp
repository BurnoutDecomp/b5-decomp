// Compile-only embed check for AptRenderItemDynamicText.h -- verifies the dynamic
// text render item's named members (the +0x34..+0x6C fields the AptCharacterTextInst
// facade + the hitTest/getBounds/text natives read/write, including mpTextFormat
// @ +0x68 and mStateFlags @ +0x6C) are reachable and the inline accessors compile.
// (AptRenderItemDynamicText is constructed only by the manager; exercise via ptr.)
// Not a runtime TU.

#include "SDKs/EATech/include/Apt/AptRenderItemDynamicText.h"

namespace
{
    // FLAG PC-platform leaf: ODR/embed-check scaffolding (no console counterpart).
    void AptRenderItemDynamicText_EmbedCheck(AptRenderItemDynamicText* p)
    {
        // text / variable strings
        (void)p->GetTextValueConst();
        (void)p->GetVarValueConst();

        // colours + flags (packed-word accessors)
        (void)p->GetTextColor();
        (void)p->GetBackgroundColor();
        p->SetBackgroundColor(0x00112233u);
        (void)p->GetBorderColor();
        (void)p->GetAlignment();
        p->SetAlignment(1);
        (void)p->GetDrawsBackground();
        (void)p->GetMultiline();
        (void)p->GetWordWrap();

        // bounds + font + state
        (void)p->GetBoundsConst();
        (void)p->GetFontSize();
        (void)p->GetFontID();
        p->SetStateFlags(2u);
        p->ClearStateFlags(1u);

        // text-format object pass-through (mpTextFormat @ +0x68)
        (void)p->GetTextFormatConst();
    }
}
