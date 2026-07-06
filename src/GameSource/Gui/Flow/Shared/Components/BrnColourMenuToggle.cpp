// ===================================================================================
// BrnGui::ColourMenuToggle -- out-of-line bodies.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Clear @0x824E5670, Construct @0x824E8B70, HighlightIndex @0x824EA2A0,
//   HighlightNext @0x824EA1A0, HighlightPrevious @0x824EA220, Select @0x824E56C8.
// (HighlightNeighbours / SetupMenuToggleGradient / Update are SKIPPED -- declared-only.)
// ===================================================================================

#include "GameSource/Gui/Flow/Shared/Components/BrnColourMenuToggle.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT

namespace BrnGui
{
    // Thin forwarder for the picker's Construct. The real method is
    // BrnGui::ColourSelection::Construct; it is bodied co-located with that type. Declared
    // free here so the toggle need not fully size the picker (the per-TU gate does not link).
    void ColourSelectionConstruct(void* lpSelection, const char* lpacName,
                                  CgsGui::StateInterface* lpStateInterface,
                                  const char* lpacParentName, u64 lu64Id);

    // off_82F27474 -- the five child colour-selection component names, by offset-from-centre.
    // Only entry [0] "ItemColourM2" is rodata-attested for this TU; [1..4] follow the M2..P2
    // pattern the class uses everywhere. Verify against off_82F27474 when the block is dumped.
    const char* const ColourMenuToggle::KAC_ITEM_COLOUR_COMPONENT[ColourMenuToggle::KI_NUM_ITEMS] =
    {
        "ItemColourM2",   // [0] (attested)
        "ItemColourM1",   // [1] (pattern)
        "ItemColour",     // [2] (pattern, centre)
        "ItemColourP1",   // [3] (pattern)
        "ItemColourP2",   // [4] (pattern)
    };

    // @ 0x824E5670 -- Clear each of the 5 embedded colour selections via their component
    // Clear virtual (vtable slot 6 == byte +0x18). The X360 walks maColourSelection by stride
    // 0x1338 and dispatches (*(*sel + 0x18))(sel).
    void ColourMenuToggle::Clear()
    {
        ColourSelectionSlot* lpSel = &maColourSelection[0];        // this + 0xA8
        for (s32 li = 0; li < KI_NUM_ITEMS; ++li)
        {
            void** lppVTable = *reinterpret_cast<void***>(lpSel);
            reinterpret_cast<void (*)(ColourSelectionSlot*)>(lppVTable[6])(lpSel); // slot 6 == +0x18
            lpSel = reinterpret_cast<ColourSelectionSlot*>(reinterpret_cast<u8*>(lpSel) + 0x1338);
        }
    }

    // @ 0x824E8B70 -- name the toggle + wire its state interface / apt id, construct the 5
    // embedded colour selections (each parented on this component's own name buffer), then the
    // title text field, and mark the component loaded.
    void ColourMenuToggle::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                     const char* lpacParentName, u64 lu64AptId)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name");                    // @0x824E8BA8 (cmplwi r21,0)
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface"); // @0x824E8C14 (cmplwi r22,0)

        // X360: GuiComponent::Construct(this + 0x18, name, stateInterface, parentName).
        mGuiComponentBase.Construct(lpacName, lpStateInterface, lpacParentName);

        mu64Id  = lu64AptId;   // std r19 -> +0x10
        muFlags = 0;           // stb 0  -> +0x0C

        // Child colour selections are parented on THIS component's own name buffer
        // (X360 passes r27 == this + 0x1C == mGuiComponentBase.macName).
        const char* lpacChildParent = mGuiComponentBase.GetName();

        ColourSelectionSlot* lpSel = &maColourSelection[0];   // this + 0xA8
        for (s32 li = 0; li < KI_NUM_ITEMS; ++li)
        {
            ColourSelectionConstruct(lpSel, KAC_ITEM_COLOUR_COMPONENT[li],
                                     lpStateInterface, lpacChildParent, KU64_NO_ID);
            lpSel = reinterpret_cast<ColourSelectionSlot*>(reinterpret_cast<u8*>(lpSel) + 0x1338);
        }

        // Title text field: virtual Construct (slot 0) on the embedded TextField @ +0x60C0.
        mTitleText.Construct("TitleText", lpStateInterface, lpacChildParent);

        miLoadedItems = 1;     // stb 1 -> +0x61E6
    }

    // @ 0x824EA2A0 -- highlight the requested index on the CENTRE colour selection (vtable
    // slot 12 == byte +0x30). When it reports a change, refresh the neighbour selections and
    // dirty the component. The X360 forwards ONLY r3 == centre selection.
    bool ColourMenuToggle::HighlightIndex(s32 liIndex)
    {
        (void)liIndex;
        ColourSelectionSlot* lpCentre = &maColourSelection[KI_CENTRE_ITEM]; // this + 0x2718
        void** lppVTable = *reinterpret_cast<void***>(lpCentre);
        const bool lbChanged =
            reinterpret_cast<bool (*)(ColourSelectionSlot*)>(lppVTable[12])(lpCentre); // slot 12 == +0x30
        if (!lbChanged)
            return false;

        HighlightNeighbours();
        muFlags |= KU_FLAG_QUERIED;   // *(this+0xC) |= 0x10
        return true;
    }

    // @ 0x824EA1A0
    bool ColourMenuToggle::HighlightNext()
    {
        // Delegate to the focused picker (element [2]) via its SelectableGroup base
        // (vtable slot +0x28 == HighlightNext(quiet=false)).
        if (!GetFocusedSelectionNav()->HighlightNext(false))
            return false;

        HighlightNeighbours();
        muFlags |= KU_FLAG_DIRTY;                                       // *(this+0xC) |= 0x10
        return true;
    }

    // @ 0x824EA220
    bool ColourMenuToggle::HighlightPrevious()
    {
        // As HighlightNext but vtable slot +0x2C == HighlightPrevious(quiet=false).
        if (!GetFocusedSelectionNav()->HighlightPrevious(false))
            return false;

        HighlightNeighbours();
        muFlags |= KU_FLAG_DIRTY;                                       // *(this+0xC) |= 0x10
        return true;
    }

    // @ 0x824E56C8 -- plain tail-call of the focused picker's SelectableGroup::Select
    // (vtable slot +0x10). No stack frame, no dirty flag.
    void ColourMenuToggle::Select()
    {
        GetFocusedSelectionNav()->Select();
    }
}
