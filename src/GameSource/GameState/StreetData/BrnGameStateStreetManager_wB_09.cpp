// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_09.cpp
//   (wave B partfile -- group 10 "walk-core":
//       WalkAISection / AreRoadsAFollowOnPair / FindUpcomingStreetsByRecursiveWalking
//    + the CompareSectionWalkData qsort comparator)
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::CompareSectionWalkData                          @ 0x82317268
//   BrnGameState::StreetManager::AreRoadsAFollowOnPair            @ 0x82317290
//   BrnGameState::StreetManager::WalkAISection                    @ 0x8233EAE8
//   BrnGameState::StreetManager::FindUpcomingStreetsByRecursiveWalking @ 0x8234E8C8
//
// Every asm store / branch / early-out has a counterpart here; all state is the
// frozen-header named members (BrnGameStateStreetManager.h), never raw-offset
// casts. The three RC-interface accessors sub_82310398 / sub_823102F0 /
// sub_82310240 are the committed RCEntityActiveRaceCarOutputInterface
// GetPlayerDirection / GetPlayerPosition / GetPlayerRaceCarState (address order
// == the DWARF :393/:396/:399 declaration order; the dotted vector is the
// direction, the reference-position vector is the player position).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "SharedClasses/AI/AISectionsResourceType.h"     // BrnAI::AISection / AISectionsData / BrnAI::KI_AI_SECTION_EDGES
#include "GameSource/World/AI/BrnAIPortal.h"             // BrnAI::Portal (GetPositionX/Y/Z, GetLinkSectionIndex)
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RC interface + RaceCarState (mTransform)
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h"  // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Core/CgsAssert.h"       // CGS_ASSERT

#include <cstdlib>   // qsort
#include <cmath>     // asin / sqrtf / fabsf

