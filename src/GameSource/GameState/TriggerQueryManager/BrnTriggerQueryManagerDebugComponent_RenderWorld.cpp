#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManagerDebugComponent.h"

#include "GameSource/GameState/TriggerQueryManager/BrnTriggerQueryManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"
#include "GameShared/GameClasses/Development/DebugSystem/Render/CgsDebug3DImmediateRender.h"
#include "SharedClasses/Trigger/BrnGenericRegion.h"
#include "SharedClasses/Trigger/BrnLandmark.h"
#include "SharedClasses/Trigger/BrnRegion.h"
#include "SharedClasses/Trigger/BrnRoamingLocation.h"
#include "SharedClasses/Trigger/BrnSpawnLocation.h"
#include "SharedClasses/Trigger/BrnTriggerBase.h"
#include "SharedClasses/Trigger/BrnTriggerData.h"
#include <cmath>

namespace
{
    f32 DistanceSquared(const rw::math::vpu::Vector3& lrPoint,
                        const rw::math::vpu::Vector3& lrOrigin)
    {
        const f32 lfX = lrPoint.x - lrOrigin.x;
        const f32 lfY = lrPoint.y - lrOrigin.y;
        const f32 lfZ = lrPoint.z - lrOrigin.z;
        // ARTIST RenderWorld uses vmsum3fp128 for each squared-distance test: the three
        // products are accumulated in one rounded result rather than three scalar additions.
        return static_cast<f32>(static_cast<f64>(lfX) * lfX +
                                static_cast<f64>(lfY) * lfY +
                                static_cast<f64>(lfZ) * lfZ);
    }

    rw::math::vpu::Vector3 AddScaledDirection(const rw::math::vpu::Vector3& lrPosition,
                                             const rw::math::vpu::Vector3& lrDirection,
                                             f32 lfScale)
    {
        return rw::math::vpu::Vector3{std::fma(lrDirection.x, lfScale, lrPosition.x),
                                      std::fma(lrDirection.y, lfScale, lrPosition.y),
                                      std::fma(lrDirection.z, lfScale, lrPosition.z),
                                      std::fma(lrDirection.w, lfScale, lrPosition.w)};
    }
}

