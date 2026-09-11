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
//   ShowPositionOnCompass                  (p0 wave 2026-09-08)
//   FormatDirectionLetters                 (p0 wave 2026-09-08)
//   Update                                 (p1 wave 2026-09-11)
//
// Reconstructed store-for-store from the console listing; attested declaration shape.
//
// [p0 wave 2026-09-08] The two leaves above were parked as "un-reconstructable rodata".
// They are not: the const-vector operands are the ordinary compass frame (north = +Z,
// up = +Y), the two scalars beside them are 2*pi and 180/pi, and the per-language
// direction-letter tables read straight out of the shipped image (see below). The
// player-route frame-name table was likewise a two-entry placeholder and is now the
// real triple. Nothing here is invented; every value is read from the image.
// ===================================================================================
#include "GameSource/Gui/Flow/HUD/Components/BrnCompassComponent.h"

#include <cmath>                                            // acosf / fabsf (the bearing derivation)

#include "rw/math/vpu/vector3_operation.h"                  // Dot / Cross / Normalize over Vector3
#include "GameShared/GameClasses/Core/CgsAssert.h"          // CGS_ASSERT
#include "GameShared/GameClasses/Development/Log/CgsLog.h"  // gpDebugPrint (the one-shot mount witness)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"     // CgsCore::SnPrintf
#include "GameSource/Gui/BrnGuiCache.h"                     // GuiCache::GetLandmarkInfoFromIndex / GetFreeburnChallengeManager / GetWorldDataController
#include "GameSource/Gui/BrnGuiEventTypeDefs.h"             // GuiEventUpdateSatNav::SatNavIconInfo
#include "GameSource/Gui/BrnGuiFreeburnChallengeManager.h"  // FreeburnChallengeManager::IsActive / GetCurrentAction; BrnResource::ChallengeListEntryAction
#include "GameSource/Gui/BrnGuiWorldDataController.h"        // WorldDataController::GetTriggerVolumeRegion
#include "SharedClasses/Trigger/BrnRegion.h"                // BrnTrigger::BoxRegion::GetPosition
#include "GameSource/Gui/Flapt/BrnFlaptFileRef.h"           // BrnFlapt::FileRef::FindComponent
#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h" // BrnFlapt::MovieClipInstance::ResetTimeline
#include "GameSource/GameState/BrnGameStateTypes.h"         // BrnGameState::LandmarkIndex (complete)
#include "GameSource/Gui/SatNav/BrnGuiTracker.h"            // GuiTracker::Get{,Num}ActivelyTrackedLandmarks (Update's online arm)

// The game-mode enum Update switches on; BrnGuiCache.h already pulls the owning header in.
namespace GsmIO = BrnGameState::GameStateModuleIO;

// CgsSystem::HardwareSku::FindLanguage @0x8241FB4C is a namespace free function with no
// reconstructed header home (bodies live in CgsHardwareSku{PC,PS3}.cpp); declare the exact
// symbol here so Prepare can resolve the current SKU language.
namespace CgsSystem { namespace HardwareSku { s32 FindLanguage(); } }

namespace BrnGui
{
    // --------------------------------------------------------------------------
    // KAPC_PLAYER_ROUTE_STATES (BrnCompassComponent.h:105) -- an attested class static;
    // the per-route-state animator frame names.
    //
    // [p0 wave 2026-09-08] The last two entries stood here as empty-string placeholders
    // carrying a "MUST NOT ship as-is" note. They are no longer placeholders: the whole
    // three-pointer table reads out of the shipped image's constant data, and the slot
    // immediately past its end is the first entry of the direction-letter table below,
    // which pins the array length at exactly three. UpdatePlayerMarkerState now runs for
    // real (ShowPositionOnCompass calls it), so an empty label would have been a live
    // lookup miss on every off-course frame.
    // --------------------------------------------------------------------------
    const char* const CompassComponent::KAPC_PLAYER_ROUTE_STATES[E_PLAYER_ROUTE_COUNT] =
    {
        "onTrack",    // [0] E_PLAYER_ROUTE_ON_COURSE
        "offTrack",   // [1] E_PLAYER_ROUTE_OFF_COURSE
        "neither",    // [2] E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS
    };

