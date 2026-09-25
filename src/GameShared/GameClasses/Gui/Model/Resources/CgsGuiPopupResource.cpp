#include "GameShared/GameClasses/Gui/Model/Resources/CgsGuiPopupResource.h"

// CgsGui::GuiPopupResource -- the load-time relocation pair for the Popups.pup table.
//
// FixUp rebases the table pointer by the load base, then walks miPopupCount entries and
// rebases each record pointer. FixDown is the exact inverse, entries first and the table
// pointer last (the entries are read through the still-rebased table).
//
// The console steps the table with a 4-byte stride (its pointer width). The shipped
// POPUPS.PUP is transcoded to 8-byte slots, so the stride here is the host pointer stride,
// written as an array subscript on the real member type -- the same treatment as
// GuiHudMessageResource::FixUp.

namespace CgsGui
{
    GuiPopupResource* GuiPopupResource::FixUp(uintptr_t luDelta)
    {
        // The stored value is a serialised offset, not yet a pointer.
        mppPopupData = reinterpret_cast<GuiPopup**>(
            reinterpret_cast<uintptr_t>(mppPopupData) + luDelta);

        for (s32 liEntry = 0; liEntry < miPopupCount; ++liEntry)
        {
            mppPopupData[liEntry] = reinterpret_cast<GuiPopup*>(
                reinterpret_cast<uintptr_t>(mppPopupData[liEntry]) + luDelta);
        }
        return this;
    }

    GuiPopupResource* GuiPopupResource::FixDown(uintptr_t luDelta, bool lbDeep)
    {
        for (s32 liEntry = 0; liEntry < miPopupCount; ++liEntry)
        {
            if (lbDeep)
            {
                // The deep pass walks the record's miMessageParamsUsed message params
                // (maeMessageParams, 4-byte stride) and stores nothing: the params are
                // enums with nothing to relocate.
                const GuiPopup* lpPopup = mppPopupData[liEntry];
                for (s32 liParam = 0; liParam < lpPopup->miMessageParamsUsed; ++liParam)
                {
                }
            }
            mppPopupData[liEntry] = reinterpret_cast<GuiPopup*>(
                reinterpret_cast<uintptr_t>(mppPopupData[liEntry]) - luDelta);
        }

        mppPopupData = reinterpret_cast<GuiPopup**>(
            reinterpret_cast<uintptr_t>(mppPopupData) - luDelta);
        return this;
    }
}