namespace BrnGameState
{
    // X360 0x82364E10. Draw the selected track trigger regions and the optional roaming,
    // spawn, and landmark start-position markers. The registered fields are read in the same
    // groups and order as the console body; all data access is through the named trigger and
    // manager members.
    void TriggerQueryManagerDebugComponent::RenderWorld(
        CgsDev::Debug3DImmediateRender* lpRender)
    {
        const f32 lfClipDistanceSquared = mfClipDistance * mfClipDistance;
        const f32 lfTextClipDistanceSquared = mfTextClipDistance * mfTextClipDistance;

        if (mbRenderTriggerRegions)
        {
            const s32 liRegionCount = mpTriggerData->GetRegionCount();
            CgsDev::SimpleStrStream lTextStream;

            for (s32 liRegionIndex = 0; liRegionIndex < liRegionCount; ++liRegionIndex)
            {
                if (liRegionIndex >= mpTriggerData->GetRegionCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liRegionIndex < miRegionCount",
                        "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
                        624);
                    CgsDev::Assert::EndAssert();
                }

                const BrnTrigger::TriggerRegion* lpRegion =
                    mpTriggerData->GetRegionUnchecked(liRegionIndex);
                const BrnTrigger::BoxRegion* lpBox = lpRegion->GetBoxRegion();
                const BrnTrigger::TriggerRegion::Type leRegionType = lpRegion->GetType();
                const rw::math::vpu::Vector3 lPosition = lpBox->GetPosition();
                const rw::math::vpu::Vector3 lDimensions = lpBox->GetDimensions();
                if (DistanceSquared(lPosition, lpRender->GetCameraPosition()) < lfClipDistanceSquared)
                {
                    const s32 liRegionType = static_cast<s32>(leRegionType);
                    if (miTriggerType == BrnTrigger::TriggerRegion::E_TYPE_COUNT ||
                        miTriggerType == liRegionType)
                    {
                        const BrnTrigger::GenericRegion* lpGenericRegion = 0;
                        s32 liGenericRegionType = BrnTrigger::GenericRegion::E_TYPE_COUNT;
                        if (leRegionType == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                        {
                            lpGenericRegion =
                                static_cast<const BrnTrigger::GenericRegion*>(lpRegion);
                            liGenericRegionType = static_cast<s32>(lpGenericRegion->GetType());
                        }

                        if (leRegionType != BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION ||
                            miGenericRegionType == BrnTrigger::GenericRegion::E_TYPE_COUNT ||
                            miGenericRegionType == liGenericRegionType)
                        {
                            // Raw ARTIST words (big-endian): 82CDBD98=FF00FFFF (landmark),
                            // 82CDBDAC=FF404040 (blackspot), 82CDBDA4=FFFF0000 (generic),
                            // 82CDBDA8=FF00FF00 (VFX), 82CDBDB4=FF0000FF (highlight), and
                            // 82CDBDB0=FF404040 (currently active generic). rw::RGBA stores
                            // AABBGGRR, so these are the same packed values on the PC target.
                            rw::RGBA lColour(0, 0, 255, 255);
                            bool lbHighlightedGroup = false;
                            lTextStream.Reset();

                            switch (leRegionType)
                            {
                            case BrnTrigger::TriggerRegion::E_TYPE_LANDMARK:
                            {
                                const BrnTrigger::Landmark* lpLandmark =
                                    static_cast<const BrnTrigger::Landmark*>(lpRegion);
                                lColour = rw::RGBA(255, 255, 0, 255);
                                lTextStream << "Landmark (";
                                lTextStream << static_cast<s32>(lpLandmark->GetDesignIndex());
                                lTextStream << ")";
                                break;
                            }
                            case BrnTrigger::TriggerRegion::E_TYPE_BLACKSPOT:
                                lColour = rw::RGBA(64, 64, 64, 255);
                                lTextStream << "Blackspot";
                                break;
                            case BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION:
                            {
                                lColour = rw::RGBA(0, 0, 255, 255);
                                const char* lpcTypeName = lpGenericRegion->GetTypeName();
                                if (lpcTypeName == 0)
                                    lpcTypeName = "<NULLSTRING>";
                                lTextStream << lpcTypeName;

                                const BrnTrigger::GenericRegion::Type leGenericType =
                                    lpGenericRegion->GetType();
                                if (leGenericType == BrnTrigger::GenericRegion::E_TYPE_SIGNATURE_TAKEDOWN ||
                                    leGenericType == BrnTrigger::GenericRegion::E_TYPE_JUMP ||
                                    leGenericType == BrnTrigger::GenericRegion::E_TYPE_ROAD_LIMIT)
                                {
                                    CgsID lGroupId = lpGenericRegion->GetGroupId();
                                    if (lGroupId == 0)
                                        lGroupId = lpRegion->GetId();

                                    if (lpGenericRegion->GetGroupId() != 0 &&
                                        lGroupId != lpRegion->GetId())
                                    {
                                        lbHighlightedGroup =
                                            lGroupId == mpTriggerQueryManager->mPlayerSigTakedownGroupID ||
                                            lGroupId == mpTriggerQueryManager->mPlayerSuperJumpGroupID ||
                                            lGroupId == mpTriggerQueryManager->mPlayerRoadLimitGroupID;
                                    }

                                    if (lGroupId == 0)
                                    {
                                        CgsDev::Assert::BeginAssert();
                                        CgsDev::Assert::FireAssert(
                                            "lGroupID != 0",
                                            "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../GameState/TriggerQueryManager/BrnTriggerQueryManagerDebugComponent.cpp",
                                            260);
                                        CgsDev::Assert::EndAssert();
                                    }

                                    lTextStream << " (";
                                    lTextStream << static_cast<s32>(lGroupId);
                                    lTextStream << ")";
                                }
                                break;
                            }
                            case BrnTrigger::TriggerRegion::E_TYPE_VFXBOX_REGION:
                                lColour = rw::RGBA(0, 255, 0, 255);
                                lTextStream << "VFXBoxRegion";
                                break;
                            default:
                                lTextStream << "Unknown";
                                break;
                            }

                            const u16 luRegionIndex = static_cast<u16>(liRegionIndex);
                            bool lbHighlighted = lbHighlightedGroup;
                            if (!lbHighlighted)
                            {
                                const s32 liPreviousFrameIndex =
                                    mpTriggerQueryManager->maLastFrameTriggers.FindFirstInstanceOf(
                                        luRegionIndex);
                                lbHighlighted = static_cast<u32>(liPreviousFrameIndex) <
                                    mpTriggerQueryManager->maLastFrameTriggers.GetLength();
                            }
                            if (lbHighlighted)
                            {
                                lColour = rw::RGBA(255, 0, 0, 255);
                            }
                            else if (leRegionType == BrnTrigger::TriggerRegion::E_TYPE_GENERIC_REGION)
                            {
                                const s32 liActiveIndex =
                                    mpTriggerQueryManager->maActiveTriggers.FindFirstInstanceOf(
                                        luRegionIndex);
                                if (static_cast<u32>(liActiveIndex) <
                                    mpTriggerQueryManager->maActiveTriggers.GetLength())
                                {
                                    lColour = rw::RGBA(64, 64, 64, 255);
                                }
                            }

                            const rw::math::vpu::Vector3 lHalfDimensions =
                            {
                                lDimensions.x * 0.5f,
                                lDimensions.y * 0.5f,
                                lDimensions.z * 0.5f,
                                0.0f
                            };
                            const rw::math::vpu::Vector3 lMin =
                            {
                                -lHalfDimensions.x,
                                -lHalfDimensions.y,
                                -lHalfDimensions.z,
                                0.0f
                            };
                            const rw::math::vpu::Vector3 lMax = lHalfDimensions;

                            lpRender->DrawBox(lMin, lMax, lpBox->ComputeTransform(), lColour);

                            rw::RGBA lSolidColour;
                            lSolidColour.m_rgba = (lColour.m_rgba & 0x00FFFFFFu) | 0x40000000u;
                            lpRender->DrawSolidBox(lMin, lMax, lpBox->ComputeTransform(), lSolidColour);

                            if (mbRenderAxis)
                                lpRender->DrawAxis(lpBox->ComputeTransform());

                            if (mbRenderInfoText &&
                                DistanceSquared(lPosition, lpRender->GetCameraPosition()) < lfTextClipDistanceSquared)
                            {
                                if (mbRenderTriggerIds)
                                {
                                    lTextStream << " (";
                                    lTextStream << liRegionIndex;
                                    lTextStream << ",  ";
                                    lTextStream << static_cast<s32>(lpRegion->GetId());
                                    lTextStream << ")";
                                }
                                lpRender->DrawText(lPosition, lTextStream.GetBuffer(), 12.0f);
                            }
                        }
                    }
                }
            }
        }

        if (mbRenderRoamingLocations)
        {
            const s32 liRoamingLocationCount = mpTriggerData->GetRoamingLocationCount();
            for (s32 liRoamingLocationIndex = 0;
                 liRoamingLocationIndex < liRoamingLocationCount;
                 ++liRoamingLocationIndex)
            {
                if (liRoamingLocationIndex >= mpTriggerData->GetRoamingLocationCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liRoamingLocationIndex < miRoamingLocationCount",
                        "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
                        569);
                    CgsDev::Assert::EndAssert();
                }

                const BrnTrigger::RoamingLocation* lpLocation =
                    mpTriggerData->GetRoamingLocationUnchecked(liRoamingLocationIndex);
                const rw::math::vpu::Vector3 lPosition = lpLocation->GetPosition();
                if (DistanceSquared(lPosition, lpRender->GetCameraPosition()) < lfClipDistanceSquared)
                {
                    // ARTIST holds 0xFF0000FF in r23 for roaming spheres.
                    lpRender->DrawSphere(lPosition, 5.0f, rw::RGBA(255, 0, 0, 255));
                }
            }
        }

