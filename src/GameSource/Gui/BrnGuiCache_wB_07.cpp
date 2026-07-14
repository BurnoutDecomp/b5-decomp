#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX (GuiCache accessor wave, part 07).
// Offline profile-event CgsArray accessors, each a thin read/index of one named far
// member guarded by the game's debug assert (CGS_ASSERT is a no-op in this build,
// matching the X360 release assert machinery). Offsets / branch senses are taken
// straight from the ARTIST asm; members are accessed BY NAME against the recovered
// GuiCache layout in BrnGuiCache.h.

namespace BrnGui
{
    // @ 0x82449820 -- live count of mProfileEventState.maEvents. The X360 asserts the
    // embedded CgsArray was Construct/Clear'd (count word @0x13B54 != the -1 sentinel),
    // then returns that count word (asm: *(this + 79324 + 1400) == miProfileEventsCount).
    u32 GuiCache::GetNumProfileEvents() const
    {
        CGS_ASSERT(miProfileEventsCount != -1, "Array used before Construct/Clear was called");
        return static_cast<u32>(miProfileEventsCount);
    }

    // @ 0x82449880 -- index mProfileEventState.maEvents. The X360 loads &maEvents
    // (mProfileEventStateStorage @0x135DC), asserts the array was Construct/Clear'd
    // (count word @0x13B54 != -1) and that luIndex is in range, then tail-forwards to the
    // Array<ProfileEvent,175>::operator[] which returns &maElements[luIndex]. The element
    // stride is 8 (ProfileEvent = { u32 muEventID; u16 muFlags; } + pad; 1400/175 == 8).
    const BrnProgression::ProfileEvent* GuiCache::GetProfileEvent(u32 luIndex) const
    {
        CGS_ASSERT(miProfileEventsCount != -1, "Array used before Construct/Clear was called");
        CGS_ASSERT(luIndex < static_cast<u32>(miProfileEventsCount),
                   "luIndex < mProfileEventState.maEvents.GetLength()");
        return reinterpret_cast<const BrnProgression::ProfileEvent*>(
            mProfileEventStateStorage + 8 * luIndex);
    }
}
