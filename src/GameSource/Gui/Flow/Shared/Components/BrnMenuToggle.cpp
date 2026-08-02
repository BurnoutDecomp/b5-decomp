// ===================================================================================
// BrnGui::MenuToggle -- implementation
//   class:BrnGui::MenuToggle
//
// Reconstructed store-for-store from the X360 ARTIST build:
//   Construct                  @ 0x824E8938   Update            @ 0x824E8AB0
//   SetupMenuToggle            @ 0x824EA0A0   Unloaded          @ 0x824E55D0
//   Clear                      @ 0x824E55E0   Select            @ 0x824E55F8
//   AppendExpectedAptComponent @ 0x824E5610   GetHighlightedId  @ 0x82489080
//   HighlightNext              @ 0x82482860   HighlightPrevious @ 0x824828D8
//
// 2026-08-02: the three cursor methods used to reach the row's option group through a raw
// `*(void***)this` flat vtable. Nothing in this tree ever writes a vtable word into a
// modelled component head, so that dispatch would have jumped through an uninitialised
// pointer the first time it ran. The class is now re-homed onto BrnGui::Selectable (see
// the header) and every call below is by name.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include "GameSource/Gui/BrnGuiCache.h"              // BrnGui::GuiCache

namespace BrnGui
{
    // off_82F27460 -- the toggle's per-state apt view-state names. Only the ladder is
    // binary-attested (Update indexes 1..4); the strings are the shared component
    // vocabulary BrnGui::MenuItem and BrnGui::ColourMenuToggle already carry.
    const char* const MenuToggle::KAC_STATE_NAMES[MenuToggle::E_MENUTOGGLESTATES_COUNT] =
    {
        "Invisible",       // [0] E_MENUTOGGLESTATES_UNUSED (aliases INVISIBLE)
        "Invisible",       // [1] E_MENUTOGGLESTATES_INVISIBLE
        "Disabled",        // [2] E_MENUTOGGLESTATES_DISABLED
        "Unhighlighted",   // [3] E_MENUTOGGLESTATES_UNHIGHLIGHTED
        "Highlighted",     // [4] E_MENUTOGGLESTATES_HIGHLIGHTED
    };

    // ---- Construct @ 0x824E8938 -----------------------------------------------------
    void MenuToggle::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                               const char* lpacParentName, u64 luAptId)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name");                      // cpp:59 (streamed)
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface");   // cpp:60 (streamed)

        // X360: CgsGui::GuiComponent::Construct(this + 0x18, name, stateInterface, parent).
        mGuiComponentBase.Construct(lpacName, lpStateInterface, lpacParentName);

        SetId(luAptId);   // std r19, 0x10(this)
        ClearFlags();     // stb 0,   0x0C(this)

        // Both children are parented on THIS row's own name buffer (X360 passes this + 0x1C).
        const char* lpacChildParent = mGuiComponentBase.GetName();

        mItemText.Construct("ItemText", lpStateInterface, lpacChildParent,
                            static_cast<u64>(-1));
        mTitleText.Construct("TitleText", lpStateInterface, lpacChildParent);

        mbLoaded = true;   // stb 1, 0xE8E(this)
    }

    // ---- AppendExpectedAptComponent @ 0x824E5610 ------------------------------------
    // The X360 signature is (this, flow, cache, withTitle) and it calls
    // GuiCache::AppendExpectedAptComponent(cache, flow, <name>) three times: the row's own
    // component name (this + 0x1C), the caption field's (this + 0xD6C) when the flag is set,
    // and the option selection's (this + 0xC4).
    void MenuToggle::AppendExpectedAptComponent(GuiFlow leFlow, GuiCache* lpGuiCache,
                                                bool lbWithTitle)
    {
        lpGuiCache->AppendExpectedAptComponent(leFlow, mGuiComponentBase.GetName());
        if (lbWithTitle)
        {
            lpGuiCache->AppendExpectedAptComponent(leFlow, mTitleText.GetName());
        }
        lpGuiCache->AppendExpectedAptComponent(leFlow, mItemText.GetName());
    }

    // ---- SetupMenuToggle @ 0x824EA0A0 -----------------------------------------------
    // The caption goes to the title field (the pooled empty string when none is supplied),
    // the option payload straight to the selection, then the row is dirtied.
    void MenuToggle::SetupMenuToggle(s32 liNumOptions, bool lbWrapped, const char* lpacText,
                                     const char** lppacOptions, u64* lpu64Ids)
    {
        mTitleText.SetText(lpacText ? lpacText : "");   // unk_820046A7 == ""
        mItemText.SetupTextSelection(liNumOptions, lbWrapped, lppacOptions, lpu64Ids);
        SetDirty();                                     // *(this+0xC) |= 0x10
    }

    // ---- Clear @ 0x824E55E0 ---------------------------------------------------------
    // Tail-call of the option selection's component clear (its slot 6).
    void MenuToggle::Clear()
    {
        mItemText.Clear();
    }

    // ---- Unloaded @ 0x824E55D0 ------------------------------------------------------
    void MenuToggle::Unloaded()
    {
        miLoadedItems = 0;   // stb 0, 0xE90(this)
    }

    // ---- Select @ 0x824E55F8 --------------------------------------------------------
    // Tail-call of the option selection's Select (its slot 4).
    void MenuToggle::Select()
    {
        mItemText.Select();
    }

    // ---- Update @ 0x824E8AB0 --------------------------------------------------------
    // Lazy apt refresh: when the dirty bit is set, clear it, resolve the view state off the
    // flag byte (bit0 active / bit2 selectable-enabled / bit3 highlighted), push it, then
    // update the option selection. When not dirty, do nothing.
    void MenuToggle::Update()
    {
        if (!IsDirty())
        {
            return;
        }

        ClearFlag(E_FLAG_DIRTY);   // X360 `xori 0x10` -- the bit was set, so xor == clear

        MenuToggleStates leState;
        if (!IsActive())
        {
            leState = E_MENUTOGGLESTATES_INVISIBLE;
        }
        else if (!IsSelectable())
        {
            leState = E_MENUTOGGLESTATES_DISABLED;
        }
        else
        {
            // X360: `((_cntlzw(flags & 8) & 0x20) == 0) + 3` -- 4 when bit 3 is set, else 3.
            leState = IsHighlighted() ? E_MENUTOGGLESTATES_HIGHLIGHTED
                                      : E_MENUTOGGLESTATES_UNHIGHLIGHTED;
        }

        // X360: CgsGui::GuiComponent::AddOutputAptViewState(this + 0x18, ...).
        mGuiComponentBase.AddOutputAptViewState("apt_state", KAC_STATE_NAMES[leState], false);

        mItemText.Update();
    }

    // ---- GetHighlightedId @ 0x82489080 ----------------------------------------------
    // The X360 calls GetHighlighted() twice: once for the assert, once for the id read.
    u64 MenuToggle::GetHighlightedId()
    {
        CGS_ASSERT(mItemText.GetHighlighted() != 0, "GetHighlighted()");   // BrnSelectableGroup.h:218
        return mItemText.GetHighlightedId();
    }

    // ---- HighlightNext @ 0x82482860 -------------------------------------------------
    bool MenuToggle::HighlightNext()
    {
        if (!mItemText.HighlightNext(false))
        {
            return false;
        }
        SetDirty();
        return true;
    }

    // ---- HighlightPrevious @ 0x824828D8 ---------------------------------------------
    bool MenuToggle::HighlightPrevious()
    {
        if (!mItemText.HighlightPrevious(false))
        {
            return false;
        }
        SetDirty();
        return true;
    }
}
