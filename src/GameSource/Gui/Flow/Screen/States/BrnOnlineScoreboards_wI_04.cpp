// ===================================================================================
// BrnGui::OnlineScoreboards -- wave-I partfile 04: the filter-toggle setup family.
//   SetupCategories @0x8248F838  (DWARF cpp:1277)
//   SetupIndexes    @0x8248F8D8  (DWARF cpp:1299)
//   SetupVariations @0x8248F998  (DWARF cpp:1330)
// ===================================================================================

#include "GameSource/Gui/Flow/Screen/States/BrnOnlineScoreboards.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"

namespace BrnGui
{

    namespace
    {
        // The three rows of mFilterToggleGroup, in the order the screen's apt movie lays
        // them out. The X360 passes each as a literal 0/1/2 to SetupToggle / HighlightItem /
        // GetSelectable, and indexes KAPC_FILTER_TITLE_STRINGS with the same value
        // (off_82F2687C[0] / [1] / [2] == "$SCOREBOARD_EVENT" / "$SCOREBOARD_FILTER" /
        // "$SCOREBOARD_ROAD").
        const s32 KI_FILTER_ROW_CATEGORY  = 0;
        const s32 KI_FILTER_ROW_INDEX     = 1;
        const s32 KI_FILTER_ROW_VARIATION = 2;
    }

    // ================================================================================
    //  SetupCategories  @ 0x8248F838  (DWARF cpp:1277)
    //
    //  Re-stock the leaderboard-category filter row from the category names the back end
    //  sent, then park its option cursor on the category the screen is currently showing.
    // ================================================================================
    void OnlineScoreboards::SetupCategories()
    {
        // X360 argument registers: r4 = row, r5 = miMaxCategories (option count),
        // r6 = 1 (active), r7 = the title string, r8 = mapcCategories, r9 = 0 (no id
        // array -- the row falls back to the option indices).
        mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_CATEGORY, miMaxCategories, true,
                                       KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_CATEGORY],
                                       mapcCategories, 0);

        // The X360 inlines MenuToggleGroupVarSize<3>::HighlightItem here -- GetSelectable(0),
        // the row's mItemText.HighlightIndex through its inner group vtable slot 12, then
        // row +0xC |= 0x10 on success. The declared group method IS exactly that body, so
        // this is the inlining reversed, not a re-implementation.
        mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_CATEGORY, miCurrentCategory);
    }

    // ================================================================================
    //  SetupIndexes  @ 0x8248F8D8  (DWARF cpp:1299)
    //
    //  The same for the middle filter row (the category's sub-index, e.g. which of a
    //  category's boards), with the extra rule that a row offering a single choice is
    //  taken out of the filter row's highlight cycle.
    // ================================================================================
    void OnlineScoreboards::SetupIndexes()
    {
        mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_INDEX, miMaxIndexes, true,
                                       KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_INDEX],
                                       mapcIndexes, 0);

        // HighlightItem inlined by the X360, as in SetupCategories.
        mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_INDEX, miCurrentIndex);

        // One index is no choice at all, so the row stops accepting the highlight. The X360
        // hoists the test above the call and so emits GetSelectable(1) once per arm with a
        // different `li r4`; it is one logical call. The vtable slot dispatched is the row's
        // +4 == Selectable slot 1 == SetHighlightable(bool).
        mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_INDEX)
            ->SetHighlightable(miMaxIndexes > 1);
    }

    // ================================================================================
    //  SetupVariations  @ 0x8248F998  (DWARF cpp:1330)
    //
    //  The third filter row is optional: with a single variation there is nothing to pick,
    //  so the row is switched off entirely rather than stocked. Its title depends on what
    //  the variations mean for this board -- events for an event leaderboard, otherwise the
    //  per-road list.
    // ================================================================================
    void OnlineScoreboards::SetupVariations()
    {
        if (miMaxVariations > 1)
        {
            // Row vtable +0 == Selectable slot 0 == SetActive(bool).
            mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_VARIATION)->SetActive(true);

            const char* lpacTitle = mbEventLeaderboard
                                        ? "$HUD_INFO_EVENT"
                                        : KAPC_FILTER_TITLE_STRINGS[KI_FILTER_ROW_VARIATION];

            mFilterToggleGroup.SetupToggle(KI_FILTER_ROW_VARIATION, miMaxVariations, true,
                                           lpacTitle, mapcVariations, 0);

            // Unlike the two rows above, the X360 leaves this HighlightItem out of line
            // (a real `bl` to MenuToggleGroupVarSize<3>::HighlightItem) -- same call either
            // way here.
            mFilterToggleGroup.HighlightItem(KI_FILTER_ROW_VARIATION, miCurrentVariation);
        }
        else
        {
            mFilterToggleGroup.GetSelectable(KI_FILTER_ROW_VARIATION)->SetActive(false);
        }
    }
}