    // --------------------------------------------------------------------------
    // The per-language direction-letter frame names FormatDirectionLetters plays on the
    // East_mc / West_mc children of every compass view sub-clip. Two parallel tables of
    // CgsLanguage::E_LANGUAGE_TOTAL const char*, indexed by the SKU language, read out of
    // the shipped image's constant data. Almost every slot points at the same one-letter
    // string; the interesting rows are the four that do not:
    //   * German  -- "Ost"   -> East letter is 'O' (its West letter stays 'W').
    //   * French  -- "Ouest" -> West letter is 'O'.
    //   * Italian -- "Ovest" -> West letter is 'O'.
    //   * Spanish -- "Oeste" -> West letter is 'O'.
    // That pattern is itself the corroboration that the tables were read at the right
    // stride and the right base: the four exceptions land exactly on the four languages
    // whose compass words begin with O, and nowhere else.
    // --------------------------------------------------------------------------
    namespace
    {
        const char* const KAPC_FRAMES_EAST[CgsLanguage::E_LANGUAGE_TOTAL] =
        {
            "E",  // E_LANGUAGE_ARABIC
            "E",  // E_LANGUAGE_CHINESE
            "E",  // E_LANGUAGE_CHINESE_SIMPLIFIED
            "E",  // E_LANGUAGE_CHINESE_TRADITIONAL
            "E",  // E_LANGUAGE_CZECH
            "E",  // E_LANGUAGE_DANISH
            "E",  // E_LANGUAGE_DUTCH
            "E",  // E_LANGUAGE_ENGLISH_US
            "E",  // E_LANGUAGE_ENGLISH_UK
            "E",  // E_LANGUAGE_FINNISH
            "E",  // E_LANGUAGE_FRENCH
            "O",  // E_LANGUAGE_GERMAN                -- "Ost"
            "E",  // E_LANGUAGE_GREEK
            "E",  // E_LANGUAGE_HEBREW
            "E",  // E_LANGUAGE_HUNGARIAN
            "E",  // E_LANGUAGE_ITALIAN
            "E",  // E_LANGUAGE_JAPANESE
            "E",  // E_LANGUAGE_KOREAN
            "E",  // E_LANGUAGE_NORWEGIAN
            "E",  // E_LANGUAGE_POLISH
            "E",  // E_LANGUAGE_PORTUGUESE_BRAZIL
            "E",  // E_LANGUAGE_PORTUGUESE_PORTUGAL
            "E",  // E_LANGUAGE_SPANISH
            "E",  // E_LANGUAGE_SWEDISH
        };

        const char* const KAPC_FRAMES_WEST[CgsLanguage::E_LANGUAGE_TOTAL] =
        {
            "W",  // E_LANGUAGE_ARABIC
            "W",  // E_LANGUAGE_CHINESE
            "W",  // E_LANGUAGE_CHINESE_SIMPLIFIED
            "W",  // E_LANGUAGE_CHINESE_TRADITIONAL
            "W",  // E_LANGUAGE_CZECH
            "W",  // E_LANGUAGE_DANISH
            "W",  // E_LANGUAGE_DUTCH
            "W",  // E_LANGUAGE_ENGLISH_US
            "W",  // E_LANGUAGE_ENGLISH_UK
            "W",  // E_LANGUAGE_FINNISH
            "O",  // E_LANGUAGE_FRENCH                -- "Ouest"
            "W",  // E_LANGUAGE_GERMAN
            "W",  // E_LANGUAGE_GREEK
            "W",  // E_LANGUAGE_HEBREW
            "W",  // E_LANGUAGE_HUNGARIAN
            "O",  // E_LANGUAGE_ITALIAN               -- "Ovest"
            "W",  // E_LANGUAGE_JAPANESE
            "W",  // E_LANGUAGE_KOREAN
            "W",  // E_LANGUAGE_NORWEGIAN
            "W",  // E_LANGUAGE_POLISH
            "W",  // E_LANGUAGE_PORTUGUESE_BRAZIL
            "W",  // E_LANGUAGE_PORTUGUESE_PORTUGAL
            "O",  // E_LANGUAGE_SPANISH               -- "Oeste"
            "W",  // E_LANGUAGE_SWEDISH
        };

