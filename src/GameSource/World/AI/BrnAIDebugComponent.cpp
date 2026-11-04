#include "GameSource/World/AI/BrnAIDebugComponent.h"

#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (member-pointer completeness)
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAICar_Constants.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnAIDebugUtils.h"
#include "SDKs/XboxMath/XMVectorACos.h"
#include <cmath>
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex operator++
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::SimpleStrStream
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameSource/World/AI/Route/BrnRoute.h"                                    // Route / RouteNode (state table)
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "rw/math/vpu/vector3_operation.h"                                         // Magnitude / operator-

void MaybeDrawText(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText,
                  f32 lfX, f32 lfY, f32 lfScale, CgsDev::RGBA lColour, bool lbCentred);

// BrnAI::AIDebugComponent -- the in-game "Main AI" debug overlay. Reconstructed from the X360
// ARTIST build (function addresses cited per method) cross-checked against the DecFIGS DWARF
// (member names / virtual shape) and the recovered base CgsDev::DebugComponent.
//
// Bodied in this TU: GetName / GetPath, the HUD path (RenderHUD with the inlined
// DrawDriftingDebug, DrawAIStatesTable and its StateTable* cursor helpers) and DrawAIStatesOnCar.
// Their member reads are on the homed AICar / AIDriver / AIAggression records, and the toggle each
// one reads is pinned by its byte offset in this object (+0x18 mbDrawCarUnderSection .. +0x39
// mbDrawDriftingDebug, +0x40/+0x44 the state-table cursor). RenderWorld and the 3D
// per-car draw helpers (DrawAICarRoute / DrawAICarSection / DrawCarControlData /
// DrawAggressiveTargetPoints / DrawAggressiveWarning / DrawCarIndices / DrawChevrons /
// DrawPortalTargets / DrawDirectDestinationVector / DrawResetOnTrackAISection / DrawAISectionEdge)
// are not bodied here.

namespace BrnAI
{

// ARTIST82773B48: active fighting drivers with an aggression target draw both primitives.
void AIDebugComponent::DrawAggressiveTargetPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        const AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mbIsActive && lrDriver.mpCar->meBehaviour == E_AI_BEHAVIOUR_FIGHTING &&
            lrDriver.mAggression.mbTargetPosValid)
        {
            const Vector3 lPosition = lrDriver.mpCar->GetPosition();
            rw::RGBA lColour; lColour.m_rgba = 0xFF0000FFu;
            lpDisplay->DrawSphere(lrDriver.mAggression.mTargetPos, 1.5f, lColour);
            lpDisplay->DrawLine(lPosition, lrDriver.mAggression.mTargetPos, lColour);
        }
    }
}

// ARTIST82773C18: warn over the targeted car while the fighting driver is in its slam state.
void AIDebugComponent::DrawAggressiveWarning(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        const AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mbIsActive && lrDriver.mpCar->meBehaviour == E_AI_BEHAVIOUR_FIGHTING &&
            lrDriver.mAggression.meAggressionState == E_AI_AGGRESSION_STATE_ATTACK_SLAM &&
            lrDriver.mAggression.mpTargetCar)
        {
            Vector3 lPosition = lrDriver.mAggression.mpTargetCar->GetPosition();
            lPosition.mV.y += 2.0f;
            rw::RGBA lColour; lColour.m_rgba = 0xFF0000FFu;
            lpDisplay->DrawText(lPosition, "DANGER", 40.0f, lColour);
        }
    }
}

// ARTIST827743A0: the active-driver ordinal is the printed index, with * for speed matching.
void AIDebugComponent::DrawCarIndices(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        const AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mbIsActive)
        {
            const Vector3 lPosition = lrDriver.mpCar->GetPosition();
            char lacText[16];
            CgsCore::SPrintf(lacText, 10, "%d", i);
            rw::RGBA lColour;
            f32 lfSize;
            if (lrDriver.mpCar->meBehaviour == E_AI_BEHAVIOUR_FIGHTING)
            {
                if (lrDriver.mAggression.meSpeedMatchType != 0)
                    CgsCore::SPrintf(lacText, 10, "%d*", i);
                lfSize = 40.0f;
                lColour.m_rgba = 0xFF0000FFu;
            }
            else
            {
                lfSize = 24.0f;
                lColour.m_rgba = 0xFF4062FFu;
            }
            lpDisplay->DrawText(lPosition, lacText, lfSize, lColour);
        }
    }
}

namespace
{
    __vector4 DebugRcpSqrt2(__vector4 lSquared)
    {
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lSquared);
        __vector4 lCorrection = __vnmsubfp(lSquared, __vmulfp(lEstimate, lEstimate), lOne);
        lEstimate = __vmaddfp(__vmulfp(lEstimate, lHalf), lCorrection, lEstimate);
        lCorrection = __vnmsubfp(lSquared, __vmulfp(lEstimate, lEstimate), lOne);
        return __vmaddfp(__vmulfp(lEstimate, lHalf), lCorrection, lEstimate);
    }
    __vector4 DebugDot2(__vector4 lLeft, __vector4 lRight)
    {
        const __vector4 lProduct = __vmulfp(lLeft, lRight);
        return __vaddfp(__vspltw(lProduct, 0), __vspltw(lProduct, 1));
    }
}

// ARTIST8277F390: find section edges at this portal, then draw the direction marker.
void AIDebugComponent::DrawAISectionEdge(CgsDev::Debug3DImmediateRender* lpDisplay,
    const AISection* lpSection, s32 liPortal, Vector3 lDirection, s32 liAlpha) const
{
    const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(liPortal));
    const Vector3 lPortalPosition = lpPortal->GetPosition();
    const __vector4 lPortal2 = {lPortalPosition.mV.x, lPortalPosition.mV.z, 0.0f, 0.0f};
    const __vector4 lDirection2 = {lDirection.mV.x, lDirection.mV.z, 0.0f, 0.0f};
    for (u32 i = 0; i < 4; ++i)
    {
        const AISection::Vector2& lrStart = lpSection->mpaCorners[i];
        const AISection::Vector2& lrEnd = lpSection->mpaCorners[(i + 1) % 4];
        const __vector4 lStart = {lrStart.x, lrStart.y, 0.0f, 0.0f};
        const __vector4 lEnd = {lrEnd.x, lrEnd.y, 0.0f, 0.0f};
        const __vector4 lEdge = __vsubfp(lStart, lEnd);
        const __vector4 lNormal = __vmulfp(lEdge, DebugRcpSqrt2(DebugDot2(lEdge, lEdge)));
        const f32 lfDirection = DebugDot2(lNormal, lDirection2).x;
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        const __vector4 lCentre = __vmulfp(__vaddfp(lStart, lEnd), lHalf);
        const __vector4 lDelta = __vsubfp(lCentre, lPortal2);
        const __vector4 lSquared = DebugDot2(lDelta, lDelta);
        f32 lfDistance = __vmulfp(lSquared, DebugRcpSqrt2(lSquared)).x;
        if (lSquared.x == 0.0f) lfDistance = 0.0f;
        if (lfDistance < 1.0f)
        {
            const f32 lfHeight = lpSection->GetPortal(static_cast<u8>(liPortal))->GetPositionY();
            const Vector3 lStartBottom(lrStart.x, lfHeight - 40.0f, lrStart.y);
            const Vector3 lStartTop(lrStart.x, lfHeight, lrStart.y);
            const Vector3 lEndTop(lrEnd.x, lfHeight, lrEnd.y);
            const Vector3 lEndBottom(lrEnd.x, lfHeight - 40.0f, lrEnd.y);
            rw::RGBA lColour;
            if (std::fabs(lfDirection) < 0.1f)
            {
                lColour.m_rgba = (static_cast<u32>(liAlpha) << 24) | 0x004040FFu;
                lpDisplay->DrawSolidQuad(lStartBottom, lStartTop, lEndTop, lEndBottom, lColour);
            }
            else
            {
                lColour.m_rgba = (static_cast<u32>(liAlpha) << 24) | 0x0000FF00u;
                lpDisplay->DrawSolidQuad(lStartBottom, lStartTop, lEndTop, lEndBottom, lColour);
                lColour.m_rgba = (static_cast<u32>(liAlpha) << 24) | 0x000000FFu;
                if (lfDirection < 0.0f)
                    lpDisplay->DrawSolidTriangle(Vector3(lrStart.x, lfHeight - 12.0f, lrStart.y),
                        Vector3(lrStart.x, lfHeight - 2.0f, lrStart.y), Vector3(lrEnd.x, lfHeight - 6.0f, lrEnd.y), lColour);
                else
                    lpDisplay->DrawSolidTriangle(Vector3(lrStart.x, lfHeight - 6.0f, lrStart.y),
                        Vector3(lrEnd.x, lfHeight - 2.0f, lrEnd.y), Vector3(lrEnd.x, lfHeight - 12.0f, lrEnd.y), lColour);
            }
        }
    }
}

