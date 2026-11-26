// ===================================================================================
// BrnGui::MenuToggleGroupVarSize<TI_SIZE>  -- template method bodies
//   b5-decomp/src/GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.cpp
//
// The shared template bodies live in BrnMenuToggleGroup.h (ARTIST inlines them across TUs);
// this file only instantiates every member of the four shipped sizes (<2>/<3>/<4>/<5>) so each
// out-of-line copy is emitted and measured in one object. Reconstructed from BURNOUT_X360_ARTIST.XEX
// (the <3> recon fixed the canonical member names/signatures in BrnMenuToggleGroup.h; the <2>/<4>
// /<5> recons match those bodies store-for-store, differing only in TI_SIZE).
//
// The group is a real SelectableGroup (so a real Selectable with a live vptr) and its rows
// are real MenuToggles, so every group/row virtual below is an ordinary virtual call. Slot
// map (read from the ARTIST .rdata, e.g. MenuToggleGroupVarSize<2> @0x820746F0):
//   group slot  0/1/2/3 -> SetActive / SetHighlightable / SetSelectable / SetHighlighted
//   group slot  4/5/6/7 -> Select / Update / Clear / Add
//   group slot 10/11/12 -> HighlightNext / HighlightPrevious / HighlightIndex
//   group slot 13/14    -> HighlightNextItem / HighlightPreviousItem
//
// The compiler-synthesised default ctor (MenuToggleGroupVarSize()) used to be BLOCKED on the
// grounds that its X360 body installs a raft of sub-object vtables. That is exactly what C++
// emits IMPLICITLY for an object of this shape (its own vtable pair, each row's Selectable +
// GuiComponent + TextSelection + 100 TextSelectionItem chains): the body stores no field
// values, so the reconstruction is an empty body -- the same reading BrnGui::CarSelectVehicle's
// and BrnGui::TextSelection's ctors already carry.
// ===================================================================================

#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggleGroup.h"
#include "GameSource/Gui/Flow/Shared/Components/BrnMenuToggle.h"       // BrnGui::MenuToggle
#include "GameSource/Gui/Flow/Shared/Components/BrnSelectableGroup.h"  // BrnGui::SelectableGroup (base)
#include "GameSource/Gui/BrnGuiCache.h"                                // BrnGui::GuiCache / GuiFlow (AppendExpectedAptComponent forwards)
#include "GameShared/GameClasses/Core/CgsAssertStreamMessageAt.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/Log/CgsLog.h"             // CgsDev::Log::gpDebugPrint
#include "GameShared/GameClasses/Development/MessageSystem/CgsMessage.h" // CgsDev::Message::gxMessageFilterFlags / KX_FILTER_GLOBAL

#include <cstring>   // std::strlen (Construct name-length assert)

namespace BrnGui
{
    // ---------------------------------------------------------------------------------
    // Per-N explicit instantiations of the shared template bodies (the four shipped screens use
    // <2>/<3>/<4>/<5>). Instantiated MEMBER-BY-MEMBER rather than whole-class, so the BLOCKED
    // compiler-synthesised ctor (declared-but-undefined above) is NOT forced -- a whole-class
    // `template class MenuToggleGroupVarSize<N>;` would require its (unreconstructed) body.
    // ---------------------------------------------------------------------------------
#define BRN_INSTANTIATE_MENUTOGGLEGROUP(N)                                                        \
    template MenuToggleGroupVarSize<N>::MenuToggleGroupVarSize();                                  \
    template void        MenuToggleGroupVarSize<N>::AppendExpectedAptComponent(GuiFlow, GuiCache*, bool); \
    template void        MenuToggleGroupVarSize<N>::Clear();                                       \
    template void        MenuToggleGroupVarSize<N>::Construct(const char*, CgsGui::StateInterface*,\
                                                              s32, const char*, u64);              \
    template MenuToggle* MenuToggleGroupVarSize<N>::GetSelectable(s32);                            \
    template bool        MenuToggleGroupVarSize<N>::HighlightItem(s32, s32);                       \
    template bool        MenuToggleGroupVarSize<N>::HighlightNextItem();                           \
    template bool        MenuToggleGroupVarSize<N>::HighlightPreviousItem();                       \
    template void        MenuToggleGroupVarSize<N>::SetupGroup(s32, u8);                           \
    template void        MenuToggleGroupVarSize<N>::SetupToggle(s32, s32, bool, const char*,       \
                                                               const char**, u64*);                \
    template void        MenuToggleGroupVarSize<N>::Unloaded();

    BRN_INSTANTIATE_MENUTOGGLEGROUP(2)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(3)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(4)
    BRN_INSTANTIATE_MENUTOGGLEGROUP(5)

#undef BRN_INSTANTIATE_MENUTOGGLEGROUP
}
