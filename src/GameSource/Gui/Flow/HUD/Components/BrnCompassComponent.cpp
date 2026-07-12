// ===================================================================================
// BrnGui::CompassComponent  -- implementation
//   class:BrnGui::CompassComponent  +  GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.cpp
//
//   UpdatePlayerMarkerState @ 0x8241EBA8   (class-home batch)
//   SetBearing              @ 0x8241EC38   (class-home batch)
//   SetMarkerPos            @ 0x8241ED10   (class-home batch)
//   Construct               @ 0x82411568
//   Prepare                 @ 0x8241F8A0
//   SetVisibility           @ 0x824115E8
//   ShowLandmarkOnCompass   @ 0x82428C68
//   ShowChallengeOnCompass  @ 0x82428CC0
//
// Reconstructed store-for-store from the X360 asm; DWARF-attested shape.
//
// STILL TODO (blocked, bodies left for a keystone wave -- declared in the header):
//   ShowPositionOnCompass  @ 0x8241FC10 -- inlines VMX over un-exported permute/const
//                                          rodata (unk_82181510 / unk_8204B610) + the
//                                          un-homed platform intrinsic XMVectorACos.
//   FormatDirectionLetters @ 0x82411640 -- indexes the un-exported per-language rodata
//                                          string tables off_82F248B8 / off_82F24918.
//   Update                 @ 0x8242E160 -- un-homed GuiTracker actively-tracked-landmark
//                                          accessors (GetActivelyTrackedLandmarks /
//                                          GetNumActivelyTrackedLandmarks -- no reconstructed
//                                          home, no disasm) + the GuiCache heading /
//                                          event-destination-landmark reads.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SnPrintf
#include "GameSource/Gui/BrnGuiCache.h"                     // GuiCache::GetLandmarkInfoFromIndex / GetFreeburnChallengeManager / GetWorldDataController
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"  // FreeburnChallengeManager::IsActive / GetCurrentAction; BrnResource::ChallengeListEntryAction
#include "GameSource/Gui/BrnGuiWorldDataController.h"        // WorldDataController::GetTriggerVolumeRegion
#include "SharedClasses/Trigger/BrnRegion.h"                // BrnTrigger::BoxRegion::GetPosition
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"           // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // BrnFlapt::MovieClipInstance::ResetTimeline
#include "GameSource/GameState/BrnGameStateTypes.h"         // BrnGameState::LandmarkIndex (complete)

// CgsSystem::HardwareSku::FindLanguage @0x8241FB4C is a namespace free function with no
// reconstructed header home (bodies live in CgsHardwareSku{PC,PS3}.cpp); declare the exact
// symbol here so Prepare can resolve the current SKU language.
namespace CgsSystem { namespace HardwareSku { s32 FindLanguage(); } }

namespace BrnGui
{
    // --------------------------------------------------------------------------
    // KAPC_PLAYER_ROUTE_STATES (BrnCompassComponent.h:105 / XEX .data off_82F248AC).
    // DWARF-attested class static; only [E_PLAYER_ROUTE_ON_COURSE] == "onTrack" is
    // X360-attested. CONSOLIDATOR: fill [E_PLAYER_ROUTE_OFF_COURSE] and
    // [E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS] from the .data string table -- the values
    // below marked TODO are placeholders and MUST NOT ship as-is.
    // --------------------------------------------------------------------------
    const char* const CompassComponent::KAPC_PLAYER_ROUTE_STATES[E_PLAYER_ROUTE_COUNT] =
    {
        "onTrack",   // [0] E_PLAYER_ROUTE_ON_COURSE            -- X360-attested
        "",          // [1] E_PLAYER_ROUTE_OFF_COURSE           -- TODO consolidator (XEX .data @0x82F248AC)
        "",          // [2] E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS -- TODO consolidator (XEX .data @0x82F248AC)
    };