namespace
{
    Vector3 NormalizeDebugVector(Vector3 lVector);
    // ARTIST8278BE6C..AC and 8278C020..3C use one rsqrt refinement.
    Vector3 NormalizeDebugVectorOne(Vector3 lVector)
    {
        const __vector4 lDot = __vmsum3fp(lVector.mV, lVector.mV);
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lDot);
        const __vector4 lCorrection = __vnmsubfp(lDot, __vmulfp(lEstimate, lEstimate), lOne);
        lEstimate = __vmaddfp(__vmulfp(lEstimate, lHalf), lCorrection, lEstimate);
        Vector3 lResult; lResult.mV = __vmulfp(lVector.mV, lEstimate);
        return lResult;
    }
    f32 DebugLength3(Vector3 lVector)
    {
        const __vector4 lSquared = __vmsum3fp(lVector.mV, lVector.mV);
        const __vector4 lZero = __vspltisw(0);
        return __vsel(__vmulfp(lSquared, DebugRcpSqrt2(lSquared)), lZero,
                      __vcmpeqfp(lZero, lSquared)).x;
    }
    f32 DebugClampedDot3(Vector3 lLeft, Vector3 lRight)
    {
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lMinusOne = __vcfsx(__vspltisw(-1), 0);
        return __vminfp(__vmaxfp(__vmsum3fp(lLeft.mV, lRight.mV), lMinusOne), lOne).x;
    }
}

// ARTIST8278BBE0: draw alternate portals at junctions ahead of car zero.
void AIDebugComponent::DrawChevrons(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar& lrCar = mpAIModule->maAICars[0];
    const Vector3 lCarPosition = lrCar.GetPosition();
    const Vector3 lCarDirection = lrCar.GetDirection();
    const Route* lpRoute = lrCar.GetRoute();
    if (!lrCar.HasValidRoute()) return;
    Vector3 lPreviousPortal(0.0f, 0.0f, 0.0f);
    for (s32 i = 0; i < lpRoute->GetNodeCount() - 1; ++i)
    {
        const RouteNode* lpNode = lpRoute->GetNode(i);
        const RouteNode* lpNext = lpRoute->GetNode(i + 1);
        const AISection* lpSection = mpAISectionsData->GetAISection(lpNode->GetSectionIndex());
        if (lpSection->mu8NumPortals > 2)
        {
            for (u32 j = 0; j < lpSection->mu8NumPortals; ++j)
            {
                const Portal* lpPortal = lpSection->GetPortal(static_cast<u8>(j));
                const Vector3 lPortalPosition = lpPortal->GetPosition();
                if (j != lpNode->GetPortalIndex() && DebugLength3(lPreviousPortal - lPortalPosition) > 10.0f &&
                    lpSection->GetPortal(static_cast<u8>(j))->mu8NumBoundaryLines)
                {
                    const Vector3 lToPortal = lPortalPosition - lCarPosition;
                    const f32 lfForwardAngle = XboxMath::XMVectorACos(DebugClampedDot3(
                        NormalizeDebugVectorOne(lToPortal), NormalizeDebugVectorOne(lCarDirection))) * 57.295780181884766f;
                    if (!(lfForwardAngle > 90.0f))
                    {
                        const BoundaryLine* lpBoundary = lpSection->GetPortal(static_cast<u8>(j))->GetBoundaryLine(0);
                        CGS_ASSERT(lpBoundary != 0, "lpBoundry != NULL");
                        const Vector3 lBoundary(lpBoundary->mfStartX - lpBoundary->mfEndX, 0.0f,
                            lpBoundary->mfStartY - lpBoundary->mfEndY);
                        const Vector3 lBoundaryDirection = NormalizeDebugVectorOne(NormalizeDebugVector(lBoundary));
                        f32 lfAngle = std::fabs(XboxMath::XMVectorACos(DebugClampedDot3(
                            lBoundaryDirection, NormalizeDebugVectorOne(lCarDirection))) * 57.295780181884766f);
                        CGS_ASSERT(lfAngle <= 180.0f, "lfAlphaProportion <= KF_STRAIGHT_ANGLE");
                        if (lfAngle > 90.0f) lfAngle = 180.0f - lfAngle;
                        __vector4 lScale; lScale.x = 128.0f; lScale.y = lScale.z = lScale.w = 128.0f;
                        __vector4 lFraction; lFraction.x = lfAngle * 0.011111111380159855f;
                        lFraction.y = lFraction.z = lFraction.w = lFraction.x;
                        const __vector4 lZero = __vspltisw(0);
                        const s32 liAlpha = static_cast<s32>(__vmaddfp(lScale, lFraction, lZero).x);
                        const Vector3 lNode(lpNode->GetPositionX(), 0.0f, lpNode->GetPositionZ());
                        const Vector3 lNext(lpNext->GetPositionX(), 0.0f, lpNext->GetPositionZ());
                        DrawAISectionEdge(lpDisplay, lpSection, static_cast<s32>(j),
                            NormalizeDebugVector(lNext - lNode), liAlpha);
                    }
                }
            }
        }
        lPreviousPortal = lpSection->GetPortal(lpNode->GetPortalIndex())->GetPosition();
    }
}

// ARTIST82773D28 (export hole): four nearby-vehicle boundary lines at car height + 0.3.
void AIDebugComponent::DrawNearbyVehicles(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mpCar)
        {
            // The original makes this initial position call before iterating the nearby list.
            lrDriver.mpCar->GetPosition();
            for (s32 j = 0; j < lrDriver.mNearbyVehicles.GetCount(); ++j)
            {
                const NearbyVehicle* lpVehicle = lrDriver.mNearbyVehicles.GetVehiclePointer(j);
                for (s32 k = 0; k < NearbyVehicle::KI_TRAFFIC_NUM_HNG_LINES; ++k)
                {
                    const BoundaryLine& lrLine = lpVehicle->maHNGLines[k];
                    const f32 lfStartHeight = lrDriver.mpCar->GetPosition().mV.y + 0.3f;
                    const Vector3 lStart(lrLine.mfStartX, lfStartHeight, lrLine.mfStartY);
                    const f32 lfEndHeight = lrDriver.mpCar->GetPosition().mV.y + 0.3f;
                    const Vector3 lEnd(lrLine.mfEndX, lfEndHeight, lrLine.mfEndY);
                    rw::RGBA lColour; lColour.m_rgba = 0xFFFFFF00u;
                    lpDisplay->DrawLine(lStart, lEnd, lColour);
                }
            }
        }
    }
}

