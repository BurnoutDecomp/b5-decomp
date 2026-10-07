// BrnMassive::BrnMassiveDebugComponent -- the "Massive" debug HUD (Construct, menu registration, HUD draw).

#include "GameSource/Massive/BrnMassiveDebugComponent.h"
#include "GameSource/Massive/BrnMassive.h"

#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"                                   // CgsCore::SPrintf
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"

// The shared "draw debug text" helper (its home is CgsDebug2DImmediateRender.cpp).
int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

namespace BrnMassive
{

static const CgsDev::RGBA KU_COLOUR_WHITE     = 0xFFFFFFFFu;
static const CgsDev::RGBA KU_COLOUR_TITLE     = 0xFF0000FFu;
static const CgsDev::RGBA KU_COLOUR_PAUSED    = 0xFFFF0000u;
static const CgsDev::RGBA KU_COLOUR_COUNT     = 0xFF00FF00u;
static const CgsDev::RGBA KU_COLOUR_BAR_BACK  = 0x96FF0000u;
static const CgsDev::RGBA KU_COLOUR_BAR_FRONT = 0xFF000000u;

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
    mpDebugSubscriber  = nullptr;
    mbPaused           = false;
    mfLineSpacing      = 1.5f;
    miDebugSubscriber  = -1;

    CgsDev::DebugComponent::Construct();
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
    MaybeDrawText(lpRender, lpcLabel, lfX, *lpfY, mfTextScale, KU_COLOUR_WHITE, false);
    const f32 lfLabelWidth = lpRender->CalcTextWidth(lpcLabel, mfTextScale);
    MaybeDrawText(lpRender, lpcValue, lfLabelWidth + lfX, *lpfY, mfTextScale, KU_COLOUR_WHITE, false);
    *lpfY = mfLineSpacing * mfTextScale + *lpfY;
}

// The selected subscriber's delivered id and current impression record.
void BrnMassiveDebugComponent::DrawSubscriber(const BrnMassiveSubscriber* lpSubscriber,
                                              CgsDev::Debug2DImmediateRender* lpRender, f32 lfX, f32 lfY)
{
    if (lpSubscriber == nullptr)
    {
        return;
    }

    char lacValue[64];

    CgsCore::SPrintf(lacValue, 64, "%d", lpSubscriber->miInvElementID);
    DrawText(lpRender, "IE= ", lacValue, lfX, &lfY);

    const BrnMassiveSubscriber::ImpressionData& lrImpression = lpSubscriber->mImpression;
    DrawText(lpRender, "View= ", lrImpression.mbInView ? "True" : "False", lfX, &lfY);

    CgsCore::SPrintf(lacValue, 64, "%.2f", static_cast<double>(lrImpression.mfAngle));
    DrawText(lpRender, "Angle= ", lacValue, lfX, &lfY);

    CgsCore::SPrintf(lacValue, 64, "%u", lrImpression.muScreenSize);
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

    MaybeDrawText(lpRender, "Massive Debug", 100.0f, 100.0f, 32.0f, KU_COLOUR_TITLE, false);

    CGS_ASSERT(mpMassiveNames, "mpMassiveNames");
    CGS_ASSERT(mpSubscriberStates, "mpSubscriberStates");

    if (mbPaused)
    {
        MaybeDrawText(lpRender, "PAUSED", 300.0f, 100.0f, 32.0f, KU_COLOUR_PAUSED, false);
    }

    char lacText[64];
    f32 lfY = 150.0f;

    for (s32 liName = 0; liName < miNumMassiveNames; ++liName)
    {
        s32 liInvElementID = -1;
        if (mapSubscribers[liName] != nullptr)
        {
            liInvElementID = mapSubscribers[liName]->miInvElementID;
        }

        CgsCore::SPrintf(lacText, 64, "%s (%d)", mpMassiveNames[liName], liInvElementID);
        MaybeDrawText(lpRender, lacText, 100.0f, lfY, 12.0f, KU_COLOUR_WHITE, false);

        const f32 lfNextY = lfY + 15.0f;

        Vector2 lv2BarMin;
        lv2BarMin.x = 300.0f;
        lv2BarMin.y = lfY;
        lv2BarMin.z = 0.0f;
        lv2BarMin.w = 0.0f;
        Vector2 lv2BarMax;
        lv2BarMax.x = 450.0f;
        lv2BarMax.y = lfNextY;
        lv2BarMax.z = 0.0f;
        lv2BarMax.w = 0.0f;
        lpRender->DrawHorizontalBar(lv2BarMin, lv2BarMax, static_cast<f32>(mpSubscriberStates[liName]),
                                    KF_MAX_STATE, KU_COLOUR_BAR_BACK, KU_COLOUR_BAR_FRONT);

        lpRender->DrawTextInBox(GetState(mpSubscriberStates[liName]), 310.0f, lfY + 4.0f, 440.0f, lfNextY,
                                10.0f, KU_COLOUR_WHITE, 0.0f);

        lfY = lfNextY;
    }

    CgsCore::SPrintf(lacText, 64, "Number of Impressions           : %d", miNumImpressions);
    MaybeDrawText(lpRender, lacText, 100.0f, lfY, 16.0f, KU_COLOUR_COUNT, false);
}

} // namespace BrnMassive