    // @ 0x8241EBA8 -- when the player's on/off-route state changes, remember it and
    // play the matching animator frame (the per-state frame-name table, XEX .data
    // off_82F248AC). No-op when the state is unchanged.
    void CompassComponent::UpdatePlayerMarkerState(EPlayerRouteState lePlayerOnTrack)
    {
        CGS_ASSERT((E_PLAYER_ROUTE_ON_COURSE <= lePlayerOnTrack) && (E_PLAYER_ROUTE_COUNT > lePlayerOnTrack),
                   "(E_PLAYER_ROUTE_ON_COURSE <= lePlayerOnTrack) && (E_PLAYER_ROUTE_COUNT > lePlayerOnTrack)");

        if (lePlayerOnTrack != mePlayerOnTrack)
        {
            mePlayerOnTrack = lePlayerOnTrack;
            mPlayerMarkerAnimator.Run(KAPC_PLAYER_ROUTE_STATES[lePlayerOnTrack]);
        }
    }

    // @ 0x8241EC38 -- rotate the compass view clip so bearing (degrees, 0..360) maps
    // linearly onto the strip: X = (lfBearing/360) * mfSingleViewLength + initialX,
    // keeping the clip's initial Y. The X360 copies the whole initial-position vector
    // to a stack temp, overwrites only its X lane, then hands it to SetPosition.
    void CompassComponent::SetBearing(f32 lfBearing)
    {
        CGS_ASSERT(lfBearing >= -0.1, "lfBearing >= -0.1");            // BrnCompassComponent.h:298
        CGS_ASSERT(lfBearing <= 360.1f, "lfBearing <= 360.1f");        // BrnCompassComponent.h:299

        // flt_82004920 == 0.0027777778 == 1/360 (degrees -> [0,1] strip fraction).
        Vector2 lPos = mv2InitialViewPos;
        lPos.x = (lfBearing * (1.0f / 360.0f)) * mfSingleViewLength + mv2InitialViewPos.x;
        mCompassViewMovie.SetPosition(lPos);
    }

    // @ 0x8241ED10 -- place the destination marker on the compass strip when it should
    // be shown; otherwise just hide the marker clip. When shown, the marker bearing
    // (-180..180) maps linearly the same way SetBearing maps the compass view.
    void CompassComponent::SetMarkerPos(f32 lfMarkerBearing, bool lbShowMarker)
    {
        CGS_ASSERT(lfMarkerBearing >= -180.1, "lfMarkerBearing >= -180.1");     // BrnCompassComponent.h:335
        CGS_ASSERT(lfMarkerBearing <= 180.1f, "lfMarkerBearing <= 180.1f");     // BrnCompassComponent.h:336

        if (lbShowMarker)
        {
            // flt_82004920 == 0.0027777778 == 1/360.
            Vector2 lPos = mv2InitialDestMarkerPos;
            lPos.x = (lfMarkerBearing * (1.0f / 360.0f)) * mfSingleViewLength + mv2InitialDestMarkerPos.x;
            mDestMarkerMovie.SetPosition(lPos);
        }
        else
        {
            mDestMarkerMovie.SetVisible(lbShowMarker);
        }
    }

    // @ 0x82411568 -- adopt the state channel through the base component, construct the
    // embedded player-marker animator against that same channel, then clear the cache
    // pointer and default the route state. lacName / lacParentName / liParentAptLayer are
    // part of the shared component Construct signature but unused by the compass at this layer.
    void CompassComponent::Construct(const char* lacName, CgsGui::StateInterface* lpStateInterface,
                                     const char* lacParentName, s32 liParentAptLayer)
    {
        (void)lacName;
        (void)lacParentName;
        (void)liParentAptLayer;

        // BrnFlaptComponent::Construct: assert lpStateInterface (h:113), store it, SetInvalid the clip.
        BrnFlaptComponent::Construct(lpStateInterface);

        mPlayerMarkerAnimator.Construct(NULL, lpStateInterface, NULL);

        mpGuiCache      = NULL;
        mePlayerOnTrack = E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS;   // +0x48 == 2
    }

