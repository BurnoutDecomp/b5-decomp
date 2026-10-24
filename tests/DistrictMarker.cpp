#include "GameSource/Gui/Flow/HUD/Components/BrnDistrictMarker.h"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <stdexcept>
#include <string>

namespace
{
struct Clip
{
    std::string mLabel;
    unsigned muRequests = 0;
};
std::map<const BrnGui::FlaptIconComponent*, Clip> gClips;
unsigned guChecks = 0, guFailures = 0, guAsserts = 0;

void Check(bool lbCondition, const char* lpcName)
{
    ++guChecks;
    if (!lbCondition)
    {
        ++guFailures;
        std::printf("FAIL %s\n", lpcName);
    }
}
}

// Record the timeline commands at the icon boundary. The complete production
// marker methods, native declaration, enum values and label tables run below.
// Repeating a label is a no-op, as in FlaptIconComponent::SetState.
namespace BrnGui
{
void FlaptIconComponent::Construct(const void*, CgsGui::StateInterface* lpState, const void*)
{
    mpStateInterface = lpState;
    mAptRef.SetInvalid();
    gClips[this] = {};
}
void FlaptIconComponent::SetState(const char* lpcLabel)
{
    Clip& lrClip = gClips[this];
    if (lrClip.mLabel != lpcLabel)
    {
        lrClip.mLabel = lpcLabel;
        ++lrClip.muRequests;
    }
}
// Unused virtual slots must fail if this fixture unexpectedly enters them.
void FlaptIconComponent::Prepare(const char*, const BrnFlapt::FileRef&, const char*) { std::abort(); }
void FlaptIconComponent::Prepare(const BrnFlapt::MovieClipRef*) { std::abort(); }
void FlaptIconComponent::GotoAndStopLabel(const char*) { std::abort(); }
}
namespace CgsDev { namespace Assert {
int BeginAssert() { return 0; }
int FireAssert(const char* lpcMessage, const char*, int)
{
    ++guAsserts;
    throw std::runtime_error(lpcMessage);
}
void* EndAssert() { return nullptr; }
} }

#include "district_marker.inc"