        // The compass's world frame, both read as 16-byte constant vectors beside the
        // bearing math: NORTH is +Z and UP is +Y -- the very same pair the sat-nav icon
        // heading uses (GameBridgeWorldToGui's rotation derivation), so a compass bearing
        // and a minimap arrow agree by construction.
        const Vector3 KV3_COMPASS_NORTH = { 0.0f, 0.0f, 1.0f, 0.0f };
        const Vector3 KV3_COMPASS_UP    = { 0.0f, 1.0f, 0.0f, 0.0f };

        const f32 KF_TWO_PI             = 6.2831855f;    // the acos sign flip
        const f32 KF_RADIANS_TO_DEGREES = 57.29578f;
        const f32 KF_DEGREES_PER_TURN   = 360.0f;
        const f32 KF_DEGREES_HALF_TURN  = 180.0f;

        // The two route-state thresholds, in degrees off the destination.
        const f32 KF_ON_COURSE_LIMIT  = 22.5f;
        const f32 KF_OFF_COURSE_LIMIT = 157.5f;

        // The destination marker never leaves the visible strip: the relative bearing is
        // clamped to +/-120 degrees before it is handed to SetMarkerPos (console fsel pair).
        const f32 KF_MARKER_BEARING_MIN = -120.0f;
        const f32 KF_MARKER_BEARING_MAX =  120.0f;
    }

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

    // ShowPositionOnCompass -- place a world-space destination on the compass strip.
    //
    // The console body is one long VMX pipeline; de-optimised it is the ordinary
    // "which way is that, relative to where I am pointing" derivation:
    //
    //   1. take the direction from the world camera to the destination and normalise it
    //      (the camera position is the far 16-byte lane the sat-nav renderer also reads,
    //      GuiCache::GetWorldCameraPosition -- so the compass and the minimap share one
    //      reference point);
    //   2. the unsigned angle to north is acos of the clamped dot product. The clamp is a
    //      vmaxfp/vminfp pair against -1 / +1, in that order, and exists because the
    //      normalise leaves the dot a hair outside the domain;
    //   3. the SIGN comes from cross(NORTH, toDestination) . UP -- north is the LEFT
    //      operand. That is the same convention as the sat-nav icon heading, and the same
    //      trap: flipping the operands negates every bearing and mirrors the strip. With
    //      north = +Z and up = +Y the triple product reduces to the normalised direction's
    //      own x lane, which is exactly what the console computes by summing the cross
    //      against its up-vector constant. Below the plane the angle becomes 2*pi - angle;
    //   4. convert to degrees and add half a turn (the strip's zero sits half a turn from
    //      north), then wrap into [0, 360];
    //   5. subtract that from the player heading and wrap into [-180, 180] -- the marker's
    //      bearing relative to the direction the player faces;
    //   6. that relative bearing drives BOTH outputs: the coarse on/off-route player-marker
    //      state, and the clamped marker position.
    //
    // The wrap loops are the console's own compare-and-branch pairs, kept as loops. Both
    // are written as ordered compares so an unordered operand exits, matching the PPC
    // branch senses (bge/ble are taken when unordered).
    void CompassComponent::ShowPositionOnCompass(Vector3 lv3Destination, f32 lfBearing)
    {
        const Vector4& lrv4CameraPosition = mpGuiCache->GetWorldCameraPosition();
        const Vector3 lv3CameraPosition = { lrv4CameraPosition.x, lrv4CameraPosition.y,
                                            lrv4CameraPosition.z, 0.0f };

        const Vector3 lv3North         = Normalize(KV3_COMPASS_NORTH);
        const Vector3 lv3ToDestination = Normalize(lv3Destination - lv3CameraPosition);

        f32 lfDot = Dot(lv3North, lv3ToDestination);
        if (lfDot < -1.0f)
            lfDot = -1.0f;
        if (lfDot > 1.0f)
            lfDot = 1.0f;

        f32 lfAngle = acosf(lfDot);
        if (Dot(Cross(lv3North, lv3ToDestination), KV3_COMPASS_UP) < 0.0f)
            lfAngle = KF_TWO_PI - lfAngle;

        f32 lfDestinationBearing = lfAngle * KF_RADIANS_TO_DEGREES + KF_DEGREES_HALF_TURN;
        while (lfDestinationBearing < 0.0f)
            lfDestinationBearing += KF_DEGREES_PER_TURN;
        while (lfDestinationBearing > KF_DEGREES_PER_TURN)
            lfDestinationBearing -= KF_DEGREES_PER_TURN;

        f32 lfRelativeBearing = lfBearing - lfDestinationBearing;
        while (lfRelativeBearing < -KF_DEGREES_HALF_TURN)
            lfRelativeBearing += KF_DEGREES_PER_TURN;
        while (lfRelativeBearing > KF_DEGREES_HALF_TURN)
            lfRelativeBearing -= KF_DEGREES_PER_TURN;

        // Within 22.5 degrees of the destination the player is on course; past 157.5 the
        // destination is behind them and they are off course; between the two the console
        // reports the middle state. The 157.5 test is the console's own greater-than, so a
        // NaN bearing lands in the middle state, not off course.
        EPlayerRouteState lePlayerOnTrack;
        if (fabsf(lfRelativeBearing) < KF_ON_COURSE_LIMIT)
            lePlayerOnTrack = E_PLAYER_ROUTE_ON_COURSE;
        else if (fabsf(lfRelativeBearing) > KF_OFF_COURSE_LIMIT)
            lePlayerOnTrack = E_PLAYER_ROUTE_OFF_COURSE;
        else
            lePlayerOnTrack = E_PLAYER_ROUTE_WITHIN_NORMAL_BOUNDS;
        UpdatePlayerMarkerState(lePlayerOnTrack);

        f32 lfMarkerBearing = lfRelativeBearing;
        if (lfMarkerBearing < KF_MARKER_BEARING_MIN)
            lfMarkerBearing = KF_MARKER_BEARING_MIN;
        if (lfMarkerBearing > KF_MARKER_BEARING_MAX)
            lfMarkerBearing = KF_MARKER_BEARING_MAX;

        SetMarkerPos(lfMarkerBearing, true);
    }