    // @ 0x824115E8 -- drive the component's own apt clip to the transition/steady label that
    // matches the requested visibility (immediate vs. animated).
    void CompassComponent::SetVisibility(bool lbVisible, bool lbImmediate)
    {
        if (lbVisible)
            mAptRef.GotoAndPlayLabel(lbImmediate ? "visible" : "transin");
        else
            mAptRef.GotoAndPlayLabel(lbImmediate ? "invisible" : "transout");
    }

    // @ 0x82428C68 -- fetch the sat-nav icon record for the landmark and place its world
    // position on the compass at the given player heading.
    void CompassComponent::ShowLandmarkOnCompass(BrnGameState::LandmarkIndex lLandmark, f32 lfBearing)
    {
        GuiEventUpdateSatNav::SatNavIconInfo lLandmarkInfo;
        mpGuiCache->GetLandmarkInfoFromIndex(lLandmark, &lLandmarkInfo);
        // The X360 VMX-copies the icon's 16-byte position lane straight into the vector arg;
        // narrow it to the Vector3 destination ShowPositionOnCompass takes.
        const Vector4& lv4Position = lLandmarkInfo.GetPositionLane();
        const Vector3 lv3Destination = { lv4Position.x, lv4Position.y, lv4Position.z, 0.0f };
        ShowPositionOnCompass(lv3Destination, lfBearing);
    }

    // @ 0x82428CC0 -- when a freeburn challenge is active and its current action has a
    // trigger-located target, resolve that trigger's world-space region and place it on
    // the compass, then flash the "fbcTarget" marker icon; returns true when it drew the
    // marker. The X360 inlines the manager's mpChallengeManager fetch (+assert),
    // FreeburnChallengeManager::IsActive (state in {INITIALISED, RUNNING, RESULTS}) and the
    // BoxRegion position read at the call site; de-inlined here to the named accessors.
    bool CompassComponent::ShowChallengeOnCompass(f32 lfBearing)
    {
        const FreeburnChallengeManager* lpChallengeManager = mpGuiCache->GetFreeburnChallengeManager();
        if (lpChallengeManager->IsActive())
        {
            const BrnResource::ChallengeListEntryAction* lpAction = lpChallengeManager->GetCurrentAction();

            const u8 luLocationIndex = 0;
            if ((lpAction->GetNumLocations() != 0)
                && (lpAction->GetLocationType(luLocationIndex)
                    == BrnResource::ChallengeListEntryAction::E_LOCATION_TYPE_TRIGGER))
            {
                const CgsID lTriggerID = lpAction->GetTriggerID(luLocationIndex);

                BrnTrigger::BoxRegion lBox;
                mpGuiCache->GetWorldDataController()->GetTriggerVolumeRegion(lTriggerID, &lBox);

                ShowPositionOnCompass(lBox.GetPosition(), lfBearing);
                mDestMarkerMovie.GotoAndPlayLabel("fbcTarget");
                return true;
            }
        }
        return false;
    }

