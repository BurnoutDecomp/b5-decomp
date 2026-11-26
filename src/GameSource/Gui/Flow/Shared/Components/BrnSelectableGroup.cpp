// ===================================================================================
// BrnGui::SelectableGroup  -- implementation
//   class:BrnGui::SelectableGroup
//
// Reconstructed from the X360 ARTIST build. The group owns up to 100 BrnGui::Selectable*
// with a highlight cursor + wrap flag. Member-by-name access; each item's highlight/select
// virtuals are called directly (Selectable vtable slots: 0 SetActive, 1 SetHighlightable,
// 2 SetSelectable, 3 SetHighlighted, 4 Select, 5 Update). The group's own dirty bit is
// Selectable dirty bit (mxFlags 0x10).
// ===================================================================================
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectable.h"   // BrnGui::Selectable (item virtuals + accessors)
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                 // CGS_ASSERT
#define CGS_ARTIST_FILE "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnSelectableGroup.cpp"
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
        GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        SetId(luAptId);             // *(this+0x10)
        ClearFlags();               // *(this+0x0C)
        for (s32 li = 0; li < KI_MAX_SELECTABLES; ++li)
            maSelectables[li] = 0;

        // A direct `bl BrnGui__SelectableGroup__Clear`, not a slot-6 dispatch: a derived
        // group's Construct runs the BASE Clear here and its own Clear afterwards.
        SelectableGroup::Clear();

        // The asm re-normalises the group tail after Clear (idempotent with Clear's resets).
        mbWrapped          = false;
        miSelectableCount  = 0;
        miHighlightedIndex = -1;
        SetDirty();
    }

    // @ 0x824E3100 -- reset + release every occupied slot, then clear the group state.
    void SelectableGroup::Clear()
    {
        // DWARF cpp:76 liItem / cpp:77 lpSelectable. ARTIST re-reads the slot for every call
        // (0x824E3144/0x824E315C/0x824E3174/0x824E3188), so the calls go through the array.
        for (s32 liItem = 0; liItem < KI_MAX_SELECTABLES; ++liItem)
        {
            Selectable* lpSelectable = maSelectables[liItem];
            if (lpSelectable != NULL)
            {
                maSelectables[liItem]->SetActive(false);        // slot 0
                maSelectables[liItem]->SetHighlighted(false);   // slot 3 (X360 order: after Active, before Highlightable)
                maSelectables[liItem]->SetHighlightable(false); // slot 1
                lpSelectable = maSelectables[liItem];
                lpSelectable->SetId(KU64_NO_ID);                // id -> none
                lpSelectable->SetDirty();
                maSelectables[liItem]->Update();                // slot 5
                maSelectables[liItem] = NULL;
            }
        }
        miSelectableCount  = 0;
        SetId(KU64_NO_ID);
        miHighlightedIndex = -1;
        SetDirty();
    }

    // @ 0x824E32F8 -- append a selectable pointer, ++count, dirty.
    void SelectableGroup::Add(Selectable* lpSelectable)
    {
        // Streamed asserts, ARTIST 0x824E32F8 (cpp:160/161/162).
        CGS_ASSERT_STREAM_AT(lpSelectable != NULL, "SelectableGroup::Add() Invalid selectable",
                             CGS_ARTIST_FILE, 160);
        CGS_ASSERT_STREAM_AT(miSelectableCount >= 0, "SelectableGroup::Add() Index is invalid.",
                             CGS_ARTIST_FILE, 161);
        CGS_ASSERT_STREAM_AT(miSelectableCount < KI_MAX_SELECTABLES,
                             "SelectableGroup::Add() Group is already full.", CGS_ARTIST_FILE, 162);

        maSelectables[miSelectableCount] = lpSelectable;
        ++miSelectableCount;
        SetDirty();
    }

    // @ 0x8240FC98 -- bounds-check, set the queried flag, return the slot.
    Selectable* SelectableGroup::GetSelectable(s32 liIndex)
    {
        CGS_ASSERT_STREAM_AT(liIndex >= 0 && liIndex < miSelectableCount, "Invalid index passed to SelectableGroup::GetSelectable()", "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnSelectableGroup.h", 248);

        SetDirty();
        return maSelectables[liIndex];
    }

    // ARTIST 0x8240FD60 checks both bounds and leaves the group's flags alone.
    const Selectable* SelectableGroup::GetHighlighted() const
    {
        CGS_ASSERT_STREAM_AT(miHighlightedIndex >= 0 && miHighlightedIndex < KI_MAX_SELECTABLES,
            "No highlighted index to get",
                                         "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnSelectableGroup.h", 298);
        return maSelectables[miHighlightedIndex];
    }

    // ARTIST 0x824E21B8 validates the same cursor before marking the group dirty.
    Selectable* SelectableGroup::GetHighlighted()
    {
        CGS_ASSERT_STREAM_AT(miHighlightedIndex >= 0 && miHighlightedIndex < KI_MAX_SELECTABLES,
            "No highlighted index to get",
                                         "..\\..\\..\\GameSource\\Gui/Flow/Shared/Components/BrnSelectableGroup.h", 281);
        SetDirty();
        return maSelectables[miHighlightedIndex];
    }

    // @ 0x824E4158 -- linear search for the item whose id matches (low 32 bits), else -1.
    s32 SelectableGroup::GetIndexFromId(u64 lId)
    {
        // Full 64-bit id compares (ARTIST cmpld 0x824E4174/0x824E42FC); DWARF cpp:534 liIndex;
        // streamed cpp:532/538, GetSelectable called for the assert and again for the compare.
        CGS_ASSERT_STREAM_AT(lId != KU64_NO_ID,
            "Invalid index to search for (probable duplication of invalid index, too)", CGS_ARTIST_FILE, 532);
        for (s32 liIndex = 0; liIndex < miSelectableCount; ++liIndex)
        {
            CGS_ASSERT_STREAM_AT(GetSelectable(liIndex) != NULL,
                "unable to get selectable with index : " << liIndex << "\n", CGS_ARTIST_FILE, 538);
            if (GetSelectable(liIndex)->GetId() == lId)
            {
                return liIndex;
            }
        }
        return -1;
    }

    // @ 0x824E3CE0 -- highlight index (un-highlight the previous; -1 clears). Returns changed.
    bool SelectableGroup::HighlightIndex(s32 liIndex)
    {
        CGS_ASSERT_STREAM_AT(liIndex >= -1 && liIndex < miSelectableCount,
            "Invalid item index to highlight : " << liIndex << " / " << miSelectableCount << "\n",
            CGS_ARTIST_FILE, 419);

        if (liIndex == -1)
        {
            if (miHighlightedIndex != -1)
            {
                maSelectables[miHighlightedIndex]->SetHighlighted(false);
                miHighlightedIndex = -1;
                SetDirty();
                return true;
            }
            return false;
        }

        // No DWARF local here: the slot is re-read for the final call (ARTIST 0x824E3E8C).
        if (!maSelectables[liIndex]->IsActive() || !maSelectables[liIndex]->IsHighlightable())
            return false;

        if (miHighlightedIndex != -1)
            maSelectables[miHighlightedIndex]->SetHighlighted(false);
        miHighlightedIndex = static_cast<s8>(liIndex);
        maSelectables[liIndex]->SetHighlighted(true);
        SetDirty();
        return true;
    }

    // @ 0x824E6720 -- resolve the id to an index, then highlight it.
    bool SelectableGroup::HighlightId(u64 lId)
    {
        // Streamed cpp:462/467 (the id through the u64 operator<<, ARTIST 0x824E685C); DWARF
        // cpp:464 lbSuccess, cpp:465 liIndex.
        CGS_ASSERT_STREAM_AT(lId != KU64_NO_ID, "Not valid to find an invalid Id", CGS_ARTIST_FILE, 462);
        bool lbSuccess = false;
        s32 liIndex = GetIndexFromId(lId);
        CGS_ASSERT_STREAM_AT(liIndex >= 0 && liIndex < miSelectableCount,
            "Could not find item with liIndex : " << liIndex << " : (" << lId << ")\n",
            CGS_ARTIST_FILE, 467);
        if (liIndex != -1)
        {
            lbSuccess = HighlightIndex(liIndex);
        }
        return lbSuccess;
    }

    // @ 0x824E3808 -- move the highlight forward to the next usable item; wrap when mbWrapped.
    bool SelectableGroup::HighlightNext(bool lbQuiet)
    {
        // DWARF cpp:294..296 locals; streamed asserts cpp:290/302 (ARTIST 0x824E3890/0x824E394C).
        if (miHighlightedIndex == -1)
        {
            CGS_FAIL_STREAM_AT("ERROR : No option highlighted to select the next\n",
                                 CGS_ARTIST_FILE, 290);
            return false;
        }

        s32 liIter;
        Selectable* lpSelectable;
        bool lbSuccess = false;
        for (liIter = miHighlightedIndex + 1; liIter < miSelectableCount; ++liIter)
        {
            lpSelectable = maSelectables[liIter];
            CGS_ASSERT_STREAM_AT(lpSelectable != NULL, "Invalid selectable item", CGS_ARTIST_FILE, 302);
            if (lpSelectable->IsActive() && lpSelectable->IsHighlightable() && !lpSelectable->IsHighlighted())
            {
                if (!lbQuiet)
                    HighlightIndex(liIter);
                lbSuccess = true;
                break;
            }
        }

        if (!lbSuccess && mbWrapped)
        {
            for (liIter = 0; liIter < miHighlightedIndex; ++liIter)
            {
                lpSelectable = maSelectables[liIter];
                if (lpSelectable->IsActive() && lpSelectable->IsHighlightable() && !lpSelectable->IsHighlighted())
                {
                    if (!lbQuiet)
                        HighlightIndex(liIter);
                    lbSuccess = true;
                    break;
                }
            }
        }
        return lbSuccess;
    }

    // @ 0x824E3A78 -- move the highlight backward to the previous usable item; wrap when mbWrapped.
    bool SelectableGroup::HighlightPrevious(bool lbQuiet)
    {
        // Streamed asserts cpp:356/368 (the mirror of HighlightNext).
        if (miHighlightedIndex == -1)
        {
            CGS_FAIL_STREAM_AT("No option highlighted to select the previous\n",
                                 CGS_ARTIST_FILE, 356);
            return false;
        }

        s32 liIter;
        Selectable* lpSelectable;
        bool lbSuccess = false;
        for (liIter = miHighlightedIndex - 1; liIter >= 0; --liIter)
        {
            lpSelectable = maSelectables[liIter];
            CGS_ASSERT_STREAM_AT(lpSelectable != NULL, "Invalid selectable item", CGS_ARTIST_FILE, 368);
            if (lpSelectable->IsActive() && lpSelectable->IsHighlightable() && !lpSelectable->IsHighlighted())
            {
                if (!lbQuiet)
                    HighlightIndex(liIter);
                lbSuccess = true;
                break;
            }
        }

        if (!lbSuccess && mbWrapped)
        {
            for (liIter = miSelectableCount - 1; liIter > miHighlightedIndex; --liIter)
            {
                lpSelectable = maSelectables[liIter];
                if (lpSelectable->IsActive() && lpSelectable->IsHighlightable() && !lpSelectable->IsHighlighted())
                {
                    if (!lbQuiet)
                        HighlightIndex(liIter);
                    lbSuccess = true;
                    break;
                }
            }
        }
        return lbSuccess;
    }

    // @ 0x824E3490 -- (re)enable the item: Active/Highlightable/Selectable on.
    void SelectableGroup::Enable(s32 liIndex)
    {
        CGS_ASSERT_STREAM_AT(liIndex >= 0 && liIndex < miSelectableCount,
                             "Invalid item to enable. use Add() or SetupMenu() first", CGS_ARTIST_FILE, 182);
        // DWARF cpp:184 lpSelectable; the null assert follows it (cpp:186).
        Selectable* lpSelectable = GetSelectable(liIndex);
        CGS_ASSERT_STREAM_AT(lpSelectable != NULL, "Invalid selectable", CGS_ARTIST_FILE, 186);
        if (lpSelectable != NULL)
        {
            if (!lpSelectable->IsActive())
            {
                lpSelectable->SetActive(true);
                lpSelectable->SetHighlightable(true);
                lpSelectable->SetSelectable(true);
                SetDirty();
            }
        }
    }

    // @ 0x824E3620 -- disable the item (Active off).
    void SelectableGroup::Disable(s32 liIndex)
    {
        CGS_ASSERT_STREAM_AT(liIndex >= 0 && liIndex < miSelectableCount,
                             "Invalid item to enable. use Add() first", CGS_ARTIST_FILE, 230);
        // DWARF cpp:232 lpSelectable; the null assert follows it (cpp:234).
        Selectable* lpSelectable = GetSelectable(liIndex);
        CGS_ASSERT_STREAM_AT(lpSelectable != NULL, "Invalid selectable", CGS_ARTIST_FILE, 234);
        if (lpSelectable != NULL)
        {
            if (lpSelectable->IsActive())
            {
                // Disabling the highlighted item first moves the highlight off it: slot 10
                // then slot 11 on the group's own vptr (`lwz 0x28` / `lwz 0x2C`, r4 = 0), i.e.
                // HighlightNext(false) and, only if that found nothing, HighlightPrevious(false).
                // Neither moving is the console's "last item" tripwire (cpp:246, non-gating).
                if (lpSelectable->IsHighlighted() && !HighlightNext(false) && !HighlightPrevious(false))
                {
                    CGS_FAIL_STREAM_AT("Disabling the last item available", CGS_ARTIST_FILE, 246);
                }
                lpSelectable->SetActive(false);
                SetDirty();
            }
        }
    }

    // @ 0x824E3EC8 -- select (activate) the highlighted item if it is selectable.
    void SelectableGroup::Select()
    {
        // Streamed cpp:488; the index is re-read for each use (ARTIST 0x824E3F90/0x824E3FB4).
        CGS_ASSERT_STREAM_AT(miHighlightedIndex >= 0 && miHighlightedIndex < KI_MAX_SELECTABLES,
            "Invalid highlighted index : " << miHighlightedIndex << "\n", CGS_ARTIST_FILE, 488);
        if (maSelectables[miHighlightedIndex]->IsSelectable())
            maSelectables[miHighlightedIndex]->Select();   // slot 4
    }

    // @ 0x824E3FE0 -- if dirty, Update every item and clear the dirty bit.
    void SelectableGroup::Update()
    {
        if (!IsDirty())
            return;
        ClearFlag(E_FLAG_DIRTY);   // `xori 0x10` on a bit just tested set
        // DWARF cpp:512 int8_t liIndex; streamed cpp:515 (its s32 operator<< inlines in the loop).
        for (s8 liIndex = 0; liIndex < miSelectableCount; ++liIndex)
        {
            CGS_ASSERT_STREAM_AT(maSelectables[liIndex] != NULL,
                "Invalid pointer index set in SelectableGroup::Update() : " << liIndex << "\n",
                CGS_ARTIST_FILE, 515);
            maSelectables[liIndex]->Update();   // slot 5
        }
    }

    // @ 0x824E31D8 -- set one item's id + dirty it.
    void SelectableGroup::SetSelectableId(s32 liIndex, u64 luId)
    {
        Selectable* lpSel = GetSelectable(liIndex);
        CGS_ASSERT_AT(lpSel != 0,
            "lpSelectable",
                CGS_ARTIST_FILE, 112 /* ARTIST 0x824E3208 */);

        lpSel->SetId(luId);
        lpSel->SetDirty();
    }

    // @ 0x824E3248 -- set every item's id (from the array, or its index when null) + dirty.
    void SelectableGroup::SetIds(const u64* lpIdArray)
    {
        // DWARF cpp:127 liIndex / cpp:131 lpSelectable; the id is chosen after the dirty mark
        // (ARTIST 0x824E32B4 ori before the lpIdArray test).
        for (s32 liIndex = 0; liIndex < miSelectableCount; ++liIndex)
        {
            Selectable* lpSelectable = GetSelectable(liIndex);
            CGS_ASSERT_AT(lpSelectable != 0, "lpSelectable", CGS_ARTIST_FILE, 133 /* ARTIST 0x824E32AC */);
            lpSelectable->SetId((lpIdArray != NULL) ? lpIdArray[liIndex] : static_cast<u64>(liIndex));
            lpSelectable->SetDirty();
        }
    }
}

#undef CGS_ARTIST_FILE
