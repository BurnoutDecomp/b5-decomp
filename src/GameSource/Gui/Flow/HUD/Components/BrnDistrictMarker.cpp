// ===================================================================================
// BrnGui::DistrictMarkerComponent -- out-of-line bodies.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x82412550, Prepare @0x82420FB8,
//   ProcessCountyTransitionComplete @0x82412A20, ProcessDistrictTransitionComplete @0x82412AB0,
//   SetCounty @0x82412658, SetDistrict @0x82412858.
// (The two static Transition*CompleteCallback thunks @0x82412B40/@0x82412BA0 are SKIPPED --
//  declared-only; Prepare takes their address to install them.)
// ===================================================================================

#include "GameSource/Gui/Flow/hud/Components/BrnDistrictMarker.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"  // MovieClipInstance::ResetTimeline
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGui
{
    // County / district icon-label tables (X360 off_82F2C90C / off_820A7600). Values not
    // recovered in this slice -> empty-string placeholders sized to the region-id counts.
    const char* const KAPC_COUNTY_ICON_NAMES[BrnWorld::E_COUNTY_COUNT]     = {};
    const char* const KAPC_DISTRICT_ICON_NAMES[BrnWorld::E_DISTRICT_COUNT] = {};

    // Temp-string capacity for the composite icon-clip names (X360 hard-NULs [63]).
    static const s32 KI_TEMP_STRING_LENGTH = 64;

    // @ 0x82412550 -- adopt the state channel (inlined BrnFlaptComponent::Construct),
    // construct the four embedded icon container/icon movies, and seed the county/district
    // state machine to its "loaded, nothing selected yet" defaults.
    void DistrictMarkerComponent::Construct(const char* /*lacName*/,
                                            CgsGui::StateInterface* lpStateInterface,
                                            const char* /*lacParentName*/)
    {
        CGS_ASSERT(lpStateInterface != 0, "lpStateInterface");

        // Inlined base BrnFlaptComponent::Construct: adopt the channel, invalidate mAptRef.
        mpStateInterface = lpStateInterface;
        mAptRef.SetInvalid();

        mCountyContainerMovie.Construct(0, lpStateInterface, 0);
        mDistrictContainerMovie.Construct(0, lpStateInterface, 0);
        mCountyIcon.Construct(0, lpStateInterface, 0);
        mDistrictIcon.Construct(0, lpStateInterface, 0);

        mbCountyChangePending   = false;
        mbDistrictChangePending = false;
        mbOnline                = false;

        meCurrentCountyState   = E_MARKERSTATE_LOADED;
        meCurrentDistrictState = E_MARKERSTATE_LOADED;

        meCurrentCounty  = BrnWorld::E_COUNTY_INVALID;
        mePendingCounty  = BrnWorld::E_COUNTY_INVALID;

        meCurrentDistrict = BrnWorld::E_DISTRICT_INVALID;
        mePendingDistrict = BrnWorld::E_DISTRICT_INVALID;
    }

    // @ 0x82420FB8 -- resolve this district marker out of the loaded Flapt file: bind the
    // top-level clip, wire the two container-movie transition callbacks, and prepare the
    // two container movies plus the two per-region icon clips.
    void DistrictMarkerComponent::Prepare(const char* lacName,
                                          const BrnFlapt::FileRef& lFile)
    {
        char lacTempString[KI_TEMP_STRING_LENGTH];
        lacTempString[KI_TEMP_STRING_LENGTH - 1] = '\0';   // X360 hard-NULs [63] first

        CGS_ASSERT(lacName != 0, "lacName != NULL");

        // Bind this component's own top-level clip into the base mAptRef (@+0x04).
        lFile.FindComponent(&mAptRef, lacName);
        CGS_ASSERT(mAptRef.mpMovieClipInst != 0, "mpMovieClipInst");
        mAptRef.mpMovieClipInst->ResetTimeline();

        // Prepare the two container movies (the marker name is their parent prefix).
        mCountyContainerMovie.Prepare("CountyMovie_mc", lFile, lacName);
        mDistrictContainerMovie.Prepare("DistrictMovie_mc", lFile, lacName);

        // Install the per-frame transition-complete callbacks on the container clips
        // (via each container's clip-ref accessor).
        mCountyContainerMovie.GetMovieClipRef().SetFrameTriggerCallback(
            reinterpret_cast<void*>(&DistrictMarkerComponent::CountyTransitionCompleteCallback),
            this);
        mDistrictContainerMovie.GetMovieClipRef().SetFrameTriggerCallback(
            reinterpret_cast<void*>(&DistrictMarkerComponent::DistrictTransitionCompleteCallback),
            this);

        // Prepare the icon clips; their names are the composite "<lacName>_<containerMovie>_<iconSet>".
        CgsCore::SnPrintf(lacTempString, KI_TEMP_STRING_LENGTH - 1, "%s_%s_%s",
                          lacName, "CountyMovie_mc", "CountyIcons_mc");
        mCountyIcon.Prepare(lacTempString, lFile, 0);

        CgsCore::SnPrintf(lacTempString, KI_TEMP_STRING_LENGTH - 1, "%s_%s_%s",
                          lacName, "DistrictMovie_mc", "DistrictIcons_mc");
        mDistrictIcon.Prepare(lacTempString, lFile, 0);
    }

    // @ 0x82412A20 -- completion handler for the county container clip's transition-complete
    // frame trigger. Only acts while the county marker is mid-transition.
    void DistrictMarkerComponent::ProcessCountyTransitionComplete(const char* /*lpcComponentName*/)
    {
        if (meCurrentCountyState != E_MARKERSTATE_TRANSITIONING)
        {
            return;
        }

        mCountyContainerMovie.SetState("transin");

        const BrnWorld::ECounty leCounty = mePendingCounty;
        mbCountyChangePending = false;
        meCurrentCounty       = leCounty;
        mCountyIcon.SetState(KAPC_COUNTY_ICON_NAMES[leCounty]);

        meCurrentCountyState = E_MARKERSTATE_SHOWING;
    }

    // @ 0x82412AB0 -- district counterpart of ProcessCountyTransitionComplete.
    void DistrictMarkerComponent::ProcessDistrictTransitionComplete(const char* /*lpcComponentName*/)
    {
        if (meCurrentDistrictState != E_MARKERSTATE_TRANSITIONING)
        {
            return;
        }

        mDistrictContainerMovie.SetState("transin");

        const BrnWorld::EDistrict leDistrict = mePendingDistrict;
        mbDistrictChangePending = false;
        meCurrentDistrict       = leDistrict;
        mDistrictIcon.SetState(KAPC_DISTRICT_ICON_NAMES[leDistrict]);

        meCurrentDistrictState = E_MARKERSTATE_SHOWING;
    }

    // @ 0x82412658 -- request the county marker show leCountyID. See header for the
    // preserved +0x65-store asm quirk.
    void DistrictMarkerComponent::SetCounty(BrnWorld::ECounty leCountyID)
    {
        CGS_ASSERT(leCountyID < BrnWorld::E_COUNTY_COUNT,
                   "Invalid district id passed to DistrictMarkerComponent::ChangeCounty");

        if ((leCountyID == mePendingCounty && mbCountyChangePending) ||
            (leCountyID == meCurrentCounty && !mbCountyChangePending))
        {
            return;
        }

        const EMarkerState leState = meCurrentCountyState;
        mePendingCounty         = leCountyID;
        mbDistrictChangePending = true;   // NOTE: writes +0x65 (asm quirk), not +0x64

        switch (leState)
        {
        case E_MARKERSTATE_LOADING:
        case E_MARKERSTATE_TRANSITIONING:
            break;

        case E_MARKERSTATE_LOADED:
        case E_MARKERSTATE_INVISIBLE:
            mCountyContainerMovie.SetState("transin");
            mbCountyChangePending = false;
            meCurrentCounty       = mePendingCounty;
            mCountyIcon.SetState(KAPC_COUNTY_ICON_NAMES[meCurrentCounty]);
            meCurrentCountyState  = E_MARKERSTATE_TRANSITIONING;
            break;

        case E_MARKERSTATE_SHOWING:
            mCountyContainerMovie.SetState("transout");
            meCurrentCountyState = E_MARKERSTATE_TRANSITIONING;
            break;

        case E_MARKERSTATE_HIDING:
            mbCountyChangePending = false;
            meCurrentCounty       = leCountyID;
            mCountyIcon.SetState(KAPC_COUNTY_ICON_NAMES[leCountyID]);
            break;

        default:
            CGS_ASSERT(false, "Invalid marker state in CountyMarkerComponent::SetCounty");
            break;
        }
    }

    // @ 0x82412858 -- district counterpart of SetCounty (no HIDING fast-path).
    void DistrictMarkerComponent::SetDistrict(BrnWorld::EDistrict leDistrictID)
    {
        CGS_ASSERT(leDistrictID < BrnWorld::E_DISTRICT_COUNT,
                   "Invalid district id passed to DistrictMarkerComponent::ChangeDistrict");

        if ((leDistrictID == mePendingDistrict && mbDistrictChangePending) ||
            (leDistrictID == meCurrentDistrict && !mbDistrictChangePending))
        {
            return;
        }

        const EMarkerState leState = meCurrentDistrictState;
        mePendingDistrict       = leDistrictID;
        mbDistrictChangePending = true;

        switch (leState)
        {
        case E_MARKERSTATE_LOADING:
        case E_MARKERSTATE_TRANSITIONING:
            break;

        case E_MARKERSTATE_LOADED:
        case E_MARKERSTATE_INVISIBLE:
            mDistrictContainerMovie.SetState("transin");
            mbDistrictChangePending = false;
            meCurrentDistrict       = mePendingDistrict;
            mDistrictIcon.SetState(KAPC_DISTRICT_ICON_NAMES[meCurrentDistrict]);
            meCurrentDistrictState  = E_MARKERSTATE_TRANSITIONING;
            break;

        case E_MARKERSTATE_SHOWING:
            mDistrictContainerMovie.SetState("transout");
            meCurrentDistrictState = E_MARKERSTATE_TRANSITIONING;
            break;

        default:
            break;
        }
    }
}