        if (mbRenderSpawnLocations)
        {
            const s32 liSpawnLocationCount = mpTriggerData->GetSpawnLocationCount();
            for (s32 liSpawnLocationIndex = 0;
                 liSpawnLocationIndex < liSpawnLocationCount;
                 ++liSpawnLocationIndex)
            {
                if (liSpawnLocationIndex >= mpTriggerData->GetSpawnLocationCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liSpawnLocationIndex < miSpawnLocationCount",
                        "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
                        585);
                    CgsDev::Assert::EndAssert();
                }

                const BrnTrigger::SpawnLocation* lpLocation =
                    mpTriggerData->GetSpawnLocationUnchecked(liSpawnLocationIndex);
                const rw::math::vpu::Vector3 lPosition = lpLocation->mPosition;
                if (DistanceSquared(lPosition, lpRender->GetCameraPosition()) < lfClipDistanceSquared)
                {
                    // ARTIST uses 0xFF00FF00 for the spawn sphere, then -16776961
                    // (0xFF0000FF) for its direction arrow.
                    const rw::RGBA lColour(0, 255, 0, 255);
                    lpRender->DrawSphere(lPosition, 5.0f, lColour);
                    lpRender->DrawArrow(
                        lPosition,
                        AddScaledDirection(lPosition, lpLocation->mDirection, 5.0f),
                        rw::RGBA(255, 0, 0, 255));
                }
            }
        }

        if (mbRenderStartPositions)
        {
            const s32 liRegionCount = mpTriggerData->GetRegionCount();
            for (s32 liRegionIndex = 0; liRegionIndex < liRegionCount; ++liRegionIndex)
            {
                if (liRegionIndex >= mpTriggerData->GetRegionCount())
                {
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert(
                        "liRegionIndex < miRegionCount",
                        "..\\..\\..\\SharedClasses\\Trigger/BrnTriggerData.h",
                        624);
                    CgsDev::Assert::EndAssert();
                }

                const BrnTrigger::TriggerRegion* lpRegion =
                    mpTriggerData->GetRegionUnchecked(liRegionIndex);
                const BrnTrigger::BoxRegion* lpBox = lpRegion->GetBoxRegion();
                if (!(DistanceSquared(lpBox->GetPosition(), lpRender->GetCameraPosition()) < lfClipDistanceSquared))
                    continue;
                if (lpRegion->GetType() != BrnTrigger::TriggerRegion::E_TYPE_LANDMARK)
                    continue;

                const BrnTrigger::Landmark* lpLandmark =
                    static_cast<const BrnTrigger::Landmark*>(lpRegion);
                for (s32 liStartingGrid = 0;
                     liStartingGrid < lpLandmark->GetStartingGridCount();
                     ++liStartingGrid)
                {
                    const BrnTrigger::StartingGrid* lpStartingGrid =
                        lpLandmark->GetStartingGrid(liStartingGrid);
                    for (s32 liStartPosition = 0;
                         liStartPosition < BrnTrigger::StartingGrid::KI_STARTING_POINT_COUNT;
                         ++liStartPosition)
                    {
                        const rw::math::vpu::Vector3 lPosition =
                            lpStartingGrid->GetPosition(liStartPosition);
                        const rw::math::vpu::Vector3 lDirection =
                            lpStartingGrid->GetDirection(liStartPosition);
                        lpRender->DrawArrow(
                            lPosition,
                            AddScaledDirection(lPosition, lDirection, 5.0f),
                            rw::RGBA(255, 255, 255, 255));
                    }
                }
            }
        }
    }
}