namespace
{
    // ARTIST827738F8 uses an unguarded rsqrt estimate and two refinements.
    Vector3 NormalizeDebugVector(Vector3 lVector)
    {
        const __vector4 lDot = __vmsum3fp(lVector.mV, lVector.mV);
        const __vector4 lOne = __vcfsx(__vspltisw(1), 0);
        const __vector4 lHalf = __vcfsx(__vspltisw(1), 1);
        __vector4 lEstimate = __vrsqrtefp(lDot);
        __vector4 lCorrection = __vnmsubfp(lDot, __vmulfp(lEstimate, lEstimate), lOne);
        lEstimate = __vmaddfp(__vmulfp(lEstimate, lHalf), lCorrection, lEstimate);
        lCorrection = __vnmsubfp(lDot, __vmulfp(lEstimate, lEstimate), lOne);
        lEstimate = __vmaddfp(__vmulfp(lEstimate, lHalf), lCorrection, lEstimate);
        Vector3 lResult; lResult.mV = __vmulfp(lVector.mV, lEstimate);
        return lResult;
    }
}

// ARTIST827738F8: draw steering-target and velocity directions for ready drivers.
void AIDebugComponent::DrawCarControlData(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        const AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mpCar)
        {
            const Vector3 lPosition = lrDriver.mpCar->GetPosition();
            if (lrDriver.mbIsActive && lrDriver.mbIsRacingLineInitialised)
            {
                Vector3 lDirection(lrDriver.mSteeringTargetVector.mV.x, 0.0f,
                    lrDriver.mSteeringTargetVector.mV.y);
                lDirection = NormalizeDebugVector(NormalizeDebugVector(lDirection));
                const __vector4 lLength = __vspltw(__vcfsx(__vspltisw(15), 0), 0);
                Vector3 lEnd; lEnd.mV = __vmaddfp(lDirection.mV, lLength, lPosition.mV);
                rw::RGBA lSteeringColour; lSteeringColour.m_rgba = 0xFFFFFF00u;
                lpDisplay->DrawLine(lPosition, lEnd, lSteeringColour);
                Vector3 lVelocity = lrDriver.mpCar->GetVelocity();
                lVelocity.mV.y = 0.0f;
                lVelocity = NormalizeDebugVector(lVelocity);
                lEnd.mV = __vmaddfp(lVelocity.mV, lLength, lPosition.mV);
                rw::RGBA lVelocityColour; lVelocityColour.m_rgba = 0xFFFF00FFu;
                lpDisplay->DrawLine(lPosition, lEnd, lVelocityColour);
            }
        }
    }
}

// ARTIST82773798: the cached portal targets are plotted at car height plus eight.
void AIDebugComponent::DrawPortalTargets(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        if (lrDriver.mbIsActive && lrDriver.mbIsRacingLineInitialised)
        {
            const Vector3 lCarPosition = lrDriver.mpCar->GetPosition();
            RacingLine& lrLine = lrDriver.mRacingLine;
            for (s32 j = lrLine.mFirstSectionInCache; j < lrLine.mLastSectionInCache; ++j)
            {
                const SectionData* lpSection = lrDriver.mRacingLineGenerator.GetSectionPointer(&lrLine, j);
                const Vector3 lTarget(lpSection->mPortalEntranceAndExit.mV.z,
                    lCarPosition.mV.y + 8.0f, lpSection->mPortalEntranceAndExit.mV.w);
                rw::RGBA lColour; lColour.m_rgba = 0xFF0000F0u;
                lpDisplay->DrawSolidSphere(lTarget, 2.0f, lColour);
            }
        }
    }
}

// ARTIST827735D8: the selected car marker and successive route-node segments.
void AIDebugComponent::DrawAICarRoute(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    Vector3 lCarPosition = lrCar.GetPosition();
    lCarPosition.mV.y -= 2.0f;
    // ARTIST CRT initializer 82C69108 sets 8300D730 to {3, 200, 3, 0}.
    const Vector3 lExtent(3.0f, 200.0f, 3.0f);
    rw::RGBA lMarker; lMarker.m_rgba = 0xFFAAAAAAu;
    lpDisplay->DrawSolidBox(lCarPosition + lExtent, lCarPosition - lExtent, lMarker);
    const Route* lpRoute = lrCar.GetRoute();
    Vector3 lPrevious(0.0f, 0.0f, 0.0f);
    for (s32 i = 0; i < lpRoute->GetNodeCount(); ++i)
    {
        const RouteNode* lpNode = lpRoute->GetNode(i);
        const Vector3 lCurrent(lpNode->GetPositionX(), 2.0f, lpNode->GetPositionZ());
        if (i)
        {
            rw::RGBA lColour;
            lColour.m_rgba = i == lrCar.miNextRouteNodeIndex ? 0xFFFF0000u : 0xFF0000F0u;
            lpDisplay->DrawLine(lPrevious, lCurrent, lColour);
        }
        lPrevious = lCurrent;
    }
}

// ARTIST827671C8: draw the current car's reset section portal pair and their boxes.
void AIDebugComponent::DrawResetOnTrackAISection(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    if (lrCar.muResetOnTrackSectionIndex != 0x7FFF)
    {
        const AISection* lpSection = mpAISectionsData->GetAISection(lrCar.muResetOnTrackSectionIndex);
        const Vector3 lStart = lpSection->GetPortal(lrCar.muResetOnTrackStartPortal)->GetPosition();
        const Vector3 lEnd = lpSection->GetPortal(lrCar.muResetOnTrackEndPortal)->GetPosition();
        rw::RGBA lLine; lLine.m_rgba = 0xFF00FFFFu;
        rw::RGBA lStartColour; lStartColour.m_rgba = 0xFF0000FFu;
        rw::RGBA lEndColour; lEndColour.m_rgba = 0xFFFF0000u;
        const Vector3 lStartExtent(3.0f, 3.0f, 3.0f);
        const Vector3 lEndExtent(0.25f, 0.25f, 0.25f);
        lpDisplay->DrawLine(lStart, lEnd, lLine);
        lpDisplay->DrawSolidBox(lStart - lStartExtent, lStart + lStartExtent, lStartColour);
        lpDisplay->DrawSolidBox(lEnd - lEndExtent, lEnd + lEndExtent, lEndColour);
    }
}

// ARTIST8278BAC8: draw at most two route sections starting at the current node.
void AIDebugComponent::DrawAICarSection(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    if (lrCar.HasValidRoute())
    {
        const Route* lpRoute = lrCar.GetRoute();
        const s32 liRemaining = lpRoute->GetNodeCount() - lrCar.miNextRouteNodeIndex;
        const s32 liCount = liRemaining < 2 ? liRemaining : 2;
        const s32 liEnd = lrCar.miNextRouteNodeIndex + liCount;
        for (s32 i = lrCar.miNextRouteNodeIndex; i < liEnd; ++i)
        {
            const u32 luSection = lpRoute->GetNode(i)->GetSectionIndex();
            const bool lbBest = lrCar.GetBestSectionIndex() == luSection;
            BrnAIDebugUtils::DrawAllSectionData(lpDisplay, mpAISectionsData->GetAISection(luSection), lbBest);
        }
    }
}

