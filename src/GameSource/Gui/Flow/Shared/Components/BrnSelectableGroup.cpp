// ===================================================================================
// BrnGui::SelectableGroup  -- implementation
//   class:BrnGui::SelectableGroup
//
// Reconstructed from the X360 ARTIST build. The group owns up to 100 BrnGui::Selectable*
// with a highlight cursor + wrap flag. Member-by-name access; each item's highlight/select
// virtuals are called directly (Selectable vtable slots: 0 SetActive, 1 SetHighlightable,
// 2 SetSelectable, 3 SetHighlighted, 4 Select, 5 Update). The group's own dirty bit is
// muFlags 0x10.
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"   // BrnGui::Selectable (item virtuals + accessors)
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CGS_ASSERT

namespace BrnGui
{
    using CgsGui::StateInterface;

    // @ 0x824E65B8 -- name the group + wire its state interface/apt id, null the slots, Clear.
    void SelectableGroup::Construct(const char* lpacName, StateInterface* lpStateInterface,
                                    const char* lpacParentName, u64 luAptId)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name passed to SelectableGroup::Construct");
        CGS_ASSERT(lpStateInterface != 0, "Invalid StateInterface passed to SelectableGroup::Construct");

        // X360: GuiComponent::Construct(this + 0x18, name, stateInterface, parentName).
        mGuiComponentBase.Construct(lpacName, lpStateInterface, lpacParentName);

        mu64SelectedId = luAptId;   // *(this+0x10)
        muFlags        = 0;         // *(this+0x0C)
        for (s32 li = 0; li < KI_MAX_SELECTABLES; ++li)
            maSelectables[li] = 0;

        Clear();

