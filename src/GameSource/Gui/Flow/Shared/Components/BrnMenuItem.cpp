#include "GameSource/Gui/Flow/Shared/Components/BrnMenuItem.h"
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnMenuItem.cpp"
#include <cstring>                                        // std::memset
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf

// BrnGui::MenuItem -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (4 ledger functions, DWARF primary file
// GameSource/Gui/Flow/Shared/Components/BrnMenuItem.cpp):
//   MenuItem::Clear   @0x824E2E90    MenuItem::Set    @0x824E2F58
//   MenuItem::SetText @0x824E3030    MenuItem::Update @0x824E6490

namespace BrnGui
{

// X360 .data @0x82F27430 (XEX-recovered pointers/strings). The UNUSED entry
// aliases the same "Invisible" string as INVISIBLE on the console.
const char* MenuItem::KAC_STATE_NAMES[MenuItem::E_MENUITEMSTATES_COUNT] =
{
    "Invisible",    // E_MENUITEMSTATES_UNUSED (aliases INVISIBLE's string)
    "Invisible",    // E_MENUITEMSTATES_INVISIBLE
    "Disabled",     // E_MENUITEMSTATES_DISABLED
    "Unselected",   // E_MENUITEMSTATES_UNHIGHLIGHTED
    "Selected",     // E_MENUITEMSTATES_HIGHLIGHTED
};

// @ 0x824E2E90 -- wipe the label, invalidate the id, reset the selectable gates
// (active off, highlightable/selectable on, highlighted off; each dispatched
// through the live vtable exactly as the X360 does), and dirty the row. The X360
// raises the dirty bit both before and after the virtual resets -- order kept.
void MenuItem::Clear()
{
    std::memset(macItemText, 0, KU_MAX_NAME_LENGTH);
    SetDirty();
    SetId(K_INVALID_ID);   // the full 64-bit slot: low word -1, high (valid-tag) word 0
    SetActive(false);          // vtbl slot 0
    SetHighlightable(true);    // vtbl slot 1
    SetHighlighted(false);     // vtbl slot 3 (X360 call order: after Highlightable)
    SetSelectable(true);       // vtbl slot 2
    SetDirty();
    mbUpdateText = false;
}

// @ 0x824E2F58 -- cpp:101. Adopt a label + id. The id store is a plain sign
// extension of the s32 id into the 64-bit slot (asm `extsw r10,r26; std` -- the
// pseudocode's `| 0x100000000` is wrong).
void MenuItem::Set(const char* lpacText, s32 liId)
{
    CGS_ASSERT(lpacText != 0, CGS_ASSERT_STREAM_MESSAGE_AT("invalid text specified", CGS_ARTIST_FILE, 101));   // :101 (streamed on the X360; folded static)

    CgsCore::SPrintf(macItemText, KU_MAX_NAME_LENGTH, "%s", lpacText);
    SetDirty();
    SetId(static_cast<u64>(static_cast<s64>(liId)));   // extsw: sign-extended id
    mbUpdateText = true;
}

// @ 0x824E3030 -- cpp:119. Retext the label without touching the id or the dirty
// bit; the update-text latch re-pushes just the label next Update.
void MenuItem::SetText(const char* lpacText)
{
    CGS_ASSERT(lpacText != 0, CGS_ASSERT_STREAM_MESSAGE_AT("invalid text specified", CGS_ARTIST_FILE, 119));   // :119

    CgsCore::SPrintf(macItemText, KU_MAX_NAME_LENGTH, "%s", lpacText);
    mbUpdateText = true;
}

// @ 0x824E6490 -- consume the dirty flag (full label + state + updatestate push),
// else the update-text latch (label-only push).
void MenuItem::Update()
{
    if (IsDirty())
    {
        ClearFlag(E_FLAG_DIRTY);

        // DWARF local (BrnMenuItem.cpp:144); FindState is inlined here in ARTIST 0x824E6490.
        MenuItemStates leState = FindState();

        AddOutputAptViewState("apt_labeltxt", macItemText, false);
        AddOutputAptViewState("apt_state", KAC_STATE_NAMES[leState], false);
        AddOutputAptViewState("apt_updatestate", "1", false);
        mbUpdateText = false;
    }

    if (mbUpdateText)
    {
        AddOutputAptViewState("apt_labeltxt", macItemText, false);
        AddOutputAptViewState("apt_updatestate", "0", false);
        mbUpdateText = false;
    }
}

// DWARF BrnMenuItem.cpp:~170 (local leState cpp:172): the item's apt state from its flags.
MenuItem::MenuItemStates MenuItem::FindState() const
{
    MenuItemStates leState;
    if (!IsActive())
    {
        leState = E_MENUITEMSTATES_INVISIBLE;
    }
    else if (!IsSelectable())
    {
        leState = E_MENUITEMSTATES_DISABLED;
    }
    else if (IsHighlighted())
    {
        leState = E_MENUITEMSTATES_HIGHLIGHTED;
    }
    else
    {
        leState = E_MENUITEMSTATES_UNHIGHLIGHTED;
    }
    return leState;
}

}

#undef CGS_ARTIST_FILE
