// ===================================================================================
// BrnGui::DistrictMarkerComponent -- out-of-line bodies.
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX:
//   Construct @0x82412550, Prepare @0x82420FB8,
//   ProcessCountyTransitionComplete @0x82412A20, ProcessDistrictTransitionComplete @0x82412AB0,
//   SetCounty @0x82412658, SetDistrict @0x82412858,
//   CountyTransitionCompleteCallback @0x82412B40, DistrictTransitionCompleteCallback @0x82412BA0,
//   SetHideCountyIcon @0x824733B8 (re-homed from the retired Gui/View ODR-fork slice),
//   Update (ICF-folded empty; see the body's fold note).
// ===================================================================================

#include "GameSource/Gui/Flow/hud/Components/BrnDistrictMarker.h"

#include "GameSource/Gui/Flapt/BrnFlaptMovieClipInstance.h"  // MovieClipInstance::ResetTimeline
#include "GameShared/GameClasses/Core/CgsStringUtils.h"      // CgsCore::SnPrintf
#include "GameShared/GameClasses/Core/CgsAssert.h"           // CGS_ASSERT

namespace BrnGui
{
    // County / district icon-label tables -- contents RECOVERED from the image 2026-08-25
    // (headless idat read @0x82F2C90C x6 / @0x820A7600 x19; scratch h1_dump.txt). The old
    // "values not recovered -> empty placeholders" note hid a real hazard: `= {}` fills the
    // arrays with NULL, and every SetState(KAPC_...[id]) call would have handed the hasher a
    // null name. Both INVALID sentinels are real authored frames ("Anywhere" /
    // "DistrictInvalid"), which is what lets the boot-seed refresh (district 18) run the
    // marker without a special case.
    const char* const KAPC_COUNTY_ICON_NAMES[BrnWorld::E_COUNTY_COUNT] =
    {
        "PalmBayHeights",     // 0
        "SilverLake",         // 1
        "HarborTown",         // 2
        "WhiteMountain",      // 3
        "DowntownParadise",   // 4
        "Anywhere",           // 5 E_COUNTY_INVALID
    };
    const char* const KAPC_DISTRICT_ICON_NAMES[BrnWorld::E_DISTRICT_COUNT] =
    {
        "OceanView",          // 0
        "WestAcres",          // 1
        "TwinBridges",        // 2
        "BigSurfBeach",       // 3
        "EasternShore",       // 4
        "HillsidePass",       // 5
        "HeartbreakHills",    // 6
        "RockridgeCliffs",    // 7
        "SouthBay",           // 8
        "ParkVale",           // 9
        "ParadiseWharf",      // 10
        "CristalSummit",      // 11
        "LonePeaks",          // 12
        "SunsetValley",       // 13
        "Downtown",           // 14
        "RiverCity",          // 15
        "MotorCity",          // 16
        "Waterfront",         // 17
        "DistrictInvalid",    // 18 E_DISTRICT_INVALID
    };

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

    // The per-frame member handler. ICF-FOLDED EMPTY on the X360: both call sites
    // (FBurnMainHudState::ProcessAptEvents @0x82475048 type-1 and UpdateRunning
    // @0x8247B660's post-loop) show the literal folded symbol
    // `CgsSceneManager::CgsCollision::BaseCollisionGenerator::Destruct()` called with NO
    // arguments -- the linker collapsed this body onto that empty destructor because the
    // machine code is identical, i.e. the body is empty. (The
    // debug-component-and-framework-blocks fold precedent; re-verified from the image
    // 2026-08-25, scratch h1_dump.txt.) Kept as a real named method so the state's two
    // call sites read as the console's control flow.
    void DistrictMarkerComponent::Update()
    {
    }

    // @ 0x824733B8 -- drive the county icon hide/show transition. RE-HOMED 2026-08-25 from
    // Gui/View/BrnDistrictMarkerComponent.cpp (now retired): that pre-DWARF slice modelled
    // this SAME class as an opaque 0x60-byte shape whose "+0xC apt target with a no-op
    // SetViewState stub" was, in this class's real layout, mCountyContainerMovie and its
    // FlaptIconComponent::SetState vtable slot 3 -- i.e. the two files were an ODR fork and
    // the stub was a silent-drop. Its icon-state codes decode as EMarkerState:
    // "hidden" == 5 == E_MARKERSTATE_HIDING, "visible" == 2 == E_MARKERSTATE_SHOWING.
    //   hide  & not already HIDING -> county container "transout", state := HIDING
    //   show  & currently HIDING   -> county container "transin",  state := SHOWING
    void DistrictMarkerComponent::SetHideCountyIcon(bool lbHide)
    {
        if (lbHide && meCurrentCountyState != E_MARKERSTATE_HIDING)
        {
            mCountyContainerMovie.SetState("transout");
            meCurrentCountyState = E_MARKERSTATE_HIDING;
            return;
        }
        if (!lbHide && meCurrentCountyState == E_MARKERSTATE_HIDING)
        {
            mCountyContainerMovie.SetState("transin");
            meCurrentCountyState = E_MARKERSTATE_SHOWING;
        }
    }

    // @ 0x82412B40 -- static frame-trigger callback installed on the county container clip.
    // The registered user-data is the owning component; forward to the county completion
    // handler (the u16 frame arg is unused).
    void DistrictMarkerComponent::CountyTransitionCompleteCallback(void* lpUserData, u16 /*luArg*/)
    {
        CGS_ASSERT(lpUserData != 0, "lpUserData");

        DistrictMarkerComponent* const lpThis = static_cast<DistrictMarkerComponent*>(lpUserData);
        lpThis->ProcessCountyTransitionComplete(0);
    }

    // @ 0x82412BA0 -- district counterpart of CountyTransitionCompleteCallback.
    void DistrictMarkerComponent::DistrictTransitionCompleteCallback(void* lpUserData, u16 /*luArg*/)
    {
        CGS_ASSERT(lpUserData != 0, "lpUserData");

        DistrictMarkerComponent* const lpThis = static_cast<DistrictMarkerComponent*>(lpUserData);
        lpThis->ProcessDistrictTransitionComplete(0);
    }
}
