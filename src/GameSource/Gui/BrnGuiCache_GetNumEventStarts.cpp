#include "GameSource/Gui/BrnGuiCache.h"
#include "GameSource/GameState/BrnGameStateSharedIO.h"   // SetUpAllEventStartsInterface (complete type)

// Split from BrnGuiCache.cpp at the l2 merge: that TU types the cache's embedded
// OptionsDataProfile via BrnGuiOptionsDataProfile.h, whose compile-only network/
// game-state slices clash with the REAL BrnGameStateSharedIO.h types this body needs.

namespace BrnGui
{
    // @0x824F8830 -- GuiCache::GetNumEventStarts (DWARF BrnGuiCache.h:801). Pure tail-forwarder:
    // X360 `addi r3, r3, 0x5690` (this + 0x5690) then `b sub_824F7688`, i.e. hands
    // &mSetUpAllEventStartsInterface (the SetUpAllEventStartsInterface embedded at GuiCache+0x5690)
    // to that interface's GetNumEventStarts() @0x824F7688 and tail-returns its result. No local assert.
    u32 GuiCache::GetNumEventStarts() const
    {
        const u8* lpBase = reinterpret_cast<const u8*>(this);
        const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface* lpEventStarts =
            reinterpret_cast<const BrnGameState::GameStateModuleIO::SetUpAllEventStartsInterface*>(
                lpBase + 0x5690);
        return lpEventStarts->GetNumEventStarts();
    }
}
