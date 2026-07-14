#include "GameSource/Gui/BrnGuiCache.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameSource/Gui/BrnGuiWorldDataController.h"   // complete WorldDataController (GetFreeburnChallengeList callee)

// Reconstructed from BURNOUT_X360_ARTIST.XEX. Three GuiCache event-snapshot accessors,
// each a thin read of one cached member guarded by the game's debug assert (CGS_ASSERT is
// a no-op in this build, matching the X360 release assert machinery). Offsets / branch
// senses are taken straight from the X360 ARTIST asm; members are accessed BY NAME against
// the recovered GuiCache layout in BrnGuiCache.h.

namespace BrnGui
{
    // @ 0x8240F018 -- resolve the freeburn challenge list through the owned world-data
    // controller. Asserts the controller is present, forwards to its accessor, then asserts
    // the returned list is non-null. The X360 passes the controller pointer straight to
    // WorldDataController::GetFreeburnChallengeList and tail-returns its r3 unchanged; the
    // GuiCache header forward-declares the return element as BrnResource::ChallengeList while
    // the controller header forward-declares the same opaque resource as BrnGui::ChallengeList,
    // so bridge the two boundary forward-decls with a pointer passthrough cast.
    const BrnResource::ChallengeList* GuiCache::GetFreeburnChallengeList() const
    {
        CGS_ASSERT(mpWorldDataController != nullptr, "mpWorldDataController");
        const BrnResource::ChallengeList* lpChallengeList =
            reinterpret_cast<const BrnResource::ChallengeList*>(
                mpWorldDataController->GetFreeburnChallengeList());
        CGS_ASSERT(lpChallengeList != nullptr, "lpChallengeList");
        return lpChallengeList;
    }

    // @ 0x8240F1C0 -- the checkpoint count for the current event. The X360 only requires a
    // positive count (asserts 0 < muCheckpointsInEvent) for the checkpoint-carrying race
    // modes; the nested mode-tests skip the assert entirely when meGameModeType is in
    // {2,3,4,7,9,12,14,15,16,17}. Value read regardless.
    u8 GuiCache::GetCheckpointsInEvent() const
    {
        const s32 leMode = meGameModeType;
        // asm nesting: outer {3,9,7,4}, then {14,12,17}, then {2,16}, then {15,16}.
        const bool lbCheckpointCountRequired =
            (leMode != 3) && (leMode != 9) && (leMode != 7) && (leMode != 4)
            && (leMode != 14) && (leMode != 12) && (leMode != 17)
            && (leMode != 2) && (leMode != 16) && (leMode != 15);
        CGS_ASSERT(!lbCheckpointCountRequired || (0 < muCheckpointsInEvent),
                   "0 < muCheckpointsInEvent");
        return static_cast<u8>(muCheckpointsInEvent);
    }

    // @ 0x8240F398 -- the distance-to-go for the current event. The X360 builds the assert
    // text dynamically ("Event Distance=" + the value) before firing; the assert is a no-op
    // here, so the guard collapses to the branch sense (fires when the distance is negative).
    f32 GuiCache::GetDistanceInEvent() const
    {
        CGS_ASSERT(0.0f <= mfDistanceInEvent, "0.0f <= mfDistanceInEvent (Event Distance)");
        return mfDistanceInEvent;
    }
}
