// BrnMassive::BrnMassiveDebugComponent -- the "Massive" debug HUD (Construct, menu registration, HUD draw).

#include "GameSource/Massive/BrnMassiveDebugComponent.h"
#include "GameSource/Massive/BrnMassive.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

// The shared "draw debug text" helper (its home is CgsDebug2DImmediateRender.cpp).

namespace BrnMassive
{

// KU_COLOUR_WHITE retired (no DWARF name): its 4 use(s) spell rw::RGBA(255, 255, 255, 255), the immediate the u32 constant gave
// KU_COLOUR_TITLE retired (no DWARF name): its 1 use(s) spell rw::RGBA(255, 0, 0, 255), the immediate the u32 constant gave
// KU_COLOUR_PAUSED retired (no DWARF name): its 1 use(s) spell rw::RGBA(0, 0, 255, 255), the immediate the u32 constant gave
// KU_COLOUR_COUNT retired (no DWARF name): its 1 use(s) spell rw::RGBA(0, 255, 0, 255), the immediate the u32 constant gave
// KU_COLOUR_BAR_BACK retired (no DWARF name): its 1 use(s) spell rw::RGBA(0, 0, 255, 150), the immediate the u32 constant gave
// KU_COLOUR_BAR_FRONT retired (no DWARF name): its 1 use(s) spell rw::RGBA(0, 0, 0, 255), the immediate the u32 constant gave

// The highest debug state code (the bar's full-scale value).
static const f32 KF_MAX_STATE = 5.0f;

void BrnMassiveDebugComponent::Construct(const char* const* lpapcMassiveNames, s32 liNumMassiveNames,
                                         s32* lpaiSubscriberStates, BrnMassiveSubscriber** lpapSubscribers)
{
    mbDisplay = false;
    CGS_ASSERT(lpapcMassiveNames, "lpMassiveNames");
    CGS_ASSERT(lpaiSubscriberStates, "lpSubscriberStates");

    mapSubscribers     = lpapSubscribers;
    mpMassiveNames     = lpapcMassiveNames;
    miNumMassiveNames  = liNumMassiveNames;
    mpSubscriberStates = lpaiSubscriberStates;
    miNumImpressions   = 0;
    mfTextScale        = 18.0f;
    mpDebugSubscriber  = 0;
    mbPaused           = false;
    mfLineSpacing      = 1.5f;
    miDebugSubscriber  = -1;

}

const char* BrnMassiveDebugComponent::GetName() const
{
    return "Massive";
}

void BrnMassiveDebugComponent::OnActivate()
{
    RegisterVariable(&mbDisplay, "Display Massive Debug");
    RegisterVariable(&mbResetImpressionCount, "Reset Impression Count");
    RegisterVariable(&miDebugSubscriber, "Debug Subscriber");
    mbDisplay = true;
}

const char* BrnMassiveDebugComponent::GetState(s32 liState) const
{
    switch (liState)
    {
        case 0:  return "Not Created";
        case 1:  return "Download Skipped - Could not Find IE";
        case 2:  return "Download Blocked - Object is in View";
        case 3:  return "Download Pending";
        case 4:  return "Download Complete";
        case 5:  return "In View";
        default: return "Unknown";
    }
}

// Draw "label value" on one line at (lfX, *lpfY) and advance *lpfY by one line.
void BrnMassiveDebugComponent::DrawText(CgsDev::Debug2DImmediateRender* lpRender, const char* lpcLabel,
                                        const char* lpcValue, f32 lfX, f32* lpfY)
{
    lpRender->DrawText(lpcLabel, lfX, *lpfY, mfTextScale, rw::RGBA(255, 255, 255, 255));
    const f32 lfLabelWidth = lpRender->CalcTextWidth(lpcLabel, mfTextScale);
    lpRender->DrawText(lpcValue, lfLabelWidth + lfX, *lpfY, mfTextScale, rw::RGBA(255, 255, 255, 255));
    *lpfY = mfLineSpacing * mfTextScale + *lpfY;
}

// The selected subscriber's delivered id and current impression record.
void BrnMassiveDebugComponent::DrawSubscriber(const BrnMassiveSubscriber* lpSubscriber,
                                              CgsDev::Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY)
{
    if (lpSubscriber == 0)
    {
        return;
    }

    char lacValue[64];

    CgsCore::SPrintf(lacValue, 64, "%d", lpSubscriber->miInvElementID);
    DrawText(lpRender, "IE= ", lacValue, lfX, &lfY);

    const MassiveAdClient3::MAD_Impression& lrImpression = lpSubscriber->mImpression;
    DrawText(lpRender, "View= ", lrImpression.mbInView ? "True" : "False", lfX, &lfY);

    CgsCore::SPrintf(lacValue, 64, "%.2f", static_cast<double>(lrImpression.mfAngle));
    DrawText(lpRender, "Angle= ", lacValue, lfX, &lfY);

    CgsCore::SPrintf(lacValue, 64, "%u", lrImpression.muSize);
    DrawText(lpRender, "Size= ", lacValue, lfX, &lfY);
}

void BrnMassiveDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpRender)
{
    if (!mbDisplay)
    {
        return;
    }

    if (mbResetImpressionCount)
    {
        miNumImpressions = 0;
        mbResetImpressionCount = false;
    }

    DrawSubscriber(mpDebugSubscriber, lpRender, 550.0f, 300.0f);

    lpRender->DrawText("Massive Debug", 100.0f, 100.0f, 32.0f, rw::RGBA(255, 0, 0, 255));

    CGS_ASSERT(mpMassiveNames, "mpMassiveNames");
    CGS_ASSERT(mpSubscriberStates, "mpSubscriberStates");

    if (mbPaused)
    {
        lpRender->DrawText("PAUSED", 300.0f, 100.0f, 32.0f, rw::RGBA(0, 0, 255, 255));
    }

    char lacText[64];
    f32 lfY = 150.0f;

    for (s32 liName = 0; liName < miNumMassiveNames; ++liName)
    {
        s32 liInvElementID = -1;
        if (mapSubscribers[liName] != 0)
        {
            liInvElementID = mapSubscribers[liName]->miInvElementID;
        }

        CgsCore::SPrintf(lacText, 64, "%s (%d)", mpMassiveNames[liName], liInvElementID);
        lpRender->DrawText(lacText, 100.0f, lfY, 12.0f, rw::RGBA(255, 255, 255, 255));

        const f32 lfNextY = lfY + 15.0f;

        const Vector2 lv2BarMin(300.0f, lfY);
        const Vector2 lv2BarMax(450.0f, lfNextY);
        lpRender->DrawHorizontalBar(lv2BarMin, lv2BarMax, static_cast<f32>(mpSubscriberStates[liName]),
                                    KF_MAX_STATE, rw::RGBA(0, 0, 255, 150), rw::RGBA(0, 0, 0, 255));

        lpRender->DrawTextInBox(GetState(mpSubscriberStates[liName]), 310.0f, lfY + 4.0f, 440.0f, lfNextY,
                                10.0f, rw::RGBA(255, 255, 255, 255), 0.0f);

        lfY = lfNextY;
    }

    CgsCore::SPrintf(lacText, 64, "Number of Impressions           : %d", miNumImpressions);
    lpRender->DrawText(lacText, 100.0f, lfY, 16.0f, rw::RGBA(0, 255, 0, 255));
}

} // namespace BrnMassive
