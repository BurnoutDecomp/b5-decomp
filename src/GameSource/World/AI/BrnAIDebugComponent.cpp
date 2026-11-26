#define CGS_ARTIST_UNITY_BRNAIDEBUGCOMPONENT_CPP_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAIDebugComponent.cpp"
#include "GameSource/World/AI/BrnAIDebugComponent.h"

#include "GameSource/World/AI/BrnAIModule.h"                                      // BrnAI::AIModule (member-pointer completeness)
#include "GameSource/World/AI/BrnAICar.h"
#include "GameSource/World/AI/BrnAIDriver.h"
#include "GameSource/World/AI/BrnAIAggression.h"
#include "GameSource/World/AI/BrnAIPortal.h"
#include "GameSource/World/AI/BrnAIDebugUtils.h"
#include "SDKs/XboxMath/XMVectorACos.h"
#include <cmath>
#include "GameSource/World/AI/BrnAIBuzzBy.h"
#include "GameSource/BurnoutConstants.h"                                           // EActiveRaceCarIndex operator++
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Core/CgsCoreConstants.h"                      // CgsCore::KF_MPS_TO_MPH (.bss 0x830180B0)
#include "GameShared/GameClasses/Core/CgsStringUtils.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"                       // CgsDev::SimpleStrStream
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug2DImmediateRender.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "GameShared/GameClasses/System/Resource/CgsResourcePtr.h"
#include "GameSource/World/AI/Route/BrnRoute.h"                                    // Route / RouteNode (state table)
#include "SharedClasses/AI/AISectionsResourceType.h"
#include "rw/math/vpu/vector3_operation.h"                                         // Magnitude / operator-

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
    // DWARF BrnAIDebugComponent.cpp:34..:183, in declaration order, with the image's values.
    // The class constants (declared h:200..:231): the two sizes are dynamically initialised
    // (K_CURRENT_CAR_SIZE .bss 0x8300D730, read by DrawAICarRoute 0x82773674), the colours are .data
    // 0x82F3039C..0x82F303DC, read from storage (r, g, b, a from the PS3 initialiser 0x9BC1B4), and the
    // scalars are ARTIST's AI unity .rdata 0x820C4350..0x820C4378 word for word.
    const Vector3  AIDebugComponent::K_CAR_SIZE(2.0f, 200.0f, 2.0f);                          // :34
    const Vector3  AIDebugComponent::K_CURRENT_CAR_SIZE(3.0f, 200.0f, 3.0f);                  // :35
    const rw::RGBA AIDebugComponent::KU_CAR_COLOUR(255, 68, 34, 255);                         // :36
    const rw::RGBA AIDebugComponent::KU_STATE_TEXT_COLOUR(255, 98, 64, 255);                  // :37
    const rw::RGBA AIDebugComponent::KU_HIGHLIGHT_TEXT_COLOUR(255, 255, 64, 255);             // :38
    const rw::RGBA AIDebugComponent::KU_UBER_HIGHLIGHT_TEXT_COLOUR(255, 255, 184, 255);       // :39
    const rw::RGBA AIDebugComponent::KU_CURRENT_CAR_COLOUR(170, 170, 170, 255);               // :40
    const rw::RGBA AIDebugComponent::KU_ROUTE_LINE_COLOUR(240, 0, 0, 255);                    // :41
    const rw::RGBA AIDebugComponent::KU_ROUTE_NODE_COLOUR(170, 0, 0, 255);                    // :42
    const rw::RGBA AIDebugComponent::KU_RACING_LINE_COLOUR1(0, 0, 255, 255);                  // :43
    const rw::RGBA AIDebugComponent::KU_RACING_LINE_COLOUR2(128, 128, 255, 255);              // :44
    const rw::RGBA AIDebugComponent::KU_TARGET_LINE_COLOUR(136, 136, 255, 255);               // :45
    const rw::RGBA AIDebugComponent::KU_CURRENT_NODE_COLOUR(0, 0, 0, 255);                    // :46
    const rw::RGBA AIDebugComponent::KU_VELOCITY_LINE_COLOUR(255, 0, 255, 255);               // :47
    const rw::RGBA AIDebugComponent::KU_LOOKAHEAD_LINE_COLOUR(255, 255, 0, 255);              // :48
    const rw::RGBA AIDebugComponent::KU_DRIVER_TARGET_LINE_COLOUR(0, 255, 255, 255);          // :49
    const rw::RGBA AIDebugComponent::KU_AGGRESSIVE_COLOUR(255, 0, 0, 255);                    // :50
    const rw::RGBA AIDebugComponent::KU_TRAFFIC_LINE_COLOUR(0, 255, 255, 255);                // :51
    const rw::RGBA AIDebugComponent::KU_BRAKING_COLOUR(0, 0, 0, 255);                         // :52
    const s32      AIDebugComponent::KI_NUM_SECTIONS_DISPLAYED;                               // :53  0x820C4350
    const f32      AIDebugComponent::KF_ROUTE_NODE_SIZE = 2.0f;                               // :54  0x820C4354
    const f32      AIDebugComponent::KF_CAR_YOFFSET = -2.0f;                                  // :55  0x820C4358
    const f32      AIDebugComponent::KF_ROUTE_YOFFSET = 2.0f;                                 // :56  0x820C435C
    const f32      AIDebugComponent::KF_AT_DISPLAY_VECTOR_LENGTH = 15.0f;                     // :57  0x820C4360
    bool           AIDebugComponent::msbDriversStayStill = false;                             // :59  .bss 0x8300D604
    bool           AIDebugComponent::msbDriversMaxAggression = false;                         // :60  .bss 0x8300D605
    const f32      AIDebugComponent::KF_STATE_TABLE_X = 50.0f;                                // :63  0x820C4364
    const f32      AIDebugComponent::KF_STATE_TABLE_Y = 50.0f;                                // :64  0x820C4368
    const f32      AIDebugComponent::KF_STATE_TABLE_SPACING_X = 130.0f;                       // :65  0x820C436C
    const f32      AIDebugComponent::KF_STATE_TABLE_HEADER_SPACING_X = 180.0f;                // :66  0x820C4370
    const f32      AIDebugComponent::KF_STATE_TABLE_SPACING_Y = 20.0f;                        // :67  0x820C4374
    const f32      AIDebugComponent::KF_STATE_TABLE_FONT_SIZE = 16.0f;                        // :68  0x820C4378

    // The PID graph's reference lines (OnActivate's AddFloat): :70 .bss 0x8300D608, :71/:72 .data
    // 0x82F30180/0x82F30184 (PS3 OnActivate 0x9BF268 names all three). The dump marks only :182/:183
    // `extern`: these file-scope variables and the pointer tables below are internal (static), which is
    // why ARTIST addresses the tables off one anchor (DrawAIStatesOnCar 0x8277ECA8: r25 = 0x82F30218,
    // KAPC_BEHAVIOUR at r25 - 0x90).
    static f32 KF_ZERO_FLOAT = 0.0f;                                                                  // :70
    static f32 KF_MINUS_ONE_FLOAT = -1.0f;                                                            // :71
    static f32 KF_ONE_FLOAT = 1.0f;                                                                   // :72

    const f32 KF_STRAIGHT_ANGLE = 180.0f;                                                      // :74  0x820C437C
    const f32 KF_RIGHT_ANGLE = 90.0f;                                                          // :75  0x820C4380
    const f32 KF_DEFAULT_CRUISE_SPEED = 80.0f;                                                 // :76  0x820C4384

    // The state table's name tables (DWARF types): the char[N][64] ones are .rdata (0x820C4388 /
    // 0x820C4548 / 0x820C4688), the `const char *[N]` ones arrays of non-const pointers in .data
    // (0x82F30188 / 0x82F301B4 / 0x82F301DC / 0x82F30218 / 0x82F30230), reloaded on each use.
    const s32 KI_MAX_ROUTE_FIND_STRING_LENGTH = 64;                                            // :78
    const char KAAC_ROUTE_FINDING_STYLE[7][KI_MAX_ROUTE_FIND_STRING_LENGTH] =                  // :79
    {
        "FreeRoam", "Race", "RoadRage", "Pursuit", "AvoidPlayer", "AlwaysStraight", "MarkedMan",
    };
    const char KAAC_SPEED_SELECTION_METHOD[5][KI_MAX_ROUTE_FIND_STRING_LENGTH] =               // :92
    {
        "FreeRoam", "Race", "MatchPlayer", "Personality", "Top",
    };
    static const char* KAPC_BEHAVIOUR[11] =                                                           // :102
    {
        "Stop", "RollingStart", "DriveThru", "Cruising", "Fighting", "QuickTurn", "SlowTurn",
        "Crashing", "Donut", "PostRaceWin", "PostRaceLose",
    };
    const char KAAC_DRIFT_BEHAVIOUR[4][KI_MAX_ROUTE_FIND_STRING_LENGTH] =                      // :118
    {
        "Normal", "Start", "Drift", "Exit",
    };
    static const char* KAPC_BIAS_NAMES[10] =                                                          // :127
    {
        "Race", "RaceDangerous", "Slam", "RoadRage", "CloseToPlayer", "SlamRivals", "HitOncoming",
        "SlamDangerous", "NoHNGCentering", "VeerAwayFromPlayer",
    };
    static const char* KAPC_AGGRESSION_STATE_NAMES[15] =                                              // :144
    {
        "OutOfRange", "OvertakeToSlam", "DropBackToSlam", "AttackSlam", "Wait", "Veer", "Passive",
        "FallPast", "BeFodder", "ClipOffBehind", "OvertakeFast", "OvertakeSlowly", "SpurtForward",
        "VeerExtreme", "HangAroundAhead",
    };
    static const char* KAPC_SPEED_MATCHING_NAMES[6] =                                                 // :164
    {
        "Disabled", "Enabled", "Slower", "SlowToClip", "OvertakeFast", "OvertakeSlowly",
    };
    static const char* KAPC_PERSONALITY_TYPE_NAMES[2] =                                               // :175
    {
        "Racing", "Aggression",
    };

    // DWARF :182/:183: non-const namespace globals. K_DRIFT_TEXT_POSITION is dynamically initialised
    // into .bss 0x8300D760 (CRT 0x82C69148: 400 from 0x8200889C, 500 from 0x820C478C); KF_DRIFT_TEXT_SIZE
    // is .data 0x82F30238 (20.0), read from storage by RenderHUD's inlined DrawDriftingDebug (0x8277E52C).
    Vector2 K_DRIFT_TEXT_POSITION(400.0f, 500.0f);                                             // :182
    f32 KF_DRIFT_TEXT_SIZE = 20.0f;                                                            // :183

    // The marked-man tunables OnActivate registers (PS3 0x9BF268 names all four): DWARF
    // BrnAICar.cpp:111/:112 and BrnAIAggression.cpp:74/:75, the non-const file-scope globals of
    // those unity members (.data 0x82F30170/0x82F30174, .bss 0x8300D6F0/0x8300D714). The unity TU
    // sees their definitions; these declarations let this member also compile on its own.
    extern f32 KF_MARKED_MAN_MIN_AGGRESSION;
    extern f32 KF_MARKED_MAN_MAX_AGGRESSION;
    extern f32 KF_MAX_MAX_MARKED_MAN_OVERTAKE_SPEED;
    extern f32 KF_MAX_MIN_MARKED_MAN_OVERTAKE_SPEED;