int main()
{
    using Marker = BrnGui::DistrictMarkerComponent;
    using namespace BrnWorld;
    // Construct stores this opaque channel; the recording icon never dereferences it.
    int liChannel = 0;
    auto* lpState = reinterpret_cast<CgsGui::StateInterface*>(&liChannel);

    Marker lDistrict;
    lDistrict.Construct("marker", lpState, nullptr);
    Check(lDistrict.meCurrentDistrictState == Marker::E_MARKERSTATE_LOADED,
          "construct starts with a loaded district marker");
    lDistrict.SetDistrict(E_DISTRICT_SOUTH_BAY);
    Clip& lrDistrictContainer = gClips[&lDistrict.mDistrictContainerMovie];
    Clip& lrDistrictIcon = gClips[&lDistrict.mDistrictIcon];
    Check(lDistrict.meCurrentDistrictState == Marker::E_MARKERSTATE_SHOWING,
          "initial district is showing without awaiting a transition callback");
    Check(lrDistrictIcon.mLabel == "SouthBay", "first district selects its authored icon");
    lDistrict.SetDistrict(E_DISTRICT_MOTOR_CITY);
    Check(lrDistrictContainer.mLabel == "transout", "next district starts the outgoing animation");
    Check(lrDistrictIcon.mLabel == "SouthBay", "old district stays visible until outgoing completion");
    lDistrict.SetDistrict(E_DISTRICT_RIVER_CITY);
    Check(lDistrict.mePendingDistrict == E_DISTRICT_RIVER_CITY &&
          lrDistrictContainer.muRequests == 2, "rapid changes coalesce without restarting the outgoing animation");
    lDistrict.ProcessDistrictTransitionComplete(nullptr);
    Check(lrDistrictIcon.mLabel == "RiverCity", "completion displays the latest pending district");
    Check(lrDistrictContainer.mLabel == "transin", "completion starts the incoming animation");
    Check(lDistrict.meCurrentDistrictState == Marker::E_MARKERSTATE_SHOWING &&
          !lDistrict.mbDistrictChangePending, "completion returns to showing and clears pending state");
    const unsigned luDistrictRequests = lrDistrictContainer.muRequests + lrDistrictIcon.muRequests;
    lDistrict.ProcessDistrictTransitionComplete(nullptr);
    lDistrict.SetDistrict(E_DISTRICT_RIVER_CITY);
    Check(luDistrictRequests == lrDistrictContainer.muRequests + lrDistrictIcon.muRequests,
          "late callbacks and repeated current districts do not replay the animation");
    lDistrict.SetDistrict(E_DISTRICT_WATERFRONT);
    Check(lrDistrictContainer.mLabel == "transout", "a third change also starts a transition");
    lDistrict.ProcessDistrictTransitionComplete(nullptr);
    Check(lrDistrictIcon.mLabel == "Waterfront", "third district completes without rebuilding the HUD");

    Marker lCounty;
    lCounty.Construct("marker", lpState, nullptr);
    lCounty.SetCounty(E_COUNTY_HARBOR_TOWN);
    Clip& lrCountyContainer = gClips[&lCounty.mCountyContainerMovie];
    Clip& lrCountyIcon = gClips[&lCounty.mCountyIcon];
    Check(lCounty.meCurrentCountyState == Marker::E_MARKERSTATE_SHOWING,
          "initial county is showing without awaiting a transition callback");
    Check(lrCountyIcon.mLabel == "HarborTown", "first county selects its authored icon");
    lCounty.SetCounty(E_COUNTY_SILVER_LAKE);
    Check(lrCountyContainer.mLabel == "transout", "next county starts the outgoing animation");
    Check(lrCountyIcon.mLabel == "HarborTown", "old county stays visible until outgoing completion");
    lCounty.SetCounty(E_COUNTY_DOWNTOWN_PARADISE);
    lCounty.ProcessCountyTransitionComplete(nullptr);
    Check(lrCountyIcon.mLabel == "DowntownParadise", "county completion selects the latest request");
    Check(lCounty.meCurrentCountyState == Marker::E_MARKERSTATE_SHOWING,
          "county completion returns to showing");
    lCounty.SetHideCountyIcon(true);
    Check(lCounty.meCurrentCountyState == Marker::E_MARKERSTATE_HIDING,
          "hiding a showing county keeps its native hidden state");
    lCounty.SetCounty(E_COUNTY_WHITE_MOUNTAIN);
    Check(lrCountyIcon.mLabel == "WhiteMountain", "a hidden county updates immediately");
    lCounty.SetHideCountyIcon(false);
    Check(lCounty.meCurrentCountyState == Marker::E_MARKERSTATE_SHOWING,
          "showing a hidden county restores its native showing state");

    bool lbInvalidStateAsserted = false;
    lDistrict.meCurrentDistrictState = Marker::E_MARKERSTATE_COUNT;
    try { lDistrict.SetDistrict(E_DISTRICT_OCEAN_VIEW); }
    catch (const std::runtime_error&) { lbInvalidStateAsserted = true; }
    Check(lbInvalidStateAsserted, "an invalid district marker state retains the console assertion");
    bool lbInvalidDistrictAsserted = false;
    try { lDistrict.SetDistrict(E_DISTRICT_COUNT); }
    catch (const std::runtime_error&) { lbInvalidDistrictAsserted = true; }
    Check(lbInvalidDistrictAsserted, "an invalid district id retains the console assertion");
    Check(guAsserts == 2, "only the two intentional invalid inputs assert");

    std::printf("DistrictMarker: %u checks, %u failures\n", guChecks, guFailures);
    return guFailures != 0;
}
