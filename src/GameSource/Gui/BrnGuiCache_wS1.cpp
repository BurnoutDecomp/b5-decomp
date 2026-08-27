// BrnGuiCache_wS1.cpp -- the road-rule-shot slice's GuiCache leg (stunt-race UI wave,
// 2026-08-27). Mounting BrnRoadRuleShotComponent.cpp -- so RaceMainHudState::OnEnter can
// stop linking against the inert Construct scaffold in BrnHudStatesLinkStubs.cpp -- pulls
// exactly two "bodies link from the GuiCache TU" rows onto the link closure:
//     GuiCache::GetRoadRuleShotOpponentARCI      (BrnGuiCache.h:791)
//     GuiCache::GetRoadRuleShotCapturedLineGate  (BrnGuiCache.h:797)
// Neither is an exported X360 function -- the console inlines both into their one reader,
// RoadRuleShotComponent::Snap @0x82415620, which is also where the offsets in the header
// come from:
//     if ( *(v8 + 44122) )                                 <- mbRoadRuleShotCapturedLineGate
//     for ( i = (v8 + 44436); *i != *(v8 + 44104); ... )   <- meRoadRuleShotOpponentARCI
// (v8 == the GuiCache; 44122 == +0xAC5A, 44104 == +0xAC48; 44436 == the records at
// +0xAC80 plus the record's meActiveRaceCarIndex at +0x114.) The asm reads each member
// once, with no bounds test and no assert, so the bodies are the bare named-member reads
// -- unlike the indexed accessors in BrnGuiCache_wB_02.cpp / _wB_06.cpp, which do carry
// the X360's range guards. Both members are already NAMED in BrnGuiCache.h (h:1479 /
// h:1481); no pad carving was needed and no neighbour moved.
//
// Homed in this partfile rather than BrnGuiCache.cpp purely for wave hygiene (that file
// is another agent's hot file); there is no include clash to work around.

#include "GameSource/Gui/BrnGuiCache.h"

namespace BrnGui
{
    // X360-inlined at Snap @0x82415620 (`lwz` of cache+44104, compared against each
    // online record's meActiveRaceCarIndex). Returns the raw latch -- the caller's scan
    // is what tolerates a stale / unmatched value.
    s32 GuiCache::GetRoadRuleShotOpponentARCI() const
    {
        return meRoadRuleShotOpponentARCI;
    }

    // X360-inlined at Snap @0x82415620 (`lbz` of cache+44122, branch-if-zero straight to
    // the return). The whole "CAPTURED_FOR <ruler>" gamertag line hangs off this byte.
    bool GuiCache::GetRoadRuleShotCapturedLineGate() const
    {
        return mbRoadRuleShotCapturedLineGate;
    }
}