// ARTIST82773B48: active fighting drivers with an aggression target draw both primitives
// (DWARF :1458..:1472 locals).
void AIDebugComponent::DrawAggressiveTargetPoints(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 liDriverIndex = 0; liDriverIndex < KI_MAX_ACTIVE_RACE_CARS; ++liDriverIndex)
    {
        const AIDriver* lpDriver = &mpAIModule->maAIDrivers[liDriverIndex];            // :1462
        if (lpDriver->mbIsActive && lpDriver->GetCar()->meBehaviour == E_AI_BEHAVIOUR_FIGHTING &&
            lpDriver->mAggression.mbTargetPosValid)
        {
            Vector3 lCarPos = lpDriver->GetCar()->GetPosition();                       // :1471
            Vector3 lTargetPos = lpDriver->mAggression.mTargetPos;                     // :1472
            lpDisplay->DrawSphere(lTargetPos, 1.5f, KU_AGGRESSIVE_COLOUR);
            lpDisplay->DrawLine(lCarPos, lTargetPos, KU_AGGRESSIVE_COLOUR);
        }
    }
}

// ARTIST82773C18: warn over the targeted car while the fighting driver is in its slam state
// (DWARF :1492..:1506 locals; the target's height through VecFloatRef<Y> + float and SetY).
void AIDebugComponent::DrawAggressiveWarning(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 liDriverIndex = 0; liDriverIndex < KI_MAX_ACTIVE_RACE_CARS; ++liDriverIndex)
    {
        const AIDriver* lpDriver = &mpAIModule->maAIDrivers[liDriverIndex];            // :1496
        if (lpDriver->mbIsActive && lpDriver->GetCar()->meBehaviour == E_AI_BEHAVIOUR_FIGHTING &&
            lpDriver->mAggression.meAggressionState == E_AI_AGGRESSION_STATE_ATTACK_SLAM &&
            lpDriver->mAggression.mpTargetCar)
        {
            Vector3 lTargetPos = lpDriver->mAggression.mpTargetCar->GetPosition();     // :1506
            lTargetPos.SetY(lTargetPos.GetY() + 2.0f);
            lpDisplay->DrawText(lTargetPos, "DANGER", 40.0f, KU_AGGRESSIVE_COLOUR);
        }
    }
}