// ARTIST8277F228: draw the first portal of each driver's final route section to its car.
void AIDebugComponent::DrawDirectDestinationVector(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 i = 0; i < KI_MAX_ACTIVE_RACE_CARS; ++i)
    {
        const AIDriver& lrDriver = mpAIModule->maAIDrivers[i];
        const AICar* lpCar = lrDriver.mpCar;
        if (lpCar && lpCar->HasValidRoute() && lpCar->GetRoute()->GetNodeCount() > 1)
        {
            const u16 luSection = lpCar->GetSectionIndexOfLastSectionInRoute();
            const AISectionsData* lpSections = mpAIModule->GetAISectionsData();
            const Vector3 lPortal = lpSections->GetAISection(luSection)->GetPortal(0)->GetPosition();
            const Vector3 lCar = lpCar->GetPosition();
            rw::RGBA lColour; lColour.m_rgba = 0xFFFFFF00u;
            lpDisplay->DrawLine(lPortal, lCar, lColour);
        }
    }
}

// ARTIST82796A08: preserve each original flag and per-draw call in console order.
void AIDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    CGS_ASSERT(mpAIModule != 0, "mpAIModule != NULL");
    if (mbDrawAllDriverData)
    {
        mbDrawRacingLines = true;
        mbDrawPortalTargets = true;
        mbDrawDirectVector = true;
        mbDrawControlData = true;
    }
    if (mbDrawAICarStatesOnCar) DrawAIStatesOnCar(lpDisplay);
    if (mbDrawHNGMap && mpAIModule->maAIDrivers[0].mbIsActive &&
        mpAIModule->maAIDrivers[0].mbIsRacingLineInitialised)
    {
        AIDriver& lrDriver = mpAIModule->maAIDrivers[0];
        lrDriver.mRacingLineGenerator.RenderHardNoGoMap(&lrDriver.mRacingLine);
    }
    if (mbDrawAICarRoute) DrawAICarRoute(lpDisplay);
    if (mbDrawCurrentSection) DrawAICarSection(lpDisplay);
    if (mbDrawResetOnTrackSection) DrawResetOnTrackAISection(lpDisplay);
    if (mbDrawPortalTargets) DrawPortalTargets(lpDisplay);
    if (mbDrawDestinationVector) DrawDirectDestinationVector(lpDisplay);
    if (mbDrawControlData) DrawCarControlData(lpDisplay);
    if (mbDrawAggressiveTargetPoints) DrawAggressiveTargetPoints(lpDisplay);
    if (mbDrawAggressiveWarning) DrawAggressiveWarning(lpDisplay);
    if (mbDrawAICarStatesTable) DrawCarIndices(lpDisplay);
    if (mbDrawNearbyVehicles) DrawNearbyVehicles(lpDisplay);
    if (mbDrawCarIndices && !mbDrawAICarStatesTable) DrawCarIndices(lpDisplay);
    if (mbDrawChevrons) DrawChevrons(lpDisplay);
}

void AIDebugComponent::AssertNativeLayout()
{
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawAICarStatesTable) == 0x19), "ARTIST states table flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawAICarStatesOnCar) == 0x1D), "ARTIST states on car flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawAICarRoute) == 0x29), "ARTIST route flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawAllDriverData) == 0x2C), "ARTIST all driver flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawPortalTargets) == 0x2E), "ARTIST portal flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawControlData) == 0x31), "ARTIST controls flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mbDrawHNGMap) == 0x37), "ARTIST HNG flag");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mPIDCoefficientsGraph) == 0x48), "PID graph offset");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mSteerAngleVsPIDOutputGraph) == 0x1C98), "steering graph offset");
    CGS_STATIC_ASSERT((offsetof(AIDebugComponent, mPIDDebugData) == 0x38E8), "PID debug data offset");
    CGS_STATIC_ASSERT((sizeof(AIDebugComponent) == 0x38F8), "AI debug size");
}


// ARTIST 0x82767090: the portal-target flag at +0x2e is deliberately untouched.
void AIDebugComponent::Construct(AIModule* lpAIModule)
{
    CGS_ASSERT(lpAIModule != 0, "lpAIModule != NULL");
    mpAIModule = lpAIModule;
    mpAISectionsData = 0;
    miCurrentAICar = 0;
    mbDrawCarUnderSection = false;
    mbDrawAICarStatesTable = false;
    mbDrawSpeedCalculationStateOnTable = false;
    mbDrawrPersonalityOnTable = false;
    mbDrawBuzzTime = false;
    mbDrawAICarStatesOnCar = false;
    mbDrawBehaviourOnCar = false;
    mbDrawProximityOnCar = false;
    mbDrawSpeedOnCar = false;
    mbDrawFanModeOnCar = false;
    mbDrawSpeedMatchingOnCar = false;
    mbDrawAggressionStateOnCar = false;
    mbDrawAggressionLevelOnCar = false;
    mbDrawPersonalityTypeOnCar = false;
    mbDrawScheduleOffsetOnCar = false;
    mbDrawInvulnerabilityOnCar = false;
    mbDrawPlayerSlowSpeedTimeOnCar = false;
    mbDrawAICarRoute = false;
    mbDrawCurrentSection = false;
    mbDrawResetOnTrackSection = false;
    mbDrawAllDriverData = false;
    mbDrawRacingLines = false;
    mbDrawDirectVector = false;
    mbDrawDestinationVector = false;
    mbDrawControlData = false;
    mbDrawAggressiveTargetPoints = false;
    mbDrawAggressiveWarning = false;
    mbDrawNearbyVehicles = false;
    mbDrawCarIndices = false;
    mbDrawChevrons = false;
    mbDrawHNGMap = false;
    mbDrawFinishDebugData = false;
    mbDrawDriftingDebug = false;
    mbAlwaysWantToDrift = false;
    mbShowDriftTarget = false;
    mfCruisingSpeed = 80.0f;
    mPIDDebugData.meCoefficientToBeTuned = E_COEFFICIENT_COUNT;
    mPIDDebugData.mfPCoefficient = 0.0f;
    mPIDDebugData.mfICoefficient = 0.0f;
    mPIDDebugData.mfDCoefficient = 0.0f;
}

// Inlined by AIModule::Destruct at 0x8276E3C0..0x8276E400.
void AIDebugComponent::Destruct()
{
    CGS_ASSERT(mpAIModule != 0, "mpAIModule != NULL");
    mpAIModule = 0;
}

// @0x827671B8  The component's display name in the debug menu.
const char* AIDebugComponent::GetName() const
{
    return "Main AI";
}

// @0x82767678  The component's debug-menu group path.
const char* AIDebugComponent::GetPath() const
{
    return "AI";
}

// ARTIST 0x82767428.
void AIDebugComponent::DeactiveateDriversCallback(void* lpUserData)
{
    AIModule& lrModule = *static_cast<AIModule*>(lpUserData);
    for (s32 liDriver = 0; liDriver < KI_MAX_ACTIVE_RACE_CARS; ++liDriver)
    {
        AIDriver& lrDriver = lrModule.maAIDrivers[liDriver];
        lrDriver.mbIsActive = !lrDriver.mbIsActive;
    }
}

