#include "GameSource/Gui/BrnGuiHudMessageAnalyzer.h"   // brings BrnGuiEventTypeDefs + GuiHudMessage

#include "GameShared/GameClasses/Core/CgsAssert.h"     // CGS_ASSERT

// BrnGui::HudMessageAnalyzer -- reconstructed from BURNOUT_X360_ARTIST.XEX.
//
// Bodied here (wave B group 06):
//   HudMessageAnalyzer::HandleImpact    @0x824F2E48  (h:218)
//
// (HandleTookLead @0x8251E820 and HandleTookLast @0x8251E938 are BLOCKED, the same
//  const-mismatch gap wB_01 hit for HandleTraitorousTakedown: the frozen header declares
//  both `const`, but their faithful bodies must call the non-const
//  TriggerMessage(const GuiHudMessage*) overload -- a const method cannot. Reported in
//  funcs_blocked.)

namespace BrnGui
{

// @ 0x824F2E48 -- impact chatter. In BURNOUT_X360_ARTIST.XEX this handler compiles
// down to the payload/impact-type validation tripwires only; the happy path falls
// straight through to the epilogue with no message construction (the pseudocode's three
// StrStream blocks are the streamed asserts, not message logic). Reproduced faithfully
// (three non-gating asserts, all streamed on the X360 -> folded static).
void HudMessageAnalyzer::HandleImpact(const GuiImpactEvent* lpImpactEvent) const
{
    CGS_ASSERT(lpImpactEvent != NULL, "Invalid impact message");        // cpp:3006
    CGS_ASSERT(lpImpactEvent->meImpactType > 0, "Impact type invalid"); // cpp:3007
    CGS_ASSERT(lpImpactEvent->meImpactType < 9, "Impact type invalid"); // cpp:3008
}

} // namespace BrnGui