    // FormatDirectionLetters -- stamp the localised East/West letters onto one compass view sub-clip.
    // Each sub-clip owns an East_mc and a West_mc child whose timelines carry a labelled
    // frame per letter; the SKU language picks the label out of the two tables above. The
    // component's own `this` is untouched -- the console body reads only its two arguments.
    void CompassComponent::FormatDirectionLetters(CgsLanguage::ELanguage leLanguage,
                                                  BrnFlapt::MovieClipRef* lpMovieClipRef)
    {
        BrnFlapt::MovieClipRef lEastMovie;
        BrnFlapt::MovieClipRef lWestMovie;
        lpMovieClipRef->FindChildMovieClip(&lEastMovie, "East_mc");
        lpMovieClipRef->FindChildMovieClip(&lWestMovie, "West_mc");

        lEastMovie.GotoAndPlayLabel(KAPC_FRAMES_EAST[leLanguage]);
        lWestMovie.GotoAndPlayLabel(KAPC_FRAMES_WEST[leLanguage]);
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

        // [p0-compass] one-shot mount witness (existing convention). Retire once the
        // compass has been seen drawing in a live event.
        {
            static bool s_bLoggedConstruct = false;
            if (!s_bLoggedConstruct)
            {
                s_bLoggedConstruct = true;
                *CgsDev::Log::gpDebugPrint << "[p0-compass] CompassComponent::Construct -- real TU\n";
            }
        }

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
        {
            static bool s_bLoggedSetVisibility = false;
            if (!s_bLoggedSetVisibility)
            {
                s_bLoggedSetVisibility = true;
                *CgsDev::Log::gpDebugPrint << "[p0-compass] CompassComponent::SetVisibility visible="
                                           << (lbVisible ? 1 : 0) << " immediate="
                                           << (lbImmediate ? 1 : 0) << "\n";
            }
        }

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

    // Update -- the per-frame compass drive. Two halves:
    //
    //   1. scroll the strip to where the player is pointing. The cache's player
    //      orientation is in RADIANS; the strip's zero sits half a turn from north, so the
    //      heading is orientation*180/pi + 180, wrapped into [0, 360]. The two wraps are
    //      the console's own compare-and-branch pairs kept as loops, written as ordered
    //      compares so an unordered (NaN) operand falls straight out of both -- which is
    //      what the PPC `bge`/`ble` exits do, and is the polarity that matters here: the
    //      second loop's repeat branch is `bgt`, so a NaN heading must NOT spin.
    //
    //   2. decide what the destination marker shows. Outside an event (the cache's
    //      in-event gate byte clear) and in every mode with no compass destination, the
    //      marker is parked and hidden. Otherwise the game mode picks the source:
    //        * offline race / burning route / marked man -- the event's destination
    //          landmark, drawn as the "finish" marker;
    //        * online race / online road rage -- the first of the sat-nav tracker's
    //          actively-tracked landmarks, drawn as "finish" when it is the only one left
    //          and "checkpoint" while more follow;
    //        * the freeburn-challenge mode -- the active challenge's trigger location,
    //          which draws its own "fbcTarget" marker and reports whether it drew one.
    //      Only the challenge arm can fall through to the hide path (when no challenge is
    //      running); the other two return with the marker placed.
    void CompassComponent::Update()
    {
        CGS_ASSERT(mpGuiCache != NULL, "mpGuiCache");

        f32 lfBearing = mpGuiCache->GetPlayerOrientation() * KF_RADIANS_TO_DEGREES
                      + KF_DEGREES_HALF_TURN;
        while (lfBearing < 0.0f)
            lfBearing += KF_DEGREES_PER_TURN;
        while (lfBearing > KF_DEGREES_PER_TURN)
            lfBearing -= KF_DEGREES_PER_TURN;

        SetBearing(lfBearing);

        if (mpGuiCache->GetInEventColouringGate())
        {
            switch (mpGuiCache->GetGameMode())
            {
                case GsmIO::E_MODE_OFFLINE_RACE:
                case GsmIO::E_MODE_BURNING_ROUTE:
                case GsmIO::E_MODE_MARKED_MAN:
                {
                    ShowLandmarkOnCompass(mpGuiCache->GetEventDestinationLandmarkIndex(), lfBearing);
                    mDestMarkerMovie.GotoAndPlayLabel("finish");
                    return;
                }

                case GsmIO::E_MODE_ONLINE_RACE:
                case GsmIO::E_MODE_ONLINE_ROAD_RAGE:
                {
                    GuiTracker* lpTracker = mpGuiCache->GetGuiTracker();
                    CGS_ASSERT(lpTracker != NULL, "lpTracker");

                    const BrnGameState::LandmarkIndex* lpLandmarks =
                        lpTracker->GetActivelyTrackedLandmarks();
                    CGS_ASSERT(lpLandmarks != NULL, "lpLandmarks");

                    ShowLandmarkOnCompass(lpLandmarks[0], lfBearing);

                    if (lpTracker->GetNumActivelyTrackedLandmarks() == 1)
                        mDestMarkerMovie.GotoAndPlayLabel("finish");
                    else
                        mDestMarkerMovie.GotoAndPlayLabel("checkpoint");
                    return;
                }

                case GsmIO::E_MODE_ONLINE_FREE_BURN_LOBBY:
                {
                    if (ShowChallengeOnCompass(lfBearing))
                        return;
                    break;
                }

                default:
                    break;
            }
        }

        SetMarkerPos(0.0f, false);
        mDestMarkerMovie.GotoAndPlayLabel("invisible");
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
                   "leLanguage > CgsLanguage::E_LANGUAGE_INVALID");
        CGS_ASSERT(leLanguage < CgsLanguage::E_LANGUAGE_TOTAL,
                   "leLanguage < CgsLanguage::E_LANGUAGE_TOTAL");

        FormatDirectionLetters(leLanguage, &lCompassViewFrontLeft);
        FormatDirectionLetters(leLanguage, &lCompassViewFrontCentre);
        FormatDirectionLetters(leLanguage, &lCompassViewFrontRight);
        FormatDirectionLetters(leLanguage, &lCompassViewBackLeft);
        FormatDirectionLetters(leLanguage, &lCompassViewBackCentre);
        FormatDirectionLetters(leLanguage, &lCompassViewBackRight);

        {
            static bool s_bLoggedPrepare = false;
            if (!s_bLoggedPrepare)
            {
                s_bLoggedPrepare = true;
                *CgsDev::Log::gpDebugPrint << "[p0-compass] CompassComponent::Prepare done language="
                                           << static_cast<s32>(leLanguage) << " viewLen="
                                           << mfSingleViewLength << "\n";
            }
        }
    }
}