// ARTIST 0x8277E578.
void AIDebugComponent::OnActivate()
{
    CgsResource::ResourcePtr<AISectionsData> lSections(mpAIModule->mMapDataHandle);
    mpAISectionsData = lSections.GetMemoryResource();

    const char* const lpcStatesTable = "AI Car States Table";
    RegisterVariable(&mbDrawAICarStatesTable, lpcStatesTable, "Draw AI car states table");
    RegisterVariable(&mbDrawSpeedCalculationStateOnTable, lpcStatesTable, "Draw speed calculation state");
    RegisterVariable(&mbDrawrPersonalityOnTable, lpcStatesTable, "Draw personality");
    RegisterVariable(&mbDrawBuzzTime, lpcStatesTable, "Show Buzz Timer");

    const char* const lpcStatesOnCar = "AI Car States On Car";
    RegisterVariable(&mbDrawAICarStatesOnCar, lpcStatesOnCar, "Draw AI car states on car");
    RegisterVariable(&mbDrawFanModeOnCar, lpcStatesOnCar, "Draw fan mode");
    RegisterVariable(&mbDrawBehaviourOnCar, lpcStatesOnCar, "Draw behaviour");
    RegisterVariable(&mbDrawProximityOnCar, lpcStatesOnCar, "Draw proximity");
    RegisterVariable(&mbDrawSpeedOnCar, lpcStatesOnCar, "Draw speed");
    RegisterVariable(&mbDrawAggressionStateOnCar, lpcStatesOnCar, "Draw aggression state");
    RegisterVariable(&mbDrawAggressionLevelOnCar, lpcStatesOnCar, "Draw aggression level on car");
    RegisterVariable(&mbDrawPersonalityTypeOnCar, lpcStatesOnCar, "Draw personality type on car");
    RegisterVariable(&mbDrawScheduleOffsetOnCar, lpcStatesOnCar, "Draw schedule offset on car");
    RegisterVariable(&mbDrawInvulnerabilityOnCar, lpcStatesOnCar, "Draw invulnerability on car");
    RegisterVariable(&mbDrawPlayerSlowSpeedTimeOnCar, lpcStatesOnCar, "Draw player slow speed time on car");

    const char* const lpcMarkedMan = "Marked man tweaks";
    RegisterVariable(&gfMarkedManMinOvertakeSpeed, lpcMarkedMan, "Min overtake speed");
    RegisterVariable(&gfMarkedManMaxOvertakeSpeed, lpcMarkedMan, "Max overtake speed");
    RegisterVariable(&gfMarkedManMinAggression, lpcMarkedMan, "Min aggression");
    RegisterVariable(&gfMarkedManMaxAggression, lpcMarkedMan, "Max aggression");
    SetStep(&gfMarkedManMinAggression, 0.05f);
    SetStep(&gfMarkedManMaxAggression, 0.05f);
    SetRange(&gfMarkedManMinAggression, 0.0f, 1.0f);
    SetRange(&gfMarkedManMaxAggression, 0.0f, 1.0f);

    static bool sbDriversStayStill;      // ARTIST 0x8300D604
    static bool sbDriversMaxAggression;  // ARTIST 0x8300D605
    static bool sbDrawAvoidanceDebug;    // ARTIST 0x8300D606
    RegisterVariable(&mpAIModule->mbHighTakenDownPenalty, "Easy mode");
    RegisterVariable(&mbDrawCarUnderSection, "Draw section under car");
    RegisterFunction(&DeactiveateDriversCallback, mpAIModule, "Turn Drivers On / Off");
    RegisterVariable(&sbDriversStayStill, "Drivers stay still");
    RegisterVariable(&sbDriversMaxAggression, "Drivers Max Aggression");
    RegisterVariable(&mbDrawDestinationVector, "Draw vector to destination");
    RegisterVariable(&mbDrawAggressiveTargetPoints, "Draw aggression target pos");
    RegisterVariable(&mbDrawAggressiveWarning, "Draw aggression warning");
    RegisterVariable(&mbDrawAICarRoute, "Draw current route");
    RegisterVariable(&mbDrawCurrentSection, "Draw current section");
    RegisterVariable(&mbDrawResetOnTrackSection, "Draw reset on track section");
    RegisterVariable(&miCurrentAICar, "Current AI Car");
    RegisterVariable(&mbDrawRacingLines, "Draw Racing Lines");
    RegisterVariable(&mbDrawHNGMap, "Draw Hard No Go Map");
    RegisterVariable(&mbDrawPortalTargets, "Draw Portal Targets");
    RegisterVariable(&mbDrawDirectVector, "Draw Direct Driving Vector");
    RegisterVariable(&mbDrawControlData, "Draw car control data");
    RegisterVariable(&mbDrawNearbyVehicles, "Draw nearby traffic");
    RegisterVariable(&mbDrawCarIndices, "Draw car indices");
    RegisterVariable(&mbDrawFinishDebugData, "Draw finsih debug data");
    RegisterVariable(&mbDrawDriftingDebug, "Draw drifting debug");
    RegisterVariable(&mbAlwaysWantToDrift, "Always want to drift");
    RegisterVariable(&mbShowDriftTarget, "Show driving fan");
    RegisterVariable(&mfCruisingSpeed, "Crusing Speed");

    const char* const lpcFeatures = "Feature switches";
    RegisterVariable(&mpAIModule->mbDoAggressiveDriving, lpcFeatures, "Aggressive Driving");
    RegisterVariable(&mpAIModule->mbDoInRangeCatchup, lpcFeatures, "In range catchup");
    RegisterVariable(&mpAIModule->mbDoOutOfRangeCatchup, lpcFeatures, "Out of range catchup");

    const char* const lpcPID = "PID";
    mPIDCoefficientsGraph.Prepare();
    RegisterVariable(&mPIDDebugData.mfPCoefficient, lpcPID, "Coeff P");
    RegisterVariable(&mPIDDebugData.mfICoefficient, lpcPID, "Coeff I");
    RegisterVariable(&mPIDDebugData.mfDCoefficient, lpcPID, "Coeff D");
    AddMenuItem("PID/Coefficient Graph", &mPIDCoefficientsGraph);
    mPIDCoefficientsGraph.AddFloat(&mPIDDebugData.mfPCoefficient);
    mPIDCoefficientsGraph.AddFloat(&mPIDDebugData.mfICoefficient);
    mPIDCoefficientsGraph.AddFloat(&mPIDDebugData.mfDCoefficient);

    static f32 sfGraphZero;             // ARTIST 0x8300D608
    static f32 sfGraphNegativeOne = -1.0f; // ARTIST 0x82F30180
    static f32 sfGraphPositiveOne = 1.0f;  // ARTIST 0x82F30184
    AIDriver& lrDriver = mpAIModule->maAIDrivers[0];
    mSteerAngleVsPIDOutputGraph.Prepare();
    RegisterVariable(&lrDriver.mfCalculatedSteeringAngle, lpcPID, "Error (steer)");
    RegisterVariable(&lrDriver.mfPIDOutput, lpcPID, "PID Output 1");
    AddMenuItem("PID/SteerAngle vs PID Graph", &mSteerAngleVsPIDOutputGraph);
    mSteerAngleVsPIDOutputGraph.AddFloat(&sfGraphZero);
    mSteerAngleVsPIDOutputGraph.AddFloat(&sfGraphNegativeOne);
    mSteerAngleVsPIDOutputGraph.AddFloat(&sfGraphPositiveOne);
    mSteerAngleVsPIDOutputGraph.AddFloat(&lrDriver.mfCalculatedSteeringAngle);
    mSteerAngleVsPIDOutputGraph.AddFloat(&mPIDDebugData.mfPCoefficient);

    SetRange(&miCurrentAICar, 0, KI_MAX_OUT_OF_RANGE_RACE_CARS - 1);
    RegisterVariable(&mbDrawChevrons, "Legacy", "Draw chevrons");
    RegisterVariable(&sbDrawAvoidanceDebug, "Legacy", "Draw avoidance debug");
}

