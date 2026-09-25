#include "GameSource/Gui/Flow/Screen/Components/BrnCrashNavBorough.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"        // CGS_ASSERT
#include "GameShared/GameClasses/Core/CgsStringUtils.h"   // CgsCore::SPrintf
#include "SharedClasses/Gui/SatNav/BrnMapUtils.h"         // MapTransform::WorldToDevice / Unflatten

// ===================================================================================
// BrnGui::CrashNavBorough
//
// The county rectangles below are written at start-up by the table's dynamic
// initialiser (corner + extent per county, read from the image); the zoom factors are
// the table's static data, and the centre points are left to Construct.
// ===================================================================================

namespace BrnGui
{
    namespace
    {
        // The label buffer the four position/size views are printed into.
        const u32 KU_VIEW_STATE_BUFFER_LENGTH = 32;
    }

    CrashNavBorough::BoroughPositions CrashNavBorough::maBoroughPositions[KI_NUM_BOROUGHS] =
    {
        { { -0.0f, -3400.0f, 0.0f, 0.0f }, { 4350.0f, 4400.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_MEDIUM },
        { { -3500.0f, -4000.0f, 0.0f, 0.0f }, { 5200.0f, 5100.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_LOW    },
        { { -2000.0f, -1150.0f, 0.0f, 0.0f }, { 4300.0f, 4700.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_MEDIUM },
        { { -4400.0f, -4000.0f, 0.0f, 0.0f }, { 8300.0f, 8300.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_LOW    },
        { { 1450.0f, -2800.0f, 0.0f, 0.0f }, { 4000.0f, 3900.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_MEDIUM },
        { { 0.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, MainMapComponent::E_ZOOMFACTOR_LOW    },
    };

    const char CrashNavBorough::macIconState[10]    = "apt_label";
    const char CrashNavBorough::macXposVar[3]       = "_x";
    const char CrashNavBorough::macYposVar[3]       = "_y";
    const char CrashNavBorough::macWidthposVar[7]   = "_width";
    const char CrashNavBorough::macHeightposVar[8]  = "_height";

    // ------------------------------------------------------------ Construct
    void CrashNavBorough::Construct(const char* lpacName, CgsGui::StateInterface* lpStateInterface,
                                    const char* lpacParentName)
    {
        CGS_ASSERT(lpacName != 0, "Invalid name sent to CrashNavBorough::Construct");
        CGS_ASSERT(lpStateInterface != 0, "Invalid state interface sent to CrashNavBorough::Construct");

        CgsGui::GuiComponent::Construct(lpacName, lpStateInterface, lpacParentName);

        meCurrentlySelectedBorough = BrnWorld::E_COUNTY_INVALID;

        for (s32 liBorough = 0; liBorough < KI_NUM_BOROUGHS; ++liBorough)
        {
            BoroughPositions& lrBorough = maBoroughPositions[liBorough];
            lrBorough.mv2CenterPosition.x = 0.5f * lrBorough.mv2WidthHeight.x + lrBorough.mv2Position.x;
            lrBorough.mv2CenterPosition.y = 0.5f * lrBorough.mv2WidthHeight.y + lrBorough.mv2Position.y;
        }
    }

    // ------------------------------------------------------------ SetCurrentBorough
    void CrashNavBorough::SetCurrentBorough(BrnWorld::ECounty leBorough)
    {
        meCurrentlySelectedBorough = leBorough;

        AddOutputAptViewState(macIconState, BrnWorld::WorldRegion::CountyToString(leBorough), false);

        const BoroughPositions& lrBorough = maBoroughPositions[meCurrentlySelectedBorough];

        char lacValue[KU_VIEW_STATE_BUFFER_LENGTH];

        const Vector2 lv2TopLeft =
            MapTransform::WorldToDevice(MapTransform::Unflatten(lrBorough.mv2Position), false);

        CgsCore::SPrintf(lacValue, KU_VIEW_STATE_BUFFER_LENGTH - 1, "%f", lv2TopLeft.x);
        lacValue[KU_VIEW_STATE_BUFFER_LENGTH - 1] = '\0';
        AddOutputAptViewState(macXposVar, lacValue, true);

        CgsCore::SPrintf(lacValue, KU_VIEW_STATE_BUFFER_LENGTH - 1, "%f", lv2TopLeft.y);
        lacValue[KU_VIEW_STATE_BUFFER_LENGTH - 1] = '\0';
        AddOutputAptViewState(macYposVar, lacValue, true);

        const Vector2 lv2BottomRightWorld = { lrBorough.mv2Position.x + lrBorough.mv2WidthHeight.x,
                                              lrBorough.mv2Position.y + lrBorough.mv2WidthHeight.y,
                                              lrBorough.mv2Position.z + lrBorough.mv2WidthHeight.z,
                                              lrBorough.mv2Position.w + lrBorough.mv2WidthHeight.w };
        const Vector2 lv2BottomRight =
            MapTransform::WorldToDevice(MapTransform::Unflatten(lv2BottomRightWorld), false);

        CgsCore::SPrintf(lacValue, KU_VIEW_STATE_BUFFER_LENGTH - 1, "%f", lv2BottomRight.x - lv2TopLeft.x);
        lacValue[KU_VIEW_STATE_BUFFER_LENGTH - 1] = '\0';
        AddOutputAptViewState(macWidthposVar, lacValue, true);

        CgsCore::SPrintf(lacValue, KU_VIEW_STATE_BUFFER_LENGTH - 1, "%f", lv2BottomRight.y - lv2TopLeft.y);
        lacValue[KU_VIEW_STATE_BUFFER_LENGTH - 1] = '\0';
        AddOutputAptViewState(macHeightposVar, lacValue, true);
    }
}
