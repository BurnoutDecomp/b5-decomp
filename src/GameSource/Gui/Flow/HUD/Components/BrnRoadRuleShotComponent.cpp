#include "GameSource/Gui/Flow/HUD/Components/BrnRoadRuleShotComponent.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"                     // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                // CgsCore::SPrintf
#include "GameShared/GameClasses/Gui/Model/State/CgsGuiStateInterface.h" // StateInterface::GetAccessPointers
#include "GameShared/GameClasses/Gui/CgsGuiShared.h"                   // GuiAccessPointers::GetGuiCache
#include "GameSource/Gui/BrnGuiCache.h"                                // the road-rule-shot block + player records
#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h" // InGamePlayerStatusData

// BrnGui::RoadRuleShotComponent -- reconstructed from BURNOUT_X360_ARTIST.XEX
// (DWARF primary file BrnRoadRuleShotComponent.cpp; member/constant names
// verbatim from the DecFIGS DWARF).
//
// Bodied here (6 ledger functions):
//   Construct @0x82424600   Prepare @0x82424690   SetupComponent @0x824151F0
//   Show      @0x82415598   Snap    @0x82415620   Hide           @0x82415790

namespace BrnGui
{

namespace
{
    // DWARF cpp:23 -- the per-component-state frame names (X360 off_82F24F60;
    // both strings read from the XEX).
    const char* KAPC_COMPONENT_STATE_FRAME_NAMES[RoadRuleShotComponent::E_COMPONENT_STATE_COUNT] =
    {
        "Ruler",   // E_COMPONENT_STATE_NEW_RULER
        "Loser",   // E_COMPONENT_STATE_OLD_LOSER
    };