namespace
{
    // The debug read-out names for the AI enums, in enumerator order.
    const char* const KAPC_AI_BEHAVIOUR_NAMES[E_AI_BEHAVIOUR_COUNT] =
    {
        "Stop", "RollingStart", "DriveThru", "Cruising", "Fighting", "QuickTurn", "SlowTurn",
        "Crashing", "Donut", "PostRaceWin", "PostRaceLose",
    };

    const char* const KAPC_BIAS_MODE_NAMES[eBiasMode_Count] =
    {
        "Race", "RaceDangerous", "Slam", "RoadRage", "CloseToPlayer", "SlamRivals", "HitOncoming",
        "SlamDangerous", "NoHNGCentering", "VeerAwayFromPlayer",
    };

    const char* const KAPC_AGGRESSION_STATE_NAMES[E_AI_AGGRESSION_STATE_COUNT] =
    {
        "OutOfRange", "OvertakeToSlam", "DropBackToSlam", "AttackSlam", "Wait", "Veer", "Passive",
        "FallPast", "BeFodder", "ClipOffBehind", "OvertakeFast", "OvertakeSlowly", "SpurtForward",
        "VeerExtreme", "HangAroundAhead",
    };

    const char* const KAPC_SPEED_MATCH_NAMES[ESpeedMatch_Count] =
    {
        "Disabled", "Enabled", "Slower", "SlowToClip", "OvertakeFast", "OvertakeSlowly",
    };

    const char* const KAPC_PERSONALITY_TYPE_NAMES[E_PERSONALITY_TYPE_COUNT] =
    {
        "Racing", "Aggression",
    };

    const char KAAC_DRIFT_STATE_NAMES[E_DRIFT_STATE_COUNT][64] =
    {
        "Normal", "Start", "Drift", "Exit",
    };

    // m/s to mph: 1 / 0.44704.
    const f32 KF_MPS_TO_MPH = 1.0f / 0.44704f;

    // The state-table speed-method and route-finding-style names, in enumerator order.
    const char KAAC_SPEED_SELECTION_METHOD_NAMES[E_AI_SPEED_SELECTION_METHOD_COUNT][64] =
    {
        "FreeRoam", "Race", "MatchPlayer", "Personality", "Top",
    };

    const char KAAC_ROUTE_FINDING_STYLE_NAMES[E_ROUTE_FINDING_MARKED_MAN + 1][64] =
    {
        "FreeRoam", "Race", "RoadRage", "Pursuit", "AvoidPlayer", "AlwaysStraight", "MarkedMan",
    };

    // State table layout: the label column is wider than the per-car columns.
    const f32 KF_STATE_TABLE_ORIGIN       = 50.0f;
    const f32 KF_STATE_TABLE_LABEL_WIDTH  = 180.0f;
    const f32 KF_STATE_TABLE_COLUMN_WIDTH = 130.0f;
    const f32 KF_STATE_TABLE_ROW_HEIGHT   = 20.0f;
    const s32 KI_STATE_TABLE_MAX_CARS     = 8;
    const u32 KU_STATE_TABLE_LABEL_COLOUR = 0xFF4062FFu;

    // Route status words of the state table.
    const char* const KPC_ROUTE_STATUS_COMPLETE = "Complete";
    const char* const KPC_ROUTE_STATUS_PARTIAL  = "Partial";
    const char* const KPC_ROUTE_STATUS_BLOCK    = "Blocked";
    const char* const KPC_ROUTE_STATUS_UNKNOWN  = "Unknown";
    const char* const KPC_ROUTE_INVALID_BLOCK   = "Blocked ";

    // A car's checkpoint distance holds this until it has a checkpoint to measure to.
    const f32 KF_NO_CHECKPOINT_DISTANCE = 3.4028234663852886e+38f;

    // The route section after the car's next route node, 0x7FFF when there is none.
    u16 GetNextNextRouteSectionIndex(const AICar* lpCar)
    {
        if (lpCar->HasValidRoute())
        {
            const s32 liNode = lpCar->GetNextRouteNodeIndex() + 1;
            if (liNode >= 0 && liNode < lpCar->GetRoute()->GetNodeCount())
            {
                return lpCar->GetRoute()->GetNode(liNode)->GetSectionIndex();
            }
        }
        return 0x7FFF;
    }

    // Where the section-under-car and drift read-outs are drawn.
    const Vector2 KV2_HUD_TEXT_POSITION(400.0f, 500.0f);

    const f32 KF_DRIFT_DEBUG_TEXT_SCALE = 20.0f;
}