// ARTIST827743A0: each active driver's index over its car ("%d", "%d*" while speed matching), in the
// aggressive colour while fighting (DWARF :1665..:1672 locals).
void AIDebugComponent::DrawCarIndices(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 liDriverIndex = 0; liDriverIndex < KI_MAX_ACTIVE_RACE_CARS; ++liDriverIndex)
    {
        const AIDriver* lpDriver = &mpAIModule->maAIDrivers[liDriverIndex];            // :1670
        if (lpDriver->mbIsActive)
        {
            Vector3 lCarPos = lpDriver->GetCar()->GetPosition();                       // :1671
            char lacBuff[10];                                                          // :1672
            CgsCore::SPrintf(lacBuff, sizeof(lacBuff), "%d", liDriverIndex);
            if (lpDriver->GetCar()->meBehaviour == E_AI_BEHAVIOUR_FIGHTING)
            {
                if (lpDriver->mAggression.meSpeedMatchType != 0)
                {
                    CgsCore::SPrintf(lacBuff, sizeof(lacBuff), "%d*", liDriverIndex);
                }
                lpDisplay->DrawText(lCarPos, lacBuff, 40.0f, KU_AGGRESSIVE_COLOUR);
            }
            else
            {
                lpDisplay->DrawText(lCarPos, lacBuff, 24.0f, KU_STATE_TEXT_COLOUR);
            }
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
                    if (!(lfForwardAngle > KF_RIGHT_ANGLE))
                    {
                        const BoundaryLine* lpBoundary = lpSection->GetPortal(static_cast<u8>(j))->GetBoundaryLine(0);
                        CGS_ASSERT(lpBoundary != 0, "lpBoundry != NULL");
                        const Vector3 lBoundary(lpBoundary->mfStartX - lpBoundary->mfEndX, 0.0f,
                            lpBoundary->mfStartY - lpBoundary->mfEndY);
                        const Vector3 lBoundaryDirection = NormalizeDebugVectorOne(NormalizeDebugVector(lBoundary));
                        f32 lfAngle = std::fabs(XboxMath::XMVectorACos(DebugClampedDot3(
                            lBoundaryDirection, NormalizeDebugVectorOne(lCarDirection))) * 57.295780181884766f);
                        CGS_ASSERT(lfAngle <= KF_STRAIGHT_ANGLE, "lfAlphaProportion <= KF_STRAIGHT_ANGLE");
                        if (lfAngle > KF_RIGHT_ANGLE) lfAngle = KF_STRAIGHT_ANGLE - lfAngle;
                        __vector4 lScale; lScale.x = 128.0f; lScale.y = lScale.z = lScale.w = 128.0f;
                        __vector4 lFraction; lFraction.x = lfAngle / KF_RIGHT_ANGLE;
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
                    lpDisplay->DrawLine(lStart, lEnd, KU_TRAFFIC_LINE_COLOUR);
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

// ARTIST827738F8: each active driver's steering target and ground-plane velocity as lines from its
// car (DWARF :1408..:1436 locals; the SDK Normalize and the VecFloat operators).
void AIDebugComponent::DrawCarControlData(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    for (s32 liDriverIndex = 0; liDriverIndex < KI_MAX_ACTIVE_RACE_CARS; ++liDriverIndex)
    {
        AIDriver* lpDriver = &mpAIModule->maAIDrivers[liDriverIndex];                  // :1412
        if (lpDriver->mpCar)
        {
            Vector3 lCarPos = lpDriver->mpCar->GetPosition();                         // :1417
            Vector3 lLineStart = lCarPos;                                             // :1418
            Vector3 lLineEnd;                                                         // :1419
            if (lpDriver->mbIsActive && lpDriver->mbIsRacingLineInitialised)
            {
                Vector3 lTargetVector = rw::math::vpu::Normalize(lpDriver->GetDirectVector3D()); // :1429
                lLineEnd = lLineStart + lTargetVector * KF_AT_DISPLAY_VECTOR_LENGTH;
                lpDisplay->DrawLine(lLineStart, lLineEnd, KU_DRIVER_TARGET_LINE_COLOUR);

                Vector3 lVelocityVector = lpDriver->mpCar->GetVelocity();             // :1436
                lVelocityVector.SetY(0.0f);
                lVelocityVector = rw::math::vpu::Normalize(lVelocityVector);
                lLineEnd = lLineStart + lVelocityVector * KF_AT_DISPLAY_VECTOR_LENGTH;
                lpDisplay->DrawLine(lLineStart, lLineEnd, KU_VELOCITY_LINE_COLOUR);
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
                lpDisplay->DrawSolidSphere(lTarget, 2.0f, KU_ROUTE_LINE_COLOUR);
            }
        }
    }
}

// ARTIST827735D8: the selected car marker and successive route-node segments.
void AIDebugComponent::DrawAICarRoute(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar& lrCar = mpAIModule->maAICars[miCurrentAICar];
    Vector3 lCarPosition = lrCar.GetPosition();
    lCarPosition.mV.y += KF_CAR_YOFFSET;
    lpDisplay->DrawSolidBox(lCarPosition + K_CURRENT_CAR_SIZE, lCarPosition - K_CURRENT_CAR_SIZE, KU_CURRENT_CAR_COLOUR);
    const Route* lpRoute = lrCar.GetRoute();
    Vector3 lPrevious(0.0f, 0.0f, 0.0f);
    for (s32 i = 0; i < lpRoute->GetNodeCount(); ++i)
    {
        const RouteNode* lpNode = lpRoute->GetNode(i);
        const Vector3 lCurrent(lpNode->GetPositionX(), KF_ROUTE_YOFFSET, lpNode->GetPositionZ());
        if (i)
        {
            lpDisplay->DrawLine(lPrevious, lCurrent,
                                i == lrCar.miNextRouteNodeIndex ? KU_RACING_LINE_COLOUR1 : KU_ROUTE_LINE_COLOUR);
        }
        lPrevious = lCurrent;
    }
}

// ARTIST827671C8: draw the current car's reset-on-track section: the line between its two portals and
// a box on each (DWARF :1220..:1226 locals; the four extents are Vector3 temporaries).
void AIDebugComponent::DrawResetOnTrackAISection(CgsDev::Debug3DImmediateRender* lpDisplay) const
{
    const AICar* lpAICar = &mpAIModule->maAICars[miCurrentAICar];                       // :1220
    if (lpAICar->muResetOnTrackSectionIndex != 0x7FFF)
    {
        const AISection* lpAISection = mpAISectionsData->GetAISection(lpAICar->muResetOnTrackSectionIndex); // :1224
        Vector3 lStartPortalPosition = lpAISection->GetPortal(lpAICar->muResetOnTrackStartPortal)->GetPosition(); // :1225
        Vector3 lEndPortalPosition = lpAISection->GetPortal(lpAICar->muResetOnTrackEndPortal)->GetPosition();     // :1226
        lpDisplay->DrawLine(lStartPortalPosition, lEndPortalPosition, rw::RGBA(255, 255, 0, 255));
        lpDisplay->DrawSolidBox(lStartPortalPosition - Vector3(3.0f, 3.0f, 3.0f),
                                lStartPortalPosition + Vector3(3.0f, 3.0f, 3.0f), rw::RGBA(255, 0, 0, 255));
        lpDisplay->DrawSolidBox(lEndPortalPosition - Vector3(0.25f, 0.25f, 0.25f),
                                lEndPortalPosition + Vector3(0.25f, 0.25f, 0.25f), rw::RGBA(0, 0, 255, 255));
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
        const s32 liCount = liRemaining < KI_NUM_SECTIONS_DISPLAYED ? liRemaining : KI_NUM_SECTIONS_DISPLAYED;
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
            lpDisplay->DrawLine(lPortal, lCar, KU_DRIVER_TARGET_LINE_COLOUR);
        }
    }
}

// ARTIST82796A08: preserve each original flag and per-draw call in console order.
void AIDebugComponent::RenderWorld(CgsDev::Debug3DImmediateRender* lpDisplay)
{
    static const char KAC_ARTIST_ASSERT_FILE[] =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../World/AI/BrnAIDebugComponent.cpp";
    CGS_ASSERT_AT(mpAIModule != 0, "mpAIModule != NULL", KAC_ARTIST_ASSERT_FILE, 283);
    if (mbDrawAllDriverData)
    {
        mbDrawRacingLines = true;
        mbDrawPortalTargets = true;
        mbDrawDirectVector = true;
        mbDrawControlData = true;
    }
    if (mbDrawAICarStatesOnCar) DrawAIStatesOnCar(lpDisplay);
    if (mbDrawHNGMap)
    {
        AIDriver& lrDriver = mpAIModule->maAIDrivers[0];
        if (lrDriver.mbIsActive && lrDriver.mbIsRacingLineInitialised)
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
    mfCruisingSpeed = KF_DEFAULT_CRUISE_SPEED;
    mPIDDebugData.meCoefficientToBeTuned = E_COEFFICIENT_COUNT;
    mPIDDebugData.mfPCoefficient = 0.0f;
    mPIDDebugData.mfICoefficient = 0.0f;
    mPIDDebugData.mfDCoefficient = 0.0f;
}

// Inlined by AIModule::Destruct at 0x8276E3C0..0x8276E400.
void AIDebugComponent::Destruct()
{
    // ARTIST (inlined into AIModule::Destruct, unity TU): li r5,0x109 at 0x8276E3DC, then the base
    // DebugComponent::Destruct (bl 0x8284CB38, the folded empty body) at 0x8276E400.
    CGS_ASSERT_AT(mpAIModule != 0, "mpAIModule != NULL", CGS_ARTIST_UNITY_BRNAIDEBUGCOMPONENT_CPP_FILE, 265);
    mpAIModule = 0;
    CgsDev::DebugComponent::Destruct();
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
    RegisterVariable(&KF_MAX_MIN_MARKED_MAN_OVERTAKE_SPEED, lpcMarkedMan, "Min overtake speed");
    RegisterVariable(&KF_MAX_MAX_MARKED_MAN_OVERTAKE_SPEED, lpcMarkedMan, "Max overtake speed");
    RegisterVariable(&KF_MARKED_MAN_MIN_AGGRESSION, lpcMarkedMan, "Min aggression");
    RegisterVariable(&KF_MARKED_MAN_MAX_AGGRESSION, lpcMarkedMan, "Max aggression");
    SetStep(&KF_MARKED_MAN_MIN_AGGRESSION, 0.05f);
    SetStep(&KF_MARKED_MAN_MAX_AGGRESSION, 0.05f);
    SetRange(&KF_MARKED_MAN_MIN_AGGRESSION, 0.0f, 1.0f);
    SetRange(&KF_MARKED_MAN_MAX_AGGRESSION, 0.0f, 1.0f);

    RegisterVariable(&mpAIModule->mbHighTakenDownPenalty, "Easy mode");
    RegisterVariable(&mbDrawCarUnderSection, "Draw section under car");
    RegisterFunction(&DeactiveateDriversCallback, mpAIModule, "Turn Drivers On / Off");
    RegisterVariable(&msbDriversStayStill, "Drivers stay still");
    RegisterVariable(&msbDriversMaxAggression, "Drivers Max Aggression");
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

    AIDriver& lrDriver = mpAIModule->maAIDrivers[0];
    mSteerAngleVsPIDOutputGraph.Prepare();
    RegisterVariable(&lrDriver.mfCalculatedSteeringAngle, lpcPID, "Error (steer)");
    RegisterVariable(&lrDriver.mfPIDOutput, lpcPID, "PID Output 1");
    AddMenuItem("PID/SteerAngle vs PID Graph", &mSteerAngleVsPIDOutputGraph);
    mSteerAngleVsPIDOutputGraph.AddFloat(&KF_ZERO_FLOAT);
    mSteerAngleVsPIDOutputGraph.AddFloat(&KF_MINUS_ONE_FLOAT);
    mSteerAngleVsPIDOutputGraph.AddFloat(&KF_ONE_FLOAT);
    mSteerAngleVsPIDOutputGraph.AddFloat(&lrDriver.mfCalculatedSteeringAngle);
    mSteerAngleVsPIDOutputGraph.AddFloat(&mPIDDebugData.mfPCoefficient);

    SetRange(&miCurrentAICar, 0, KI_MAX_OUT_OF_RANGE_RACE_CARS - 1);
    RegisterVariable(&mbDrawChevrons, "Legacy", "Draw chevrons");
    RegisterVariable(&RacingLineGenerator::mbDrawAvoidanceDebug, "Legacy", "Draw avoidance debug");
}

namespace
{
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

}

// The index of the AI section under the player car (or "Invalid"), the drift state of the current
// AI car, the AI state table and the buzz-by timer.
void AIDebugComponent::RenderHUD(CgsDev::Debug2DImmediateRender* lpDisplay)
{
    if (mbDrawCarUnderSection)
    {
        AICar* lpAICar = mpAIModule->GetPlayerCar();   // DWARF :389
        if (lpAICar->muBestSectionIndex == 0x7FFF)
        {
            lpDisplay->DrawText("Invalid", K_DRIFT_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
        }
        else
        {
            CgsDev::SimpleStrStream lStream;
            lStream.Reset();
            lStream << static_cast<s32>(lpAICar->muBestSectionIndex);
            lpDisplay->DrawText(lStream.GetBuffer(), K_DRIFT_TEXT_POSITION, 40.0f, 0xFFFFFFFFu, false);
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
        lpDisplay->DrawText(KAAC_DRIFT_BEHAVIOUR[lrCar.GetDriver()->GetDriftState()], K_DRIFT_TEXT_POSITION,
                            KF_DRIFT_TEXT_SIZE, 0xFFFFFFFFu, false);
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
            lStream << KAPC_BEHAVIOUR[lpCar->meBehaviour];
            lStream << " ";
        }
        if (mbDrawSpeedOnCar)
        {
            lStream << lpCar->GetSpeed() * CgsCore::KF_MPS_TO_MPH;
            lStream << "mph ";
        }
        if (mbDrawFanModeOnCar)
        {
            lStream << KAPC_BIAS_NAMES[lpDriver->mSteeringFan.meBiasMode];
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
            lStream << KAPC_SPEED_MATCHING_NAMES[lrAggression.meSpeedMatchType];
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
void AIDebugComponent::StateTableDrawEntry(CgsDev::Debug2DImmediateRender* lpDisplay, const char* lpcText, rw::RGBA lColour)
{
    const s32 liRow = static_cast<s32>(mfStateTableY / KF_STATE_TABLE_SPACING_Y);
    if (liRow % 2 == 0)
    {
        lColour = rw::RGBA(lColour.GetRed() * 2 / 3, lColour.GetGreen() * 2 / 3, lColour.GetBlue() * 2 / 3, 255);
    }

    lpDisplay->DrawText(lpcText, mfStateTableX, mfStateTableY, KF_STATE_TABLE_FONT_SIZE, lColour.m_rgba);
    mfStateTableY += KF_STATE_TABLE_SPACING_Y;
}

void AIDebugComponent::StateTableBegin()
{
    mfStateTableX = KF_STATE_TABLE_X;
    mfStateTableY = KF_STATE_TABLE_Y;
}

// The header column is wider than the per-car columns (PS3 0x9B5854 folds the header step to 230).
void AIDebugComponent::StateTableNextColumn()
{
    if (mfStateTableX == KF_STATE_TABLE_X)
    {
        mfStateTableX += KF_STATE_TABLE_HEADER_SPACING_X;
    }
    else
    {
        mfStateTableX += KF_STATE_TABLE_SPACING_X;
    }
    mfStateTableY = KF_STATE_TABLE_Y;
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
    StateTableDrawEntry(lpDisplay, "Global Index", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Active Index", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "State", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Opponent index", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Desired speed", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Racing line", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route status", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current checkpoint", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Behaviour", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding style", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Route finding modifiers", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Speed matching", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to player", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Misc state", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression level", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Aggression", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Position", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Current section", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Next route section", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Destination section", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Distance to checkpoint", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Buzz time", KU_STATE_TEXT_COLOUR);
    StateTableDrawEntry(lpDisplay, "Wrong way time", KU_STATE_TEXT_COLOUR);
    if (mbDrawSpeedCalculationStateOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Hard shoulder speed", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Proximity speed", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Cornering speed", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Fan Lerp", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Top Speed", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Accelerator", KU_STATE_TEXT_COLOUR);
    }
    if (mbDrawrPersonalityOnTable)
    {
        StateTableDrawEntry(lpDisplay, "Personality aggression", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality speed", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality skill", KU_STATE_TEXT_COLOUR);
        StateTableDrawEntry(lpDisplay, "Personality overtake", KU_STATE_TEXT_COLOUR);
    }

    for (EGlobalRaceCarIndex leCar = E_GLOBAL_RACE_CAR_INDEX_0; leCar < E_GLOBAL_RACE_CAR_INDEX_COUNT; leCar++)
    {
        if (liCarColumns >= KI_MAX_ACTIVE_RACE_CARS)
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

        rw::RGBA lColour;   // DWARF :674 RGBA
        if (lpCar->mbIsPlayer)
        {
            lColour = rw::RGBA(64, 196, 64, 255);
        }
        else if (leNearestCar == leCar)
        {
            lColour = rw::RGBA(220, 220, 220, 255);
        }
        else if (lpCar->meCarState == E_AI_CAR_STATE_IN_RANGE)
        {
            lColour = rw::RGBA(255, 96, 64, 255);
        }
        else
        {
            lColour = rw::RGBA(196, 196, 64, 255);
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
            lStream << lpCar->GetSpeed() * CgsCore::KF_MPS_TO_MPH;
            lStream << "mph";
        }
        StateTableDrawEntry(lpDisplay, lStream.GetBuffer(), lColour);

        // Desired speed
        lStream.Reset();
        lStream << lpCar->GetDesiredSpeed() * CgsCore::KF_MPS_TO_MPH;
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
            case Route::E_STATUS_COMPLETE: lStream << "Complete"; break;
            case Route::E_STATUS_PARTIAL:  lStream << "Partial";  break;
            case Route::E_STATUS_BLOCKED:  lStream << "Blocked";    break;
            default:                       lStream << "Unknown";  break;
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
                lStream << "Blocked ";
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
        StateTableDrawEntry(lpDisplay, KAPC_BEHAVIOUR[lpCar->meBehaviour], lColour);

        // Route finding style
        lStream.Reset();
        lStream << KAAC_ROUTE_FINDING_STYLE[lpCar->GetRouteFindingStyle()];
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
        StateTableDrawEntry(lpDisplay, KAAC_SPEED_SELECTION_METHOD[lpCar->meSpeedSelectionMethod], lColour);

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
        if (lpCar->mfDistanceToCheckpoint != FLT_MAX)
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
            lStream << lpDriver->mfTopSpeed * CgsCore::KF_MPS_TO_MPH;
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
