#ifndef GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
#define GAMESOURCE_RESOURCE_BRNDLCMANAGER_H

#include "types.hpp"

// ============================================================================
// GameSource/Resource/BrnDLCManager.h
//
// MINIMAL SLICE -- models only the downloadable-content availability flags that
// the boot-legal flow touches. The full DLC manager (BrnResource::DLCManager and
// its package list, the DLC debug component, and the feature-availability table)
// lands when those TUs are reconstructed; GROW this header then, do NOT fork the
// DLC types elsewhere.
//
// Shape recovered from the X360 asm of DLCBeatTheTeamGame::SetEnabledState
// (@0x82472CA8) and its baked assert string at BrnDLCManager.h:405
// ("!(mbIsEnabled && !mbIsAvailable)"): each downloadable game is a two-flag
// record -- availability (whether the content is installed/owned) and enabled
// (whether the gameplay path that uses it is switched on). The invariant the
// assert enforces is that a game cannot be enabled while it is unavailable.
// ============================================================================

namespace BrnResource
{

// A single downloadable "Beat The Team" online game's availability/enabled state.
// Layout recovered from SetEnabledState: mbIsAvailable @ +0x00, mbIsEnabled @ +0x01.
class DLCBeatTheTeamGame
{
public:
    bool IsAvailable() const { return mbIsAvailable; }
    bool IsEnabled() const   { return mbIsEnabled; }

    // X360 0x82472CA8. Stores the enabled flag, then asserts that the content is not
    // enabled while it is unavailable (BrnDLCManager.h:405). The assert is non-fatal
    // (the binary stores the flag and returns regardless).
    void SetEnabledState(bool lbEnabled);

private:
    bool mbIsAvailable; // +0x00
    bool mbIsEnabled;   // +0x01
};

} // namespace BrnResource

#endif // GAMESOURCE_RESOURCE_BRNDLCMANAGER_H
