#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackDebugComponent.h"

#include "GameSource/BurnoutConstants.h"
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAISharedConstants.h"
#include "GameSource/World/AI/BrnHNGTest.h"
#include "GameSource/World/AI/ResetOnTrack/BrnResetOnTrackManager.h"
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/Math/BrnMathUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"

#include <math.h>

int MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCheckScreen);

namespace BrnAI
{
namespace
{
    const s32 KI_AI_CAR_COUNT = 35;
    const f32 KF_MPH_TO_METRES_PER_SECOND = 0.44704f;
    const f32 KF_DEGREES_PER_RADIAN = 57.295780181884766f; // flt_820C3B48

    // ARTIST initializer @0x82C6D0C0 computes 1.0 / flt_82F31928 (0.44704) and
    // stores flt_830180B0; RenderHUD reads it at 0x82778928 for the mph display.
    const f32 gResetSpeedDisplayScale = 1.0f / KF_MPH_TO_METRES_PER_SECOND;

    const CgsDev::RGBA K_WHITE = 0xFFFFFFFFu;
    const CgsDev::RGBA K_CYAN  = 0xFF00FFFFu;
    const CgsDev::RGBA K_GREEN = 0xFF00FF00u;
    const CgsDev::RGBA K_BLUE  = 0xFF0000FFu;

    rw::RGBA RenderColour(u32 luPacked)
    {
        rw::RGBA lColour;
        lColour.m_rgba = luPacked;
        return lColour;
    }

    const char* const K_RESET_TYPE_NAMES[E_RESET_TYPE_COUNT] =
    {
        "Invalid",
        "Standard",
        "BehindPlayer",
        "BehindPlayerRoadRage",
        "AheadPlayerOnComing",
        "FromTurningsRoadRage",
        "BehindPlayerRaceStart",
        "AwayFromPlayer",
    };

    Vector3 Add(Vector3 lA, Vector3 lB)
    {
        return Vector3{lA.x + lB.x, lA.y + lB.y, lA.z + lB.z, 0.0f};
    }