    // @ 0x8241F8A0 -- one-time setup of the compass HUD from its apt file:
    //   * resolve this component and reset its timeline;
    //   * descend into the "CVParent_mc" container (reset it too);
    //   * bind the CompassView / DestMarker clips;
    //   * cache the strip origin (view + marker) and one view's on-screen width (used to
    //     map a bearing onto the strip);
    //   * build the player-marker animator's component name and Prepare it;
    //   * format the direction letters on all six view sub-clips for the SKU language.
    void CompassComponent::Prepare(const char* lacName, const BrnFlapt::FileRef& lFile)
    {
        CGS_ASSERT(lacName != NULL, "lacName != NULL");   // BrnGuiFlaptComponent.h:133

        BrnFlapt::MovieClipRef lComponent;
        mAptRef = *lFile.FindComponent(&lComponent, lacName);

        CGS_ASSERT(mAptRef.mpMovieClipInst != NULL, "mpMovieClipInst");   // BrnFlaptMovieClipRef.h:272
        mAptRef.mpMovieClipInst->ResetTimeline();

        BrnFlapt::MovieClipRef lCompassViewParent;
        mAptRef.FindChildMovieClip(&lCompassViewParent, "CVParent_mc");
        CGS_ASSERT(lCompassViewParent.mpMovieClipInst != NULL, "mpMovieClipInst");   // BrnFlaptMovieClipRef.h:272
        lCompassViewParent.mpMovieClipInst->ResetTimeline();

        BrnFlapt::MovieClipRef lTemp;
        mCompassViewMovie = *lCompassViewParent.FindChildMovieClip(&lTemp, "CompassView_mc");
        mDestMarkerMovie  = *lCompassViewParent.FindChildMovieClip(&lTemp, "DestMarker_mc");

        BrnFlapt::MovieClipRef lCompassViewFrontLeft;
        BrnFlapt::MovieClipRef lCompassViewFrontCentre;
        BrnFlapt::MovieClipRef lCompassViewFrontRight;
        BrnFlapt::MovieClipRef lCompassViewBackLeft;
        BrnFlapt::MovieClipRef lCompassViewBackCentre;
        BrnFlapt::MovieClipRef lCompassViewBackRight;
        mCompassViewMovie.FindChildMovieClip(&lCompassViewFrontLeft,   "LeftView_Front_mc");
        mCompassViewMovie.FindChildMovieClip(&lCompassViewFrontCentre, "CentreView_Front_mc");
        mCompassViewMovie.FindChildMovieClip(&lCompassViewFrontRight,  "RightView_Front_mc");
        mCompassViewMovie.FindChildMovieClip(&lCompassViewBackLeft,    "LeftView_Back_mc");
        mCompassViewMovie.FindChildMovieClip(&lCompassViewBackCentre,  "CentreView_Back_mc");
        mCompassViewMovie.FindChildMovieClip(&lCompassViewBackRight,   "RightView_Back_mc");

        mv2InitialViewPos       = mCompassViewMovie.GetPosition();
        mfSingleViewLength      = lCompassViewFrontRight.GetPosition().x
                                - lCompassViewFrontCentre.GetPosition().x;
        mv2InitialDestMarkerPos = mDestMarkerMovie.GetPosition();

        char lacTempString[32];
        CgsCore::SnPrintf(lacTempString, 31, "%s_%s", lacName, "playerIcon_anim");
        lacTempString[31] = '\0';
        mPlayerMarkerAnimator.Prepare(lacTempString, lFile, NULL);

        const CgsLanguage::ELanguage leLanguage =
            static_cast<CgsLanguage::ELanguage>(CgsSystem::HardwareSku::FindLanguage());
        CGS_ASSERT(leLanguage > CgsLanguage::E_LANGUAGE_INVALID,
                   "leLanguage > CgsLanguage::E_LANGUAGE_INVALID");   // BrnCompassComponent.cpp:247
        CGS_ASSERT(leLanguage < CgsLanguage::E_LANGUAGE_TOTAL,
                   "leLanguage < CgsLanguage::E_LANGUAGE_TOTAL");     // BrnCompassComponent.cpp:248

        FormatDirectionLetters(leLanguage, &lCompassViewFrontLeft);
        FormatDirectionLetters(leLanguage, &lCompassViewFrontCentre);
        FormatDirectionLetters(leLanguage, &lCompassViewFrontRight);
        FormatDirectionLetters(leLanguage, &lCompassViewBackLeft);
        FormatDirectionLetters(leLanguage, &lCompassViewBackCentre);
        FormatDirectionLetters(leLanguage, &lCompassViewBackRight);
    }
}