        // The asm re-normalises the group tail after Clear (idempotent with Clear's resets).
        mbWrapped          = false;
        miSelectableCount  = 0;
        miHighlightedIndex = -1;
        muFlags |= KU_FLAG_QUERIED;
    }

    // @ 0x824E3100 -- reset + release every occupied slot, then clear the group state.
    void SelectableGroup::Clear()
    {
        for (s32 li = 0; li < KI_MAX_SELECTABLES; ++li)
        {
            Selectable* lpSel = maSelectables[li];
            if (lpSel != 0)
            {
                lpSel->SetActive(false);        // slot 0
                lpSel->SetHighlighted(false);   // slot 3 (X360 order: after Active, before Highlightable)
                lpSel->SetHighlightable(false); // slot 1
                lpSel->SetId(KU64_NO_ID);       // id -> none
                lpSel->SetDirty();
                lpSel->Update();                // slot 5
                maSelectables[li] = 0;
            }
        }
        miSelectableCount  = 0;
        mu64SelectedId     = KU64_NO_ID;
        miHighlightedIndex = -1;
        muFlags |= KU_FLAG_QUERIED;
    }

    // @ 0x824E32F8 -- append a selectable pointer, ++count, dirty.
    void SelectableGroup::Add(Selectable* lpSelectable)
    {
        CGS_ASSERT(lpSelectable != 0, "SelectableGroup::Add() Invalid selectable");
        CGS_ASSERT(miSelectableCount < KI_MAX_ADD, "SelectableGroup::Add() Index is invalid.");
        CGS_ASSERT(miSelectableCount < KI_MAX_SELECTABLES, "SelectableGroup::Add() Group is already full.");

        maSelectables[miSelectableCount] = lpSelectable;
        ++miSelectableCount;
        muFlags |= KU_FLAG_QUERIED;
    }

    // @ 0x8240FC98 -- bounds-check, set the queried flag, return the slot.
    Selectable* SelectableGroup::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miSelectableCount,
                   "Invalid index passed to SelectableGroup::GetSelectable()");
        muFlags |= KU_FLAG_QUERIED;
        return maSelectables[liIndex];
    }

    // @ 0x8240FD60 -- return the highlighted slot (X360 only bounds-checks the upper end).
    //
    // ⚠️⚠️ HAZARD, and it is the CONSOLE's, reproduced faithfully: there is NO LOWER bound.
    // The X360 body is `lbz r11,0xA5(r28) / extsb / cmpwi r11,0 / blt <skip assert> /
    // cmpwi r11,0x64 ... / lwzx r3, r28, 4*(r11+42)` -- a negative index only skips the
    // ">= 100" ASSERT, the indexed load still happens. So calling this on an EMPTY group
    // (miHighlightedIndex == -1, the Construct/Clear value) reads the storage immediately
    // BEFORE maSelectables -- {miSelectableCount, miHighlightedIndex, mbWrapped, pad} --
    // and returns it as a Selectable*. On this build that is 0x0000FF00: NON-NULL, so every
    // caller's `CGS_ASSERT(GetHighlighted() != 0)` passes silently and the next member read
    // access-violates. (Cost: one wave, chasing an Apt-engine crash that was this.)
    //
    // Callers must test "is anything highlighted" THEMSELVES, with the console's own signed
    // idiom `miHighlightedIndex > -1` -- see TextSelection::Update @0x824E83A0 (X360
    // `extsb / cmpwi -1`) and the PC-BUILD GUARD in CarSelectVehicle::SetupComponents.
    Selectable* SelectableGroup::GetHighlighted()
    {
        CGS_ASSERT(miHighlightedIndex < KI_MAX_SELECTABLES, "No highlighted index to get");
        return maSelectables[miHighlightedIndex];
    }

    // @ 0x8241EA28 -- assert a row is highlighted, return its id.
    u64 SelectableGroup::GetHighlightedId()
    {
        CGS_ASSERT(GetHighlighted() != 0, "GetHighlighted()");
        return GetHighlighted()->GetId();
    }

    // @ 0x824E4158 -- linear search for the item whose id matches (low 32 bits), else -1.
    s32 SelectableGroup::GetIndexFromId(u64 luId)
    {
        CGS_ASSERT(static_cast<u32>(luId) != static_cast<u32>(KU64_NO_ID),
                   "Invalid index to search for (probable duplication of invalid index, too)");
        for (s32 li = 0; li < miSelectableCount; ++li)
        {
            Selectable* lpSel = GetSelectable(li);
            CGS_ASSERT(lpSel != 0, "unable to get selectable with index");
            if (static_cast<u32>(lpSel->GetId()) == static_cast<u32>(luId))
                return li;
        }
        return -1;
    }

    // @ 0x824E3CE0 -- highlight index (un-highlight the previous; -1 clears). Returns changed.
    bool SelectableGroup::HighlightIndex(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= -1 && liIndex < miSelectableCount, "Invalid item index to highlight");

        if (liIndex == -1)
        {
            if (miHighlightedIndex != -1)
            {
                maSelectables[miHighlightedIndex]->SetHighlighted(false);
                miHighlightedIndex = -1;
                muFlags |= KU_FLAG_QUERIED;
                return true;
            }
            return false;
        }

        Selectable* lpSel = maSelectables[liIndex];
        if (!lpSel->IsActive() || !lpSel->IsHighlightable())
            return false;

        if (miHighlightedIndex != -1)
            maSelectables[miHighlightedIndex]->SetHighlighted(false);
        miHighlightedIndex = static_cast<s8>(liIndex);
        lpSel->SetHighlighted(true);
        muFlags |= KU_FLAG_QUERIED;
        return true;
    }

    // @ 0x824E6720 -- resolve the id to an index, then highlight it.
    bool SelectableGroup::HighlightId(u64 luId)
    {
        CGS_ASSERT(static_cast<u32>(luId) != static_cast<u32>(KU64_NO_ID), "Not valid to find an invalid Id");
        const s32 liIndex = GetIndexFromId(luId);
        CGS_ASSERT(liIndex >= 0 && liIndex < miSelectableCount, "Could not find item with liIndex");
        if (liIndex == -1)
            return false;
        return HighlightIndex(liIndex);
    }

    // @ 0x824E3808 -- move the highlight forward to the next usable item; wrap when mbWrapped.
    bool SelectableGroup::HighlightNext(bool lbQuiet)
    {
        if (miHighlightedIndex == -1)
        {
            CGS_ASSERT(false, "ERROR : No option highlighted to select the next");
            return false;
        }

        bool lbHighlighted = false;
        for (s32 li = miHighlightedIndex + 1; li < miSelectableCount; ++li)
        {
            Selectable* lpSel = maSelectables[li];
            CGS_ASSERT(lpSel != 0, "Invalid selectable item");
            if (lpSel->IsActive() && lpSel->IsHighlightable() && !lpSel->IsHighlighted())
            {
                if (!lbQuiet)
                    HighlightIndex(li);
                lbHighlighted = true;
                break;
            }
        }

        if (!lbHighlighted && mbWrapped)
        {
            for (s32 li = 0; li < miHighlightedIndex; ++li)
            {
                Selectable* lpSel = maSelectables[li];
                if (lpSel->IsActive() && lpSel->IsHighlightable() && !lpSel->IsHighlighted())
                {
                    if (!lbQuiet)
                        HighlightIndex(li);
                    lbHighlighted = true;
                    break;
                }
            }
        }
        return lbHighlighted;
    }

    // @ 0x824E3A78 -- move the highlight backward to the previous usable item; wrap when mbWrapped.
    bool SelectableGroup::HighlightPrevious(bool lbQuiet)
    {
        if (miHighlightedIndex == -1)
        {
            CGS_ASSERT(false, "No option highlighted to select the previous");
            return false;
        }

        bool lbHighlighted = false;
        for (s32 li = miHighlightedIndex - 1; li >= 0; --li)
        {
            Selectable* lpSel = maSelectables[li];
            CGS_ASSERT(lpSel != 0, "Invalid selectable item");
            if (lpSel->IsActive() && lpSel->IsHighlightable() && !lpSel->IsHighlighted())
            {
                if (!lbQuiet)
                    HighlightIndex(li);
                lbHighlighted = true;
                break;
            }
        }

        if (!lbHighlighted && mbWrapped)
        {
            for (s32 li = miSelectableCount - 1; li > miHighlightedIndex; --li)
            {
                Selectable* lpSel = maSelectables[li];
                if (lpSel->IsActive() && lpSel->IsHighlightable() && !lpSel->IsHighlighted())
                {
                    if (!lbQuiet)
                        HighlightIndex(li);
                    lbHighlighted = true;
                    break;
                }
            }
        }
        return lbHighlighted;
    }

    // @ 0x824E3490 -- (re)enable the item: Active/Highlightable/Selectable on.
    void SelectableGroup::Enable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miSelectableCount,
                   "Invalid item to enable. use Add() or SetupMenu() first");
        Selectable* lpSel = GetSelectable(liIndex);
        if (lpSel != 0)
        {
            if (!lpSel->IsActive())
            {
                lpSel->SetActive(true);
                lpSel->SetHighlightable(true);
                lpSel->SetSelectable(true);
                muFlags |= KU_FLAG_QUERIED;
            }
        }
        else
        {
            CGS_ASSERT(false, "Invalid selectable");
        }
    }

    // @ 0x824E3620 -- disable the item (Active off).
    void SelectableGroup::Disable(s32 liIndex)
    {
        CGS_ASSERT(liIndex >= 0 && liIndex < miSelectableCount, "Invalid item to enable. use Add() first");
        Selectable* lpSel = GetSelectable(liIndex);
        if (lpSel != 0)
        {
            if (lpSel->IsActive())
            {
                // (The X360 also debug-asserts here when disabling the last usable item, via two
                // un-named component-vtable slots 10/11 that count remaining usable items; that
                // debug-only guard is omitted pending those slots' identity.)
                lpSel->SetActive(false);
                muFlags |= KU_FLAG_QUERIED;
            }
        }
        else
        {
            CGS_ASSERT(false, "Invalid selectable");
        }
    }

    // @ 0x824E3EC8 -- select (activate) the highlighted item if it is selectable.
    void SelectableGroup::Select()
    {
        CGS_ASSERT(miHighlightedIndex < KI_MAX_SELECTABLES, "Invalid highlighted index");
        Selectable* lpSel = maSelectables[miHighlightedIndex];
        if (lpSel->IsSelectable())
            lpSel->Select();   // slot 4
    }

    // @ 0x824E3FE0 -- if dirty, Update every item and clear the dirty bit.
    void SelectableGroup::Update()
    {
        if ((muFlags & KU_FLAG_QUERIED) == 0)
            return;
        muFlags ^= KU_FLAG_QUERIED;
        for (s32 li = 0; li < miSelectableCount; ++li)
        {
            Selectable* lpSel = maSelectables[li];
            CGS_ASSERT(lpSel != 0, "Invalid pointer index set in SelectableGroup::Update()");
            lpSel->Update();   // slot 5
        }
    }

    // @ 0x824E31D8 -- set one item's id + dirty it.
    void SelectableGroup::SetSelectableId(s32 liIndex, u64 luId)
    {
        Selectable* lpSel = GetSelectable(liIndex);
        CGS_ASSERT(lpSel != 0, "lpSelectable");
        lpSel->SetId(luId);
        lpSel->SetDirty();
    }

    // @ 0x824E3248 -- set every item's id (from the array, or its index when null) + dirty.
    void SelectableGroup::SetIds(const u64* lpaIds)
    {
        for (s32 li = 0; li < miSelectableCount; ++li)
        {
            Selectable* lpSel = GetSelectable(li);
            CGS_ASSERT(lpSel != 0, "lpSelectable");
            const u64 luId = (lpaIds != 0) ? lpaIds[li] : static_cast<u64>(li);
            lpSel->SetDirty();
            lpSel->SetId(luId);
        }
    }
}