// The index of the AI section under the player car (or "Invalid"), the drift state of the current
// AI car, the AI state table and the buzz-by timer.
void AIDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (mbDrawCarUnderSection)
    {
        const AICar* lpPlayerCar = mpAIModule->GetAICar(mpAIModule->mePlayerGlobalRaceCarIndex);
        if (lpPlayerCar->muBestSectionIndex == 0x7FFF)
        {
            lpDisplay->DrawText("Invalid", KV2_HUD_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
        }
        else
        {
            CgsDev::SimpleStrStream lStream;
            lStream.Reset();
            lStream << static_cast<s32>(lpPlayerCar->muBestSectionIndex);
            lpDisplay->DrawText(lStream.GetBuffer(), KV2_HUD_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
        }
    }

    if (mbDrawDriftingDebug)
    {
        DrawDriftingDebug(lpDisplay);
    }

    if (mbDrawAICarStatesTable)
    {
        DrawAIStatesTable(lpDisplay);
    }

    if (mbDrawBuzzTime)
    {
        mpAIModule->mBuzzBy.DrawBuzzTimer();
    }
}

// The drift state of the current AI car while it is in range.
void AIDebugComponent::DrawDriftingDebug(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    if (lrCar.meCarState == E_AI_CAR_STATE_IN_RANGE)
    {
        lpDisplay->DrawText(KAAC_DRIFT_STATE_NAMES[lrCar.GetDriver()->GetDriftState()], KV2_HUD_TEXT_POSITION,
                            KF_DRIFT_DEBUG_TEXT_SCALE, 0xFFFFFFFFu, false);
    }
}

// Over each active AI-driven car, one line with whichever per-car read-outs are toggled on,
// shrinking with distance from the player. A player car in the junkyard just says so.
void AIDebugComponent::DrawAIStatesOnCar(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    const rw::RGBA KCOLOUR_TEXT(255, 64, 64, 255);

    for (EActiveRaceCarIndex leActiveCar = E_ACTIVE_RACE_CAR_INDEX_0;
         leActiveCar < E_ACTIVE_RACE_CAR_INDEX_COUNT; leActiveCar++)
    {
        AIDriver* lpDriver = mpAIModule->GetAIDriver(leActiveCar);
        if (!lpDriver->IsActive())
        {
            continue;
        }

        const AICar* lpCar = lpDriver->GetCar();
        if (lpCar->mbIsPlayer)
        {
            if (lpCar->mbIsInJunkYard)
            {
                CgsDev::SimpleStrStream lStream;
                lStream << "In Junk Yard ";
                lpDisplay->DrawText(lpCar->GetPosition(), lStream.GetBuffer(), 32.0f, KCOLOUR_TEXT);
            }
            continue;
        }

        CgsDev::SimpleStrStream lStream;
        CgsDev::StrStreamBase&  lrStream     = lStream;
        const AIAggression&     lrAggression = *lpDriver->GetAggression();

        if (mbDrawProximityOnCar)
        {
            lStream << "prox " << lpCar->miProximityIndex << " ";
        }
        if (mbDrawBehaviourOnCar)
        {
            lStream << KAPC_AI_BEHAVIOUR_NAMES[lpCar->meBehaviour];
            lStream << " ";
        }
        if (mbDrawSpeedOnCar)
        {
            lStream << lpCar->GetSpeed() * KF_MPS_TO_MPH;
            lStream << "mph ";
        }
        if (mbDrawFanModeOnCar)
        {
            lStream << KAPC_BIAS_MODE_NAMES[lpDriver->mSteeringFan.meBiasMode];
            lStream << " ";
        }
        if (mbDrawAggressionStateOnCar)
        {
            lStream << KAPC_AGGRESSION_STATE_NAMES[lrAggression.GetAggressionState()];
            lStream << " ";
            if (lrAggression.GetAggressionState() == E_AI_AGGRESSION_STATE_DROP_BACK_TO_SLAM)
            {
                lStream << lrAggression.GetLeadingSeparation(lrAggression.mpCar, lrAggression.mpTargetCar);
                lStream << " ";
            }
        }
        if (mbDrawAggressionLevelOnCar)
        {
            lStream << lpCar->mAggressiveness.GetAggressionLevel();
            lStream << " ";
        }
        if (mbDrawPersonalityTypeOnCar)
        {
            lStream << KAPC_PERSONALITY_TYPE_NAMES[lpCar->mePersonalityType];
            lStream << " ";
        }
        if (mbDrawScheduleOffsetOnCar)
        {
            lStream << lpCar->mfScheduleOffset0;
            lStream << "/" << lpCar->mfScheduleOffset1;
        }
        if (mbDrawInvulnerabilityOnCar)
        {
            lrStream << lpDriver->IsInvulnerable();
        }
        if (mbDrawPlayerSlowSpeedTimeOnCar)
        {
            lStream << lpDriver->mfPlayerSlowSpeedTime;
        }
        if (mbDrawSpeedMatchingOnCar)
        {
            lStream << KAPC_SPEED_MATCH_NAMES[lrAggression.meSpeedMatchType];
            lStream << " ";
        }

        f32 lfDistanceShrink = lpCar->GetDistanceToPlayer() * 0.1f;
        lfDistanceShrink = (-lfDistanceShrink >= 0.0f) ? 0.0f : lfDistanceShrink;
        lfDistanceShrink = (10.0f - lfDistanceShrink >= 0.0f) ? lfDistanceShrink : 10.0f;

        lpDisplay->DrawText(lpCar->GetPosition(), lStream.GetBuffer(), 18.0f - lfDistanceShrink, KCOLOUR_TEXT);
    }
}

// One cell of the state table at the running cursor, every other row dimmed to two thirds (and
// made opaque), then the cursor moves down a row.
void AIDebugComponent::StateTableDrawEntry(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText, RGBA lColour)
{
    const s32 liRow = static_cast<s32>(mfStateTableY * 0.05f);
    if (liRow % 2 == 0)
    {
        const u32 luRed   = ((lColour & 0xFFu) * 2) / 3;
        const u32 luGreen = (((lColour >> 8) & 0xFFu) * 2) / 3;
        const u32 luBlue  = (((lColour >> 16) & 0xFFu) * 2) / 3;
        lColour = 0xFF000000u | ((luBlue & 0xFFu) << 16) | ((luGreen & 0xFFu) << 8) | (luRed & 0xFFu);
    }

    MaybeDrawText(lpDisplay, lpcText, mfStateTableX, mfStateTableY, 16.0f, lColour, false);
    mfStateTableY += KF_STATE_TABLE_ROW_HEIGHT;
}

void AIDebugComponent::StateTableBegin()
{
    mfStateTableX = KF_STATE_TABLE_ORIGIN;
    mfStateTableY = KF_STATE_TABLE_ORIGIN;
}

void AIDebugComponent::StateTableNextColumn()
{
    const f32 lfColumnWidth = (mfStateTableX == KF_STATE_TABLE_ORIGIN) ? KF_STATE_TABLE_LABEL_WIDTH
                                                                        : KF_STATE_TABLE_COLUMN_WIDTH;
    mfStateTableY  = KF_STATE_TABLE_ORIGIN;
    mfStateTableX += lfColumnWidth;
}

// The label column, then one column per active AI car (up to eight): indices, range state, speeds,
// route state, behaviour, aggression, position, sections, timers and, behind their toggles, the
// speed-calculation and personality values. The player car's column is green, the in-range car
// nearest the player white, other in-range cars red and out-of-range cars cyan.
void AIDebugComponent::DrawAIStatesTable(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    CgsDev::SimpleStrStream lStream;
    CgsDev::StrStreamBase&  lrStream = lStream;

    f32                 lfNearestDistance = 3.4028234663852886e+38f;
    EGlobalRaceCarIndex leNearestCar      = E_GLOBAL_RACE_CAR_INDEX_COUNT;
    s32                 liCarColumns      = 0;

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0; leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; leCar++)
    {
        const AICar* lpCar = mpAIModule->GetAICar(leCar);
        if (lpCar->IsActive() && lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE && !lpCar->mbIsPlayer)
        {
            const AICar* lpPlayerCar = mpAIModule->GetAICar(mpAIModule->mePlayerGlobalRaceCarIndex);
            const f32 lfDistance = rw::math::vpu::Magnitude(
                rw::math::vpu::operator-(lpPlayerCar->GetPosition(), lpCar->GetPosition()));
            if (lfDistance < lfNearestDistance)
            {
                lfNearestDistance = lfDistance;
                leNearestCar      = leCar;
            }
        }
    }

    StateTableBegin();
    StateTableDrawEntry(lpDisplay, "Global Index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Active Index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "State", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Opponent index", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Desired speed", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Racing line", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route status", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current checkpoint", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Behaviour", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding style", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding modifiers", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed matching", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to player", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Misc state", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression level", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Position", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Next route section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Destination section", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to checkpoint", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Buzz time", KU_STATE_TABLE_LABEL_COLOUR);
    StateTableDrawEntry(lpDisplay, "Wrong way time", KU_STATE_TABLE_LABEL_COLOUR);
    if (mbDrawSpeedCalculationStateOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Hard shoulder speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Proximity speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Cornering speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Fan Lerp", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Top Speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Accelerator", KU_STATE_TABLE_LABEL_COLOUR);
    }
    if (mbDrawrPersonalityOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Personality aggression", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality speed", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality skill", KU_STATE_TABLE_LABEL_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality overtake", KU_STATE_TABLE_LABEL_COLOUR);
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0; leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; leCar++)
    {
        if (liCarColumns >= KI_STATE_TABLE_MAX_CARS)
        {
            break;
        }

        const AICar* lpCar = mpAIModule->GetAICar(leCar);
        if (!lpCar->IsActive())
        {
            continue;
        }

        const AIDriver* lpDriver = (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? lpCar->GetDriver() : 0;

        ++liCarColumns;
        StateTableNextColumn();

        RGBA lColour;
        if (lpCar->mbIsPlayer)
        {
            lColour = 0xFF40C440u;
        }
        else if (leNearestCar == leCar)
        {
            lColour = 0xFFDCDCDCu;
        }
        else if (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE)
        {
            lColour = 0xFF4060FFu;
        }
        else
        {
            lColour = 0xFF40C4C4u;
        }

        // Global index
        lStream.Reset();
        lrStream << static_cast<s32>(leCar);
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Active index
        lStream.Reset();
        if (lpDriver != 0)
        {
            lrStream << lpDriver->GetRelatedActiveCarIndex();
        }
        else
        {
            lStream << "Invalid";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // State
        lStream.Reset();
        lStream << ((lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE) ? "InRange " : "OutOfRange ");
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Opponent index
        lStream.Reset();
        lrStream << static_cast<s32>(lpCar->GetOpponentIndex());
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Speed
        lStream.Reset();
        if (lpCar->IsCrashing())
        {
            lStream << "Crashing";
        }
        else
        {
            lStream << lpCar->GetSpeed() * KF_MPS_TO_MPH;
            lStream << "mph";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Desired speed
        lStream.Reset();
        lStream << lpCar->GetDesiredSpeed() * KF_MPS_TO_MPH;
        lStream << "mph";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Racing line
        lStream.Reset();
        if (lpDriver != 0)
        {
            lStream << (lpDriver->mbIsRacingLineInitialised ? "Valid" : "Invalid");
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Route status
        lStream.Reset();
        const Route* lpRoute = lpCar->GetRoute();
        if (lpCar->HasValidRoute())
        {
            switch (lpRoute->GetStatus())
            {
            case Route::E_STATUS_COMPLETE: lStream << KPC_ROUTE_STATUS_COMPLETE; break;
            case Route::E_STATUS_PARTIAL:  lStream << KPC_ROUTE_STATUS_PARTIAL;  break;
            case Route::E_STATUS_BLOCKED:  lStream << KPC_ROUTE_STATUS_BLOCK;    break;
            default:                       lStream << KPC_ROUTE_STATUS_UNKNOWN;  break;
            }
            lStream << " (";
            lrStream << lpRoute->GetNodeCount();
            lStream << ")";
        }
        else
        {
            lStream << "Invalid ";
            if (lpRoute->GetStatus() == Route::E_STATUS_BLOCKED)
            {
                lStream << KPC_ROUTE_INVALID_BLOCK;
            }
        }
        if (lpCar->NeedsNewRoute())
        {
            lStream << "NeedsNew ";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Current checkpoint
        lStream.Reset();
        lrStream << lpCar->GetCurrentCheckpoint();
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Behaviour
        StateTableDrawEntry(lpDisplay, KAPC_AI_BEHAVIOUR_NAMES[lpCar->meBehaviour], lColour);

        // Route finding style
        lStream.Reset();
        lStream << KAAC_ROUTE_FINDING_STYLE_NAMES[lpCar->GetRouteFindingStyle()];
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Route finding modifiers
        lStream.Reset();
        if (lpCar->HasBlockCheckpoints())
        {
            lStream << "Block ";
        }
        else
        {
            if (lpCar->WantsAlternativeRoute())
            {
                lStream << "Deviate ";
            }
            if (lpCar->UseAIShortcuts())
            {
                lStream << "Shortcuts ";
            }
        }
        if (lpCar->GetRouteFindingStyle() == E_ROUTE_FINDING_FREE_ROAM)
        {
            lStream << "FreeRoam";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Speed matching (the speed selection method)
        StateTableDrawEntry(lpDisplay, KAAC_SPEED_SELECTION_METHOD_NAMES[lpCar->meSpeedSelectionMethod], lColour);

        // Distance to player
        lStream.Reset();
        lStream << lpCar->GetDistanceToPlayer();
        lStream << "m ";
        lStream << (lpCar->IsAheadOfPlayer() ? "Ahead" : "Behind");
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Misc state
        lStream.Reset();
        if (lpDriver != 0)
        {
            if (lpDriver->mfBrake > 0.1f)
            {
                lStream << "Brake ";
            }
            if (lpDriver->mbBoosting)
            {
                lStream << "Boost ";
            }
            if (lpCar->mbIsDrifting)
            {
                lStream << (lpDriver->mbWantToExitDrift ? "Exit drift " : "Drift ");
            }
            if (lpCar->IsCrashing())
            {
                lStream << "Crash ";
            }
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Aggression level
        lStream.Reset();
        lStream << lpCar->mAggressiveness.GetAggressionLevel();
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Aggression
        lStream.Reset();
        if (lpDriver != 0)
        {
            lStream << KAPC_AGGRESSION_STATE_NAMES[lpDriver->GetAggression()->GetAggressionState()];
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Position
        const Vector3 lv3Position = lpCar->GetPosition();
        lStream.Reset();
        lrStream << static_cast<s32>(lv3Position.GetX().GetFloat());
        lStream << ",";
        lrStream << static_cast<s32>(lv3Position.GetY().GetFloat());
        lStream << ",";
        lrStream << static_cast<s32>(lv3Position.GetZ().GetFloat());
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Current section
        if (lpCar->muBestSectionIndex == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(lpCar->muBestSectionIndex);
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Next route section
        if (GetNextNextRouteSectionIndex(lpCar) == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(GetNextNextRouteSectionIndex(lpCar));
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Destination section
        if (lpCar->GetDestinationSectionIndex() == 0x7FFF)
        {
            StateTableDrawEntry(lpDisplay, "Invalid", lColour);
        }
        else
        {
            lStream.Reset();
            lrStream << static_cast<s32>(lpCar->GetDestinationSectionIndex());
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        // Distance to checkpoint
        lStream.Reset();
        if (lpCar->mfDistanceToCheckpoint != KF_NO_CHECKPOINT_DISTANCE)
        {
            lStream << lpCar->mfDistanceToCheckpoint;
            lStream << "m";
        }
        else
        {
            lStream << "Invalid";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Buzz time
        lStream.Reset();
        lStream << mpAIModule->mBuzzBy.GetBuzzFrequency(lpCar);
        lStream << "s";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Wrong way time
        lStream.Reset();
        lStream << lpCar->mfWrongWayTime;
        lStream << "s";
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        if (mbDrawSpeedCalculationStateOnTable && lpDriver != 0)
        {
            lStream.Reset();
            lStream << lpDriver->mfTopSpeed * KF_MPS_TO_MPH;
            lStream << "mph";
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

            lStream.Reset();
            lStream << lpDriver->mfAccelerator;
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }

        if (mbDrawrPersonalityOnTable)
        {
            lStream.Reset();
            lStream << lpCar->mAggressiveness.GetAggressionLevel();
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

            lStream.Reset();
            lStream << lpCar->mAggressiveness.GetProximityToSpeedMatch();
            StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);
        }
    }
}

}
