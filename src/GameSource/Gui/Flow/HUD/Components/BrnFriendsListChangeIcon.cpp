#include "GameSource/Gui/Flow/HUD/Components/BrnFriendsListChangeIcon.h"

// BrnGui::FriendsListChangeIconComponent -- reconstructed from
// BURNOUT_X360_ARTIST.XEX (DWARF primary file
// GameSource/Gui/Flow/HUD/Components/BrnFriendsListChangeIcon.cpp).
//
// Bodied here (5 ledger functions):
//   Construct @0x82423CB0   Prepare @0x82423D10   AnimateIn @0x82414C50
//   ShowNow   @0x82414CA8   Hide    @0x82414D00
//
// The three latch methods drive the icon through the live SetState virtual
// (vtable slot 3, the asm's +0xC dispatch).

namespace BrnGui
{

// @ 0x82423CB0 -- the X360 re-inlines the base Construct content (the
// BrnGuiFlaptComponent.h:113 interface tripwire + the clip clear + the
// state-hash reset); expressed as the committed base call, then the latch clear.
void FriendsListChangeIconComponent::Construct(const void* lpDEBUGName,
                                               CgsGui::StateInterface* lpStateInterface,
                                               const void* lpcParentName)
{
    FlaptIconComponent::Construct(lpDEBUGName, lpStateInterface, lpcParentName);
    mbShowing = false;
}

// @ 0x82423D10 -- the base Prepare re-inlined verbatim (the
// BrnGuiFlaptIconComponent.cpp:65 / BrnGuiFlaptComponent.h:133 /
// BrnFlaptMovieClipRef.h:272 tripwires + the clip bind + ResetTimeline + the
// state-hash reset); no derived member is touched.
void FriendsListChangeIconComponent::Prepare(const char* lacName,
                                             const BrnFlapt::FileRef& lFile)
{
    // The X360 instance folds the parent prefix away (FindComponent is called
    // with the file + bare name only) -- the DWARF's 2-param derived shape.
    FlaptIconComponent::Prepare(lacName, lFile, 0);
}

// @ 0x82414C50 -- play "transin" once while hidden.
void FriendsListChangeIconComponent::AnimateIn()
{
    if (!mbShowing)
    {
        SetState("transin");   // the live vtable dispatch (slot 3)
        mbShowing = true;
    }
}

// @ 0x82414CA8 -- pop straight to "idle" while hidden (the init-wait path).
void FriendsListChangeIconComponent::ShowNow()
{
    if (!mbShowing)
    {
        SetState("idle");
        mbShowing = true;
    }
}

// @ 0x82414D00 -- drop to "invisible" while shown (the asm's == 1 compare).
void FriendsListChangeIconComponent::Hide()
{
    if (mbShowing)
    {
        SetState("invisible");
        mbShowing = false;
    }
}

}
