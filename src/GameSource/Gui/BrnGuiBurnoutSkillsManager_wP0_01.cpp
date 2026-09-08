#include "GameSource/Gui/BrnGuiBurnoutSkillsManager.h"

// ============================================================================
// GameSource/Gui/BrnGuiBurnoutSkillsManager_wP0_01.cpp
//
// BrnGui::BurnoutSkillsManager -- the single accessor the HUD player-position
// row needs, split into its own partfile so the mount of the position-table pair
// does not have to drag the whole manager TU in with it.
//
// GetCurrentSkill is declared in the owning header but has no standalone symbol --
// every call site inlined it. Recovered from the instance in
// PlayerPositionSingleComponent::RenderValue, whose today's-best arm reads it as a
// bare two-load chain (the cache's manager pointer, then meCurrentSkill at +0x1E0
// inside it) and immediately compares the result against the 12-entry today's-best
// wow-limit table's upper bound. One named load, no assert, no side effect.
// ============================================================================

namespace BrnGui
{
    BrnGameState::BurnoutSkillzData::EBurnoutSkillType BurnoutSkillsManager::GetCurrentSkill() const
    {
        return meCurrentSkill;
    }
}