    Vector3 Scale(Vector3 lValue, f32 lfScale)
    {
        return Vector3{lValue.x * lfScale, lValue.y * lfScale, lValue.z * lfScale, 0.0f};
    }
}

// The two fixed rings and counters are initialized in this order by the inlined X360
// component constructor at 0x82791C80..0x82791CF4.
void ResetOnTrackDebugComponent::Construct(ResetOnTrackManager* lpResetOnTrackManager)
{
    mpResetOnTrackManager = lpResetOnTrackManager;
    mResetRingBuffer.Construct();
    mHngTestRingBuffer.Construct();
    miLastHngTestCount = 0;
    miFrameCount = 0;
    mbShowPlayerInfoOnly = false;
    mbShowResetInfo = false;
    mbShowCurrentInfo = false;
    mbShowHngTestInfo = false;
    mbContinousResetOnTrack = false;
    mbUseHardcodedResetPosition = false;
    mbIncrementResetSpeed = false;
    miContinuousResetFrameSkip = 60;
}

const char* ResetOnTrackDebugComponent::GetName() const
{
    return "Reset On Track";
}

const char* ResetOnTrackDebugComponent::GetPath() const
{
    return "AI";
}

// ARTIST 0x82790E00. Registration order and labels are the direct call sequence in the
// X360 assembly; no range or callback setup follows the nine entries.
void ResetOnTrackDebugComponent::OnActivate()
{
    RegisterFunction(&ResetOnTrackDebugComponent::ResetPlayerCarOnTrackCallback,
                     this,
                     "Reset player car on track");
    RegisterVariable(&mbShowResetInfo, "Show reset info");
    RegisterVariable(&mbShowCurrentInfo, "Show current info");
    RegisterVariable(&mbShowPlayerInfoOnly, "Show player info only");
    RegisterVariable(&mbShowHngTestInfo, "Show line test info");
    RegisterVariable(&mbContinousResetOnTrack, "Continuous reset on track");
    RegisterVariable(&miContinuousResetFrameSkip, "Continuous reset frame skip");
    RegisterVariable(&mbUseHardcodedResetPosition, "Use hardcoded reset position");
    RegisterVariable(&mbIncrementResetSpeed, "Increment reset speed");
}

// ARTIST 0x827867E8. The modulo tripwire is the console's division-by-zero fault for a
// zero divisor (and separately signed overflow). Negative nonzero skips remain valid.
void ResetOnTrackDebugComponent::Update()
{
    if (mbContinousResetOnTrack)
    {
        if ((miFrameCount % miContinuousResetFrameSkip) == 0)
        {
            AIModuleIO::ResetOnTrackRequest lRequest;
            lRequest.Construct(mpResetOnTrackManager->mePlayerGlobalRaceCarIndex,
                               100.0f * KF_MPH_TO_METRES_PER_SECOND,
                               0.0f,
                               E_RESET_TYPE_STANDARD);
            mpResetOnTrackManager->PushResetOnTrackRequest(&lRequest);
        }
    }

    miFrameCount = static_cast<s32>(static_cast<u32>(miFrameCount) + 1u);
}

// ARTIST 0x82786890. The registered callback's first argument is the component user data;
// the other incoming registers do not participate in the request.
void ResetOnTrackDebugComponent::ResetPlayerCarOnTrackCallback(void* lpUserData)
{
    ResetOnTrackDebugComponent* lpThis =
        static_cast<ResetOnTrackDebugComponent*>(lpUserData);
    AIModuleIO::ResetOnTrackRequest lRequest;
    lRequest.Construct(lpThis->mpResetOnTrackManager->mePlayerGlobalRaceCarIndex,
                       60.0f * KF_MPH_TO_METRES_PER_SECOND,
                       0.0f,
                       E_RESET_TYPE_STANDARD);
    lpThis->mpResetOnTrackManager->PushResetOnTrackRequest(&lRequest);
}

// ARTIST's PushResetInfo body begins at 0x82778B30 (the export omits this body). The
// request filter, angle/distance calculations, snapshot fields, and counter reset are
// recovered from its raw assembly. The angle is reported in degrees.
void ResetOnTrackDebugComponent::PushResetInfo(
    const AIModuleIO::ResetOnTrackRequest* lpRequest,
    const AIModuleIO::ResetOnTrackResult* lpResult)
{
    if (lpRequest->GetGlobalRaceCarIndex() !=
            mpResetOnTrackManager->mePlayerGlobalRaceCarIndex &&
        mbShowPlayerInfoOnly)
    {
        miLastHngTestCount = 0;
        return;
    }

    const AICar* lpRequestCar =
        mpResetOnTrackManager->GetAICar(lpRequest->GetGlobalRaceCarIndex());
    const AICar* lpPlayerCar =
        mpResetOnTrackManager->GetAICar(mpResetOnTrackManager->mePlayerGlobalRaceCarIndex);

    const Vector3 lCarDirection = lpRequestCar->GetDirection();
    const Vector3 lResetDirection = lpResult->GetResetDirection();
    const f32 lfCarDirectionLength = sqrtf(
        lCarDirection.x * lCarDirection.x +
        lCarDirection.y * lCarDirection.y +
        lCarDirection.z * lCarDirection.z);
    const f32 lfResetDirectionLength = sqrtf(
        lResetDirection.x * lResetDirection.x +
        lResetDirection.y * lResetDirection.y +
        lResetDirection.z * lResetDirection.z);
    const f32 lfDirectionDot =
        (lCarDirection.x * lResetDirection.x +
         lCarDirection.y * lResetDirection.y +
         lCarDirection.z * lResetDirection.z) /
        (lfCarDirectionLength * lfResetDirectionLength);

    f32 lfClampedDot = lfDirectionDot;
    if (lfClampedDot < -1.0f)
    {
        lfClampedDot = -1.0f;
    }
    else if (lfClampedDot > 1.0f)
    {
        lfClampedDot = 1.0f;
    }

    const Vector3 lCarPosition = lpResult->GetResetPosition();
    const Vector3 lPlayerPosition = lpPlayerCar->GetPosition();
    const f32 lfDX = lCarPosition.x - lPlayerPosition.x;
    const f32 lfDY = lCarPosition.y - lPlayerPosition.y;
    const f32 lfDZ = lCarPosition.z - lPlayerPosition.z;

    ResetEntry lEntry;
    lEntry.mRequest = *lpRequest;
    lEntry.mResult = *lpResult;
    lEntry.mfAngle = acosf(lfClampedDot) * KF_DEGREES_PER_RADIAN;
    lEntry.mfDistanceToPlayer = sqrtf(lfDX * lfDX + lfDY * lfDY + lfDZ * lfDZ);
    lEntry.miHngTestCount = miLastHngTestCount;
    lEntry.mbHasDriver = lpRequestCar->meCarState == E_AI_CAR_STATE_IN_RANGE;
    mResetRingBuffer.Push(&lEntry);
    miLastHngTestCount = 0;
}

// ARTIST 0x82778D68. The function receives XZ-plane Vector2 endpoints, uses car zero's
// current height for both stored Vector3s, pushes the HNG history entry, then increments
// the count attached to the next accepted reset.
void ResetOnTrackDebugComponent::PushHngTestInfo(
    Vector2 lStartPosition, Vector2 lEndPosition, bool lbHit)
{
    const Vector3 lReferencePosition =
        mpResetOnTrackManager->GetAICar(E_GLOBAL_RACE_CAR_INDEX_0)->GetPosition();

    HngTestEntry lEntry;
    lEntry.mStartPosition =
        Vector3{lStartPosition.x, lReferencePosition.y, lStartPosition.y, 0.0f};
    lEntry.mEndPosition =
        Vector3{lEndPosition.x, lReferencePosition.y, lEndPosition.y, 0.0f};
    lEntry.mbHit = lbHit;
    mHngTestRingBuffer.Push(&lEntry);
    ++miLastHngTestCount;
}

void ResetOnTrackDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (mbShowCurrentInfo)
    {
        const AICar* lpPlayerCar =
            mpResetOnTrackManager->GetAICar(mpResetOnTrackManager->mePlayerGlobalRaceCarIndex);

        CgsDev::SimpleStrStream lStream;
        lStream << "Reset on track section: "
                << static_cast<s32>(lpPlayerCar->muResetOnTrackSectionIndex)
                << ", Recent resets: " << mpResetOnTrackManager->mRecentResets.GetLength();
        if (!lpPlayerCar->IsDrivenByPlayer())
        {
            lStream << ", AI Driven";
        }
        MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f, 50.0f, 12.0f, K_WHITE, false);
    }

    if (mbShowResetInfo)
    {
        s32 liDisplayedRow = 0;
        const s32 liEntryCount = mResetRingBuffer.GetLength();
        for (s32 liIndex = 0; liIndex < liEntryCount; ++liIndex)
        {
            const ResetEntry& lrEntry =
                mResetRingBuffer[static_cast<u32>(liIndex)];
            const bool lbIsPlayerRequest =
                lrEntry.mRequest.GetGlobalRaceCarIndex() ==
                mpResetOnTrackManager->mePlayerGlobalRaceCarIndex;
            if (!lbIsPlayerRequest && mbShowPlayerInfoOnly)
            {
                continue;
            }

            const s32 liResetType =
                static_cast<s32>(lrEntry.mRequest.GetResetType());
            const char* lpcResetType = K_RESET_TYPE_NAMES[liResetType];
            if (lpcResetType == 0)
            {
                lpcResetType = "<NULLSTRING>";
            }

            CgsDev::SimpleStrStream lStream;
            lStream << "Car "
                    << static_cast<s32>(lrEntry.mRequest.GetGlobalRaceCarIndex())
                    << ", " << lpcResetType << ", "
                    << static_cast<s32>(lrEntry.mRequest.GetResetDistance()) << "m, "
                    << static_cast<s32>(lrEntry.mResult.GetResetSpeed() * gResetSpeedDisplayScale)
                    << " mph";
            if (lrEntry.miHngTestCount > 1)
            {
                lStream << ", " << lrEntry.miHngTestCount << " HNG tests";
            }
            if (!lrEntry.mbHasDriver)
            {
                lStream << ", No driver";
            }
            lStream << ", " << static_cast<s32>(lrEntry.mfDistanceToPlayer) << "m from player";
            lStream << (lrEntry.mResult.GetState() ==
                            AIModuleIO::ResetOnTrackResult::E_STATE_SUCCESS
                            ? ", Success"
                            : ", Failure");

            MaybeDrawText(lpDisplay, lStream.GetBuffer(), 50.0f,
                          100.0f + static_cast<f32>(liDisplayedRow) * 14.0f,
                          12.0f, lbIsPlayerRequest ? K_WHITE : K_CYAN, false);
            ++liDisplayedRow;
        }
    }
}

void ResetOnTrackDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    if (mbShowHngTestInfo)
    {
        const f32 lfLineLength =
            KF_MPH_TO_METRES_PER_SECOND * 60.0f + 6.0f;

        for (s32 liCarIndex = 0; liCarIndex < KI_AI_CAR_COUNT; ++liCarIndex)
        {
            const EGlobalRaceCarIndex leCarIndex =
                static_cast<EGlobalRaceCarIndex>(liCarIndex);
            const AICar* lpCar = mpResetOnTrackManager->GetAICar(leCarIndex);
            const bool lbIsPlayer = lpCar->IsPlayerCar();
            if (!lbIsPlayer && mbShowPlayerInfoOnly)
            {
                continue;
            }
            if (lpCar->meCarState != E_AI_CAR_STATE_IN_RANGE)
            {
                continue;
            }

            const Vector3 lStartPosition = lpCar->GetPosition();
            const Vector2 lStart2D = BrnMath::Flatten(lStartPosition);

            // ARTIST keeps this projection call for its validity assertion even though it
            // discards the projected value before reading the full direction for the endpoint.
            (void)BrnMath::Flatten(lpCar->GetDirection());

            const Vector3 lDirection = lpCar->GetDirection();
            const Vector3 lPosition = lpCar->GetPosition();
            const Vector3 lEndPosition = Add(lPosition, Scale(lDirection, lfLineLength));
            const Vector2 lEnd2D = BrnMath::Flatten(lEndPosition);
            const AISection* lpSection =
                mpResetOnTrackManager->mpAISectionData->GetAISection(
                    lpCar->muResetOnTrackSectionIndex);

            CgsDev::RGBA luColour = K_GREEN;
            if (LineTestSectionHNG(lpSection, lStart2D, lEnd2D))
            {
                luColour = K_BLUE;
            }
            else if (lpCar->GetDriver() != 0 &&
                     LineTestTrafficHNG(lpCar->GetDriver()->GetNearbyVehicles(),
                                        lStart2D, lEnd2D))
            {
                luColour = K_CYAN;
            }

            const Vector3 lDrawStartPosition = lpCar->GetPosition();
            lpDisplay->DrawLine(lDrawStartPosition, lEndPosition, RenderColour(luColour));
        }

        const s32 liHngEntryCount = mHngTestRingBuffer.GetLength();
        for (s32 liIndex = 0; liIndex < liHngEntryCount; ++liIndex)
        {
            const HngTestEntry& lrEntry =
                mHngTestRingBuffer[static_cast<u32>(liIndex)];
            lpDisplay->DrawLine(lrEntry.mStartPosition, lrEntry.mEndPosition,
                                RenderColour(lrEntry.mbHit ? K_BLUE : K_GREEN));
        }
    }

    if (mbShowCurrentInfo)
    {
        DrawResetOnTrackAISection(lpDisplay);
    }
}