// The scalar (fpu) vector-similarity helper the X360 walk calls out-of-line
// (rw::math::fpu::IsSimilar<float> @ the EATech reconstruction; the frozen
// RwMathVectorTemplates.h declares the sibling IsValid<T> but this helper's
// body lands with the rwmath follow-on). Declare-only call, matching the asm bl.
namespace rw { namespace math { namespace fpu {
    template <typename Type>
    bool IsSimilar( const Vector3Template<Type>& lrA,
                    const Vector3Template<Type>& lrB,
                    Type lEpsilon );
} } }

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82317268. qsort comparator over SectionWalkData: orders the collected
    // portals by ascending absolute entry angle (DecFIGS: two rw::math::fpu::
    // Abs<float> reads of mfEntryAngle, deoptimised here to fabsf).
    // -----------------------------------------------------------------------
    int32_t CompareSectionWalkData( const void* lpData1, const void* lpData2 )
    {
        const SectionWalkData* lpWalkData1 = static_cast<const SectionWalkData*>( lpData1 );
        const SectionWalkData* lpWalkData2 = static_cast<const SectionWalkData*>( lpData2 );

        const f32 lfAbsAngle1 = fabsf( lpWalkData1->mfEntryAngle );
        const f32 lfAbsAngle2 = fabsf( lpWalkData2->mfEntryAngle );

        if ( lfAbsAngle1 < lfAbsAngle2 )
            return -1;
        if ( lfAbsAngle1 > lfAbsAngle2 )
            return 1;
        return 0;
    }

    // -----------------------------------------------------------------------
    // @ 0x82317290. Unordered scan of KAA_FOLLOW_ON_ROAD_PAIRS: true iff the two
    // road indices (distinct) form one of the hard-coded follow-on pairs.
    // -----------------------------------------------------------------------
    bool StreetManager::AreRoadsAFollowOnPair( BrnStreetData::RoadIndex liRoadIndex1,
                                               BrnStreetData::RoadIndex liRoadIndex2 )
    {
        if ( liRoadIndex1 == liRoadIndex2 )
            return false;

        for ( s32 liHardCodedListIndex = 0; liHardCodedListIndex < KI_NUM_PAIRED_ROADS; ++liHardCodedListIndex )
        {
            const BrnStreetData::RoadIndex liFirst  = KAA_FOLLOW_ON_ROAD_PAIRS[liHardCodedListIndex][0];
            const BrnStreetData::RoadIndex liSecond = KAA_FOLLOW_ON_ROAD_PAIRS[liHardCodedListIndex][1];

            if ( liFirst == liRoadIndex1 && liSecond == liRoadIndex2 )
                return true;
            if ( liFirst == liRoadIndex2 && liSecond == liRoadIndex1 )
                return true;
        }
        return false;
    }

    // -----------------------------------------------------------------------
    // @ 0x8233EAE8. Recursive, breadth-limited AI-section portal walk. Collects
    // the section's onward portals into a stack SectionWalkData array, records a
    // left / right / forward road candidate when a real road (not a follow-on of
    // the current road) is reached, then recurses through the portals (sorted by
    // absolute entry angle) up to KI_MAX_SECTION_WALK_DEPTH deep.
    // -----------------------------------------------------------------------
    void StreetManager::WalkAISection( const rw::math::fpu::Vector3Template<f32>& lrEntryPos,
                                       const rw::math::fpu::Vector3Template<f32>& lrEntryDir,
                                       const BrnAI::AISection* lpSection, u16 luSectionIndex,
                                       const rw::math::fpu::Vector3Template<f32>& lrPortalPos,
                                       const rw::math::fpu::Vector3Template<f32>& lrExitRight,
                                       ESectionEntryDirection leEntryDirection,
                                       s32 liDepth, s32 liDirectionPersistance, bool lbFirst )
    {
        // Both onward road candidates already resolved -- nothing more to do.
        if ( miNewestLeftPlayerRoadIndex != -1 && miNewestRightPlayerRoadIndex != -1 )
            return;

        PushSectionIndex( luSectionIndex );

        BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luSectionIndex );
        if ( AreRoadsAFollowOnPair( liRoadIndex, miCurrentPlayerRoadIndex ) )
            return;

        // Interstate-exit section: fold this section's road into the interstate
        // slot, stamp the first junction position, and mark the road sentinel -2.
        if ( lpSection->mx8Flags & 0x80 )
        {
            bool lbCurrentRoadIsInterState = false;
            if ( miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_1
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_2
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_3
              || miCurrentPlayerRoadIndex == KI_INTERSTATE_SECTION_4 )
                lbCurrentRoadIsInterState = true;

            if ( lbCurrentRoadIsInterState )
            {
                const bool lbJunctionAlreadySet = mbJunctionPosSet;
                miOriginalInterStateRoadIndex = liRoadIndex;
                liRoadIndex = -2;
                if ( !lbJunctionAlreadySet )
                {
                    mFirstJunctionPosition.Set( lrPortalPos.X(), lrPortalPos.Y(), lrPortalPos.Z() );
                    mbJunctionPosSet = true;
                }
            }
        }

        const bool lbRoadIsNewToPlayer = ( miCurrentPlayerRoadIndex != liRoadIndex );
        const bool lbSectionHasRoad    = ( liRoadIndex != -1 );

        if ( lbFirst )
        {
            if ( miCurrentPlayerRoadIndex == liRoadIndex )
                return;
        }

        if ( miCurrentPlayerRoadIndex != -1 && !lbSectionHasRoad && !mbJunctionPosSet )
        {
            mFirstJunctionPosition.Set( lrPortalPos.X(), lrPortalPos.Y(), lrPortalPos.Z() );
            mbJunctionPosSet = true;
        }

        // Record the onward road candidate (unless there's no real road or it's
        // still the player's current road -- both fall through to the walk).
        if ( lbSectionHasRoad && lbRoadIsNewToPlayer )
        {
            switch ( leEntryDirection )
            {
                case E_SECTION_ENTRY_DIRECTION_LEFT:
                case E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT:
                    if ( miNewestLeftPlayerRoadIndex == -1 && !mbPlayerOnInterstateExit )
                    {
                        miNewestLeftPlayerRoadIndex = liRoadIndex;
                        return;
                    }
                    break;
                case E_SECTION_ENTRY_DIRECTION_FORWARDS:
                    break;
                case E_SECTION_ENTRY_DIRECTION_RIGHT:
                case E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT:
                    if ( miNewestRightPlayerRoadIndex == -1 && !mbPlayerOnInterstateExit )
                    {
                        miNewestRightPlayerRoadIndex = liRoadIndex;
                        return;
                    }
                    break;
                default:
                    CGS_ASSERT( false, "How did it get here?" );
                    break;
            }
        }

        // ---- walk the section's portals -----------------------------------
        if ( miNextFreeSectionSlot >= KI_MAX_SECTIONS_TO_WALK || liDepth >= KI_MAX_SECTION_WALK_DEPTH )
            return;
        if ( lpSection->mu8NumPortals == 0 )
            return;

        SectionWalkData laSectionWalkData[KI_MAX_SECTIONS_TO_WALK];
        s32 liNumPortalsToExplore = 0;

        for ( s32 liPortalIndex = 0; liPortalIndex < lpSection->mu8NumPortals; ++liPortalIndex )
        {
            const BrnAI::Portal* lpPortal = lpSection->GetPortal( static_cast<u8>( liPortalIndex ) );
            const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();
            CGS_ASSERT( luLinkSectionIndex != 0x7FFF, "luLinkSectionIndex != BrnWorld::KI_INVALID_SECTION_INDEX" );

            // HasSectionAlreadyBeenWalked (inlined scan of mauWalkedSections).
            bool lbAlreadyWalked = false;
            if ( miNextFreeSectionSlot > 0 )
            {
                for ( s32 liWalked = 0; liWalked < miNextFreeSectionSlot; ++liWalked )
                {
                    if ( luLinkSectionIndex == mauWalkedSections[liWalked] )
                    {
                        lbAlreadyWalked = true;
                        break;
                    }
                }
            }
            if ( lbAlreadyWalked )
                continue;

            const BrnAI::AISection* lpLinkSection = mpAISectionData.operator->()->GetAISection( luLinkSectionIndex );
            const u8 lu8LinkFlags = lpLinkSection->mx8Flags;
            if ( ( lu8LinkFlags & 1 ) || ( lu8LinkFlags & 0x40 ) || ( lu8LinkFlags & 4 ) || ( lu8LinkFlags & 0x20 ) )
                continue;

            const f32 lfPortalX = lpPortal->GetPositionX();
            const f32 lfPortalY = lpPortal->GetPositionY();
            const f32 lfPortalZ = lpPortal->GetPositionZ();

            rw::math::fpu::Vector3Template<f32> lPortalPosition;
            lPortalPosition.Set( lfPortalX, lfPortalY, lfPortalZ );

            const f32 lfToPortalY = lrEntryDir.Y() - lfPortalY;
            const f32 lfToPortalX = lrEntryDir.X() - lfPortalX;
            const f32 lfToPortalZ = lrEntryDir.Z() - lfPortalZ;
            const f32 lfDistanceToPortal = sqrtf( lfToPortalY * lfToPortalY
                                                + lfToPortalX * lfToPortalX
                                                + lfToPortalZ * lfToPortalZ );

            if ( lfDistanceToPortal > KF_MAX_ROAD_LOOKAHEAD_DIST )
                continue;
            if ( rw::math::fpu::IsSimilar<f32>( lPortalPosition, lrPortalPos, 0.0000152587890625f ) )
                continue;

            // Build the walk record: portal position + normalised entry direction.
            SectionWalkData& lRecord = laSectionWalkData[liNumPortalsToExplore];
            lRecord.mEntryPos.Set( lfPortalX, lfPortalY, lfPortalZ );

            const f32 lfEntryDirX = lfPortalX - lrPortalPos.X();
            const f32 lfEntryDirY = lfPortalY - lrPortalPos.Y();
            const f32 lfEntryDirZ = lfPortalZ - lrPortalPos.Z();
            const f32 lfEntryInvLen = 1.0f / sqrtf( lfEntryDirX * lfEntryDirX
                                                  + lfEntryDirY * lfEntryDirY
                                                  + lfEntryDirZ * lfEntryDirZ );
            const f32 lfNormEntryX = lfEntryDirX * lfEntryInvLen;
            const f32 lfNormEntryY = lfEntryDirY * lfEntryInvLen;
            const f32 lfNormEntryZ = lfEntryDirZ * lfEntryInvLen;
            lRecord.mEntryDir.Set( lfNormEntryX, lfNormEntryY, lfNormEntryZ );

            // Accept only portals ahead of the section-entry vector.
            const f32 lfAheadDot = lrEntryPos.Y() * ( lfPortalY - lrEntryDir.Y() )
                                 + lrEntryPos.Z() * ( lfPortalZ - lrEntryDir.Z() )
                                 + lrEntryPos.X() * ( lfPortalX - lrEntryDir.X() );
            if ( lfAheadDot < 0.0f )
                continue;

            lRecord.mpLinkPortal   = lpPortal;
            lRecord.muSectionIndex = luLinkSectionIndex;
            lRecord.mpLinkSection  = lpLinkSection;

            // Signed entry angle = asin( sign(crossY) * |entryDir x exitRight| ).
            const f32 lfCrossY = lfNormEntryZ * lrExitRight.X() - lfNormEntryX * lrExitRight.Z();
            const f32 lfCrossZ = lfNormEntryX * lrExitRight.Y() - lfNormEntryY * lrExitRight.X();
            const f32 lfCrossX = lfNormEntryY * lrExitRight.Z() - lfNormEntryZ * lrExitRight.Y();
            const f32 lfCrossMag = sqrtf( lfCrossY * lfCrossY + lfCrossZ * lfCrossZ + lfCrossX * lfCrossX );

            f32 lfSign;
            if ( lfCrossY == 0.0f )
                lfSign = 0.0f;
            else
                lfSign = ( lfCrossY >= 0.0f ) ? 1.0f : -1.0f;
            lRecord.mfEntryAngle = static_cast<f32>( asin( static_cast<double>( lfSign * lfCrossMag ) ) );

            ++liNumPortalsToExplore;
        }

        if ( liNumPortalsToExplore == 0 )
            return;

        // Two-portal straight-through: the only onward portal -- recurse without
        // the qsort, deepening the direction persistance.
        if ( lpSection->mu8NumPortals == 2 && liNumPortalsToExplore == 1 )
        {
            ESectionEntryDirection leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
            if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                leNextDirection = leEntryDirection;

            WalkAISection( lrEntryPos, lrEntryDir,
                           laSectionWalkData[0].mpLinkSection, laSectionWalkData[0].muSectionIndex,
                           laSectionWalkData[0].mEntryPos, laSectionWalkData[0].mEntryDir,
                           leNextDirection, liDepth + 1, liDirectionPersistance + 1, lbRoadIsNewToPlayer );
            return;
        }

        CGS_ASSERT( liNumPortalsToExplore > 0, "liNumPortalsToExplore > 0" );

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsQSortPM );
        qsort( laSectionWalkData, static_cast<size_t>( liNumPortalsToExplore ),
               sizeof( SectionWalkData ), CompareSectionWalkData );
        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsQSortPM );

        for ( s32 liExplore = 0; liExplore < liNumPortalsToExplore; ++liExplore )
        {
            SectionWalkData& lRecord = laSectionWalkData[liExplore];

            bool lbSkipPortal = false;
            ESectionEntryDirection leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;

            if ( lbRoadIsNewToPlayer )
            {
                const f32 lfEntryAngle = lRecord.mfEntryAngle;
                if ( lfEntryAngle < -KF_UPCOMING_ROAD_SIDEWAYS_ANGLE )
                {
                    if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT )
                        lbSkipPortal = true;
                    else if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT
                           && liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT;
                    else if ( lfEntryAngle < -KF_UPCOMING_ROAD_SIGNIFICANT_ANGLE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_LEFT;
                }
                else if ( lfEntryAngle > KF_UPCOMING_ROAD_SIDEWAYS_ANGLE )
                {
                    if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_LEFT )
                        lbSkipPortal = true;
                    else if ( leEntryDirection == E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT
                           && liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT;
                    else if ( lfEntryAngle > KF_UPCOMING_ROAD_SIGNIFICANT_ANGLE )
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_SIGNIFICANT_RIGHT;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_RIGHT;
                }
                else
                {
                    if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                        leNextDirection = leEntryDirection;
                    else
                        leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
                }
            }
            else
            {
                if ( liDirectionPersistance < KI_MAX_PORTAL_WALK_DIRECTION_PERSISTANCE )
                    leNextDirection = leEntryDirection;
                else
                    leNextDirection = E_SECTION_ENTRY_DIRECTION_FORWARDS;
            }

            if ( lbSkipPortal )
                continue;

            WalkAISection( lrEntryPos, lrEntryDir,
                           lRecord.mpLinkSection, lRecord.muSectionIndex,
                           lRecord.mEntryPos, lRecord.mEntryDir,
                           leNextDirection, liDepth + 1, 0, lbRoadIsNewToPlayer );

            if ( !CanContinueWalking() )
                break;
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x8234E8C8. Seed the recursive walk from the player's current section:
    // pick the most-forward onward portal, derive the section exit-right vector
    // (edge best aligned toward that portal), WalkAISection through it, then
    // apply the hopeful-road stickiness and (in-race) SendUpcomingRoadMessage.
    // -----------------------------------------------------------------------
    void StreetManager::FindUpcomingStreetsByRecursiveWalking(
        BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
        GameStateModuleIO::OutputBuffer* lpOutput,
        BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
        f32 lfSimTimeStep,
        u16 luSectionIndex,
        bool lbInRace )
    {
        const BrnAI::AISection* lpSection = mpAISectionData.operator->()->GetAISection( luSectionIndex );

        const Vector3 lPlayerDirection = lpActiveRaceCarInterface->GetPlayerDirection();
        const f32 lfPlayerDirX = lPlayerDirection.x;
        const f32 lfPlayerDirY = lPlayerDirection.y;
        const f32 lfPlayerDirZ = lPlayerDirection.z;

        const Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
        const f32 lfPlayerPosX = lPlayerPosition.x;
        const f32 lfPlayerPosY = lPlayerPosition.y;
        const f32 lfPlayerPosZ = lPlayerPosition.z;

        u16 luBestPortalSectionIndex = 0x7FFF;
        f32 lfBestPortalDot = 0.0f;
        f32 lfBestPortalX = 0.0f;
        f32 lfBestPortalY = 0.0f;
        f32 lfBestPortalZ = 0.0f;

        miNewestForwardPlayerRoadIndex = -1;
        miNewestLeftPlayerRoadIndex = -1;
        miNextFreeSectionSlot = 0;
        miNewestRightPlayerRoadIndex = -1;

        PushSectionIndex( luSectionIndex );

        if ( lpSection->mu8NumPortals != 0 )
        {
            for ( s32 liPortalIndex = 0; liPortalIndex < lpSection->mu8NumPortals; ++liPortalIndex )
            {
                const BrnAI::Portal* lpPortal = lpSection->GetPortal( static_cast<u8>( liPortalIndex ) );
                const u16 luLinkSectionIndex = lpPortal->GetLinkSectionIndex();

                const BrnAI::AISectionsData* lpSectionsData = mpAISectionData.operator->();
                CGS_ASSERT( luLinkSectionIndex < lpSectionsData->muNumSections, "luSectionIndex < muNumSections" );

                const u8 lu8LinkFlags = lpSectionsData->mpaSections[luLinkSectionIndex].mx8Flags;
                bool lbSkipPortal = false;
                if ( ( lu8LinkFlags & 1 ) || ( lu8LinkFlags & 0x40 ) || ( lu8LinkFlags & 4 ) || ( lu8LinkFlags & 0x20 ) )
                    lbSkipPortal = true;

                if ( !lbSkipPortal )
                {
                    const f32 lfPortalX = lpPortal->GetPositionX();
                    const f32 lfPortalY = lpPortal->GetPositionY();
                    const f32 lfPortalZ = lpPortal->GetPositionZ();

                    const f32 lfDeltaX = lfPortalX - lfPlayerPosX;
                    const f32 lfDeltaY = lfPortalY - lfPlayerPosY;
                    const f32 lfDeltaZ = lfPortalZ - lfPlayerPosZ;
                    const f32 lfInvLen = 1.0f / sqrtf( lfDeltaY * lfDeltaY
                                                     + lfDeltaX * lfDeltaX
                                                     + lfDeltaZ * lfDeltaZ );
                    const f32 lfDot = lfDeltaX * lfInvLen * lfPlayerDirX
                                    + lfDeltaY * lfInvLen * lfPlayerDirY
                                    + lfDeltaZ * lfInvLen * lfPlayerDirZ;

                    if ( lfDot > lfBestPortalDot )
                    {
                        luBestPortalSectionIndex = lpPortal->GetLinkSectionIndex();
                        lfBestPortalDot = lfDot;
                        lfBestPortalX = lfPortalX;
                        lfBestPortalY = lfPortalY;
                        lfBestPortalZ = lfPortalZ;
                    }
                }
            }
        }

        if ( luBestPortalSectionIndex != 0x7FFF )
        {
            const BrnAI::AISectionsData* lpSectionsData = mpAISectionData.operator->();
            CGS_ASSERT( luBestPortalSectionIndex < lpSectionsData->muNumSections, "luSectionIndex < muNumSections" );
            const BrnAI::AISection* lpBestSection = &lpSectionsData->mpaSections[luBestPortalSectionIndex];

            // Find the section edge best aligned toward the chosen portal.
            f32 lfBestEdgeDot = 0.0f;
            f32 lfEdgeStartX = 0.0f;
            f32 lfEdgeStartY = 0.0f;
            f32 lfEdgeEndX = 0.0f;
            f32 lfEdgeEndY = 0.0f;

            for ( s32 liCornerIndex = 0; liCornerIndex < BrnAI::KI_AI_SECTION_EDGES; ++liCornerIndex )
            {
                CGS_ASSERT( liCornerIndex >= 0 && liCornerIndex < BrnAI::KI_AI_SECTION_EDGES,
                            "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES" );
                const s32 liNextCorner = ( liCornerIndex + 1 ) % BrnAI::KI_AI_SECTION_EDGES;
                CGS_ASSERT( liNextCorner >= 0 && liNextCorner < BrnAI::KI_AI_SECTION_EDGES,
                            "liCornerIndex >= 0 && liCornerIndex < KI_AI_SECTION_EDGES" );

                const f32 lfCornerX = lpBestSection->mpaCorners[liCornerIndex].x;
                const f32 lfCornerY = lpBestSection->mpaCorners[liCornerIndex].y;
                const f32 lfNextCornerX = lpBestSection->mpaCorners[liNextCorner].x;
                const f32 lfNextCornerY = lpBestSection->mpaCorners[liNextCorner].y;

                const f32 lfInvToPortal = 1.0f / sqrtf( ( lfBestPortalZ - lfCornerY ) * ( lfBestPortalZ - lfCornerY )
                                                      + ( lfBestPortalX - lfCornerX ) * ( lfBestPortalX - lfCornerX ) );
                const f32 lfInvEdgeLen  = 1.0f / sqrtf( ( lfNextCornerX - lfCornerX ) * ( lfNextCornerX - lfCornerX )
                                                      + ( lfNextCornerY - lfCornerY ) * ( lfNextCornerY - lfCornerY ) );
                const f32 lfEdgeDot = ( lfBestPortalZ - lfCornerY ) * lfInvToPortal * ( ( lfNextCornerY - lfCornerY ) * lfInvEdgeLen )
                                    + ( lfBestPortalX - lfCornerX ) * lfInvToPortal * ( ( lfNextCornerX - lfCornerX ) * lfInvEdgeLen );

                if ( lfEdgeDot > lfBestEdgeDot )
                {
                    lfBestEdgeDot = lfEdgeDot;
                    lfEdgeStartX = lfCornerX;
                    lfEdgeStartY = lfCornerY;
                    lfEdgeEndX = lfNextCornerX;
                    lfEdgeEndY = lfNextCornerY;
                }
            }

            // The normalised edge direction, and its horizontal perpendicular.
            const f32 lfEdgeInvLen = 1.0f / sqrtf( ( lfEdgeStartY - lfEdgeEndY ) * ( lfEdgeStartY - lfEdgeEndY )
                                                 + ( lfEdgeStartX - lfEdgeEndX ) * ( lfEdgeStartX - lfEdgeEndX ) );
            const f32 lfEdgeDirZ = ( lfEdgeStartY - lfEdgeEndY ) * lfEdgeInvLen;
            const f32 lfEdgeDirX = ( lfEdgeStartX - lfEdgeEndX ) * lfEdgeInvLen;

            const f32 lfPerpInvLen = 1.0f / sqrtf( ( -lfEdgeDirZ ) * ( -lfEdgeDirZ ) + ( lfEdgeDirX * lfEdgeDirX ) );
            f32 lfEntryDirX = lfPerpInvLen * -lfEdgeDirZ;
            f32 lfEntryDirY = lfPerpInvLen * 0.0f;
            f32 lfEntryDirZ = lfPerpInvLen * lfEdgeDirX;

            if ( ( ( lfBestPortalX - lfPlayerPosX ) * lfEntryDirX )
               + ( ( lfBestPortalZ - lfPlayerPosZ ) * lfEntryDirZ )
               + ( ( lfBestPortalY - lfPlayerPosY ) * lfEntryDirY ) < 0.0f )
            {
                lfEntryDirX = ( lfPerpInvLen * -lfEdgeDirZ ) * -1.0f;
                lfEntryDirY = ( lfPerpInvLen * 0.0f ) * -1.0f;
                lfEntryDirZ = ( lfPerpInvLen * lfEdgeDirX ) * -1.0f;
            }

            // The section exit-right member = the normalised edge direction, sign
            // aligned against the player car's right axis (transform x-axis).
            mCurrentPlayerSectionExitRight.Set( lfEdgeDirX, 0.0f, lfEdgeDirZ );
            const f32 lfExitRightInvLen = 1.0f / sqrtf( lfEdgeDirZ * lfEdgeDirZ + lfEdgeDirX * lfEdgeDirX + 0.0f * 0.0f );
            mCurrentPlayerSectionExitRight.Set( lfExitRightInvLen * lfEdgeDirX,
                                                lfExitRightInvLen * 0.0f,
                                                lfExitRightInvLen * lfEdgeDirZ );

            const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState = lpActiveRaceCarInterface->GetPlayerRaceCarState();
            if ( ( lpPlayerRaceCarState->mTransform.xAxis.x * mCurrentPlayerSectionExitRight.X() )
               + ( mCurrentPlayerSectionExitRight.Z() * lpPlayerRaceCarState->mTransform.xAxis.z )
               + ( mCurrentPlayerSectionExitRight.Y() * lpPlayerRaceCarState->mTransform.xAxis.y ) < 0.0f )
            {
                mCurrentPlayerSectionExitRight.Set( mCurrentPlayerSectionExitRight.X() * -1.0f,
                                                    mCurrentPlayerSectionExitRight.Y() * -1.0f,
                                                    mCurrentPlayerSectionExitRight.Z() * -1.0f );
            }

            mbJunctionPosSet = false;
            mFirstJunctionPosition.Set( lfPlayerPosX, lfPlayerPosY, lfPlayerPosZ );

            rw::math::fpu::Vector3Template<f32> lSectionEntryDir;
            lSectionEntryDir.Set( lfEntryDirX, lfEntryDirY, lfEntryDirZ );
            rw::math::fpu::Vector3Template<f32> lPlayerPositionVec;
            lPlayerPositionVec.Set( lfPlayerPosX, lfPlayerPosY, lfPlayerPosZ );
            rw::math::fpu::Vector3Template<f32> lBestPortalPositionVec;
            lBestPortalPositionVec.Set( lfBestPortalX, lfBestPortalY, lfBestPortalZ );

            WalkAISection( lSectionEntryDir, lPlayerPositionVec,
                           lpBestSection, luBestPortalSectionIndex,
                           lBestPortalPositionVec, lSectionEntryDir,
                           E_SECTION_ENTRY_DIRECTION_FORWARDS, 0, 0, false );

            // Hopeful-road stickiness: only accept the new candidates once they
            // hold steady past the sticky-time threshold.
            const BrnStreetData::RoadIndex liNewestForward = miNewestForwardPlayerRoadIndex;
            if ( liNewestForward == miHopefulForwardPlayerRoadIndex
              && miNewestLeftPlayerRoadIndex == miHopefulLeftPlayerRoadIndex
              && miNewestRightPlayerRoadIndex == miHopefulRightPlayerRoadIndex )
            {
                mfHopefulRoadTimer += lfSimTimeStep;
            }
            else
            {
                miHopefulForwardPlayerRoadIndex = liNewestForward;
                mfHopefulRoadTimer = 0.0f;
                mbNeedToUpdateUpcomingRoads = true;
                miHopefulLeftPlayerRoadIndex = miNewestLeftPlayerRoadIndex;
                miHopefulRightPlayerRoadIndex = miNewestRightPlayerRoadIndex;
            }

            const f32 lfStickyThreshold = lbInRace ? KF_UPCOMING_ROADS_STICKY_TIME_RACE
                                                   : KF_UPCOMING_ROADS_STICKY_TIME;
            if ( mfHopefulRoadTimer > lfStickyThreshold )
            {
                if ( mbNeedToUpdateUpcomingRoads )
                {
                    const BrnStreetData::RoadIndex liHopefulLeft = miHopefulLeftPlayerRoadIndex;
                    mLeftRoadIndex = -1;
                    mRightRoadIndex = -1;
                    mbRightRoadHighlighted = false;
                    mbLeftRoadHighlighted = false;
                    miOriginalInterStateRoadIndex = -1;
                    if ( liHopefulLeft != -1 )
                        mLeftRoadIndex = liHopefulLeft;
                    const BrnStreetData::RoadIndex liHopefulRight = miHopefulRightPlayerRoadIndex;
                    if ( liHopefulRight != -1 )
                        mRightRoadIndex = liHopefulRight;

                    if ( lbInRace )
                        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );

                    mbNeedToUpdateUpcomingRoads = false;
                }
                else
                {
                    if ( lbInRace )
                        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );
                }
            }
        }
    }
}