    // DWARF cpp:29-:37 -- the template string id + the clip/textfield names.
    const char KAC_TEMPLATE_ON_ROAD_STRING_ID[21]        = "HUDMESSAGE_ON_PLAYER";
    const char KAC_GAMERTAG_TEXTFIELD_MOVIECLIP_NAME[12] = "gamertag_mc";
    const char KAC_ROAD_NAME_TEXTFIELD_MOVIECLIP_NAME[12] = "roadName_mc";
    const char KAC_RULE_STATE_TEXTFIELD_MOVIECLIP_NAME[12] = "RuleText_mc";
    const char KAC_GAMERTAG_TEXTFIELD_NAME[13]           = "gamertag_txt";
    const char KAC_ROAD_NAME_TEXTFIELD_NAME[13]          = "roadName_txt";
    const char KAC_RULE_STATE_TEXTFIELD_NAME[13]         = "RuleText_txt";
}

// @ 0x82424600 -- cpp:58. The X360 re-inlines the base icon Construct (the
// BrnGuiFlaptComponent.h:113 interface tripwire + the clip clear + the
// state-hash reset); expressed as the committed base call, then the derived
// latches: no component state yet, and the three text-field refs cleared
// (the anim-clip ref is left as Construct found it, per the asm).
void RoadRuleShotComponent::Construct(const void* lpDEBUGName,
                                      CgsGui::StateInterface* lpStateInterface,
                                      const void* lpcParentName)
{
    FlaptIconComponent::Construct(lpDEBUGName, lpStateInterface, lpcParentName);
    mpcComponentState = 0;
    meComponentState  = E_COMPONENT_STATE_INVALID;
    mGamertagTextfield.mpTextFieldInstance  = 0;
    mGamertagTextfield.mpParentMovie        = 0;
    mGamertagTextfield.mpTransform          = 0;
    mRoadNameTextfield.mpTextFieldInstance  = 0;
    mRoadNameTextfield.mpParentMovie        = 0;
    mRoadNameTextfield.mpTransform          = 0;
    mRuleStateTextfield.mpTextFieldInstance = 0;
    mRuleStateTextfield.mpParentMovie       = 0;
    mRuleStateTextfield.mpTransform         = 0;
}

// @ 0x82424690 -- cpp:92. The base icon Prepare re-inlined verbatim (the
// BrnGuiFlaptIconComponent.cpp:65 / BrnGuiFlaptComponent.h:133 /
// BrnFlaptMovieClipRef.h:272 tripwires + the clip bind + ResetTimeline + the
// state-hash reset); the parent prefix is folded away (FindComponent gets the
// bare name), giving the DWARF's 2-param derived shape.
void RoadRuleShotComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
{
    FlaptIconComponent::Prepare(lacName, lFile, 0);
}

// @ 0x824151F0 -- cpp:148. Configure the shot for a rule change: validate the
// player state (validated ONLY -- the body never reads it again, faithful),
// format the road id, latch the Ruler/Loser frame from the ruler flag and jump
// the icon clip to it, then resolve the anim clip's three text-field wells
// (each: find the "<x>_mc" marker recursively, hide it, and bind the "<x>_txt"
// field beside it in its parent) and fill the three lines.
void RoadRuleShotComponent::SetupComponent(EPlayerState lePlayerState,
                                           bool lbLocalPlayerNewRuler,
                                           const char* lpcGamertag,
                                           CgsID lRoadId, bool lbTimeRule)
{
    // The X360 streams the offending value into the assert text; folded static
    // (the family precedent).
    CGS_ASSERT(lePlayerState < E_PLAYER_STATE_COUNT,
               "Invalid EPlayerState : <lePlayerState>");   // :150 (non-gating, streamed)

    char lacRoadIdText[24];
    CgsCore::SPrintf(lacRoadIdText, 12, "%llu", lRoadId);
    lacRoadIdText[12] = 0;

    const char* lpcRuleStateStringId;
    if (lbLocalPlayerNewRuler)
    {
        meComponentState    = E_COMPONENT_STATE_NEW_RULER;
        lpcRuleStateStringId = lbTimeRule ? "SMUG_SHOT_YOU_WON_TIME"
                                          : "SMUG_SHOT_YOU_WON_CRASH";
    }
    else
    {
        meComponentState    = E_COMPONENT_STATE_OLD_LOSER;
        lpcRuleStateStringId = lbTimeRule ? "SMUG_SHOT_YOU_LOST_TIME"
                                          : "SMUG_SHOT_YOU_LOST_CRASH";
    }

    mpcComponentState = KAPC_COMPONENT_STATE_FRAME_NAMES[meComponentState];
    SetState(mpcComponentState);   // the live vtable dispatch (slot 3)

    mAptRef.FindChildMovieClipOnFrame(&mAnimationMovieClip, "anim_mc");

    // ---- the gamertag well ----
    BrnFlapt::MovieClipRef lTextFieldComponent;
    BrnFlapt::MovieClipRef lParent;
    CGS_ASSERT(mAnimationMovieClip.TryFindChildComponentRecursively(
                   KAC_GAMERTAG_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent),
               "mAnimationMovieClip.TryFindChildComponentRecursively( KAC_GAMERTAG_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent )");   // :197 (non-gating)
    lTextFieldComponent.SetVisible(false);
    lTextFieldComponent.GetParent(&lParent);
    lParent.FindChildTextField(&mGamertagTextfield, KAC_GAMERTAG_TEXTFIELD_NAME);

    // ---- the road-name well ----
    CGS_ASSERT(mAnimationMovieClip.TryFindChildComponentRecursively(
                   KAC_ROAD_NAME_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent),
               "mAnimationMovieClip.TryFindChildComponentRecursively( KAC_ROAD_NAME_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent )");   // :202 (non-gating)
    lTextFieldComponent.SetVisible(false);
    lTextFieldComponent.GetParent(&lParent);
    lParent.FindChildTextField(&mRoadNameTextfield, KAC_ROAD_NAME_TEXTFIELD_NAME);

    // ---- the rule-state well ----
    CGS_ASSERT(mAnimationMovieClip.TryFindChildComponentRecursively(
                   KAC_RULE_STATE_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent),
               "mAnimationMovieClip.TryFindChildComponentRecursively( KAC_RULE_STATE_TEXTFIELD_MOVIECLIP_NAME, &lTextFieldComponent )");   // :207 (non-gating)
    lTextFieldComponent.SetVisible(false);
    lTextFieldComponent.GetParent(&lParent);
    lParent.FindChildTextField(&mRuleStateTextfield, KAC_RULE_STATE_TEXTFIELD_NAME);

    // ---- the three lines (the varargs SetLocalisedText, one (value, format)
    // pair each; the rule-state line is the plain id form) ----
    mGamertagTextfield.SetLocalisedText(
        lbLocalPlayerNewRuler ? "STRIKE_A_POSE" : "HUDMESSAGE_YELLOW_STRING",
        9, 1, lpcGamertag, 0);
    mRoadNameTextfield.SetLocalisedText(KAC_TEMPLATE_ON_ROAD_STRING_ID,
                                        9, 1, lacRoadIdText, 9);
    mRuleStateTextfield.SetLocalisedText(lpcRuleStateStringId, 9);
}

// @ 0x82415598 -- cpp:253.
void RoadRuleShotComponent::Show(bool lbPose)
{
    CGS_ASSERT(0 != mpcComponentState, "NULL != mpcComponentState");   // :255 (non-gating)
    mAnimationMovieClip.GotoAndPlayLabel(lbPose ? "pose" : "transin");
}

// @ 0x82415620 -- cpp:295. Jump the anim clip to "showing"/"snap", then -- when
// the cache's captured-line gate is set -- scan the eight online player records
// for the shot opponent's race car and fill the gamertag line with
// "CAPTURED_FOR <that record's player name>".
void RoadRuleShotComponent::Snap(bool lbShowing)
{
    CGS_ASSERT(0 != mpcComponentState, "NULL != mpcComponentState");   // :297 (non-gating)
    mAnimationMovieClip.GotoAndPlayLabel(lbShowing ? "showing" : "snap");

    // The asserting access chain (CgsGuiStateInterface.h:344 "mpAccessPointers
    // != NULL" / CgsGuiShared.h:201 "mpGuiCache", both non-gating).
    GuiCache* lpGuiCache = mpStateInterface->GetAccessPointers()->GetGuiCache();

    if (lpGuiCache->GetRoadRuleShotCapturedLineGate())
    {
        // The inlined find-record-by-race-car scan (first match of eight; the
        // opponent index is read once, before the loop).
        const s32 liOpponentCar = lpGuiCache->GetRoadRuleShotOpponentARCI();
        const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpRulerRecord = 0;
        for (s32 liPlayer = 0; ; )
        {
            const BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData* lpInfo =
                lpGuiCache->GetOnlinePlayerInfo(liPlayer);
            if (lpInfo->meActiveRaceCarIndex == liOpponentCar)
            {
                lpRulerRecord = lpInfo;
                break;
            }
            if (++liPlayer >= 8)
                break;   // no record matched; lpRulerRecord stays null
        }

        // FAITHFUL QUIRK: the X360 offsets to the record's player name (+256)
        // with NO null guard -- a missed scan hands SetLocalisedText the bare
        // +0x100 offset as the text pointer. Reproduced verbatim.
        const char* lpcRulerName = reinterpret_cast<const char*>(
            reinterpret_cast<const u8*>(lpRulerRecord) + 256);
        mGamertagTextfield.SetLocalisedText("CAPTURED_FOR", 9, 1, lpcRulerName, 0);
    }
}

// @ 0x82415790 -- cpp:349.
void RoadRuleShotComponent::Hide()
{
    if (meComponentState != E_COMPONENT_STATE_INVALID)
        mAnimationMovieClip.GotoAndPlayLabel("transout");
}

}