void ResetOnTrackDebugComponent::DrawResetOnTrackAISection(
    CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 liCarIndex = 0; liCarIndex < KI_AI_CAR_COUNT; ++liCarIndex)
    {
        const EGlobalRaceCarIndex leCarIndex =
            static_cast<EGlobalRaceCarIndex>(liCarIndex);
        const bool lbIsPlayer =
            leCarIndex == mpResetOnTrackManager->mePlayerGlobalRaceCarIndex;
        if (!lbIsPlayer && mbShowPlayerInfoOnly)
        {
            continue;
        }

        const AICar* lpCar = mpResetOnTrackManager->GetAICar(leCarIndex);
        if (lpCar->muResetOnTrackSectionIndex == AICar::KI_INVALID_SECTION_INDEX)
        {
            continue;
        }

        const AISection* lpSection =
            mpResetOnTrackManager->mpAISectionData->GetAISection(
                lpCar->muResetOnTrackSectionIndex);
        const Vector3 lStart =
            lpSection->GetPortal(lpCar->muResetOnTrackStartPortal)->GetPosition();
        const Vector3 lEnd =
            lpSection->GetPortal(lpCar->muResetOnTrackEndPortal)->GetPosition();

        lpDisplay->DrawLine(lStart, lEnd, RenderColour(lbIsPlayer ? K_GREEN : K_CYAN));

        const Vector3 lStartExtent = {3.0f, 3.0f, 3.0f, 0.0f};
        const Vector3 lEndExtent = {0.25f, 0.25f, 0.25f, 0.0f};
        lpDisplay->DrawSolidBox(Vector3{lStart.x - lStartExtent.x,
                                     lStart.y - lStartExtent.y,
                                     lStart.z - lStartExtent.z, 0.0f},
                             Add(lStart, lStartExtent),
                             RenderColour(0x800000FFu));
        lpDisplay->DrawSolidBox(Vector3{lEnd.x - lEndExtent.x,
                                     lEnd.y - lEndExtent.y,
                                     lEnd.z - lEndExtent.z, 0.0f},
                             Add(lEnd, lEndExtent),
                             RenderColour(0x80FF0000u));
    }
}

}
