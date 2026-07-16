// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wB_10.cpp
//   (wave B partfile -- group 10 "upcoming-pipeline")
//
// Faithful de-optimisation of the X360 BURNOUT_X360_ARTIST.XEX:
//   BrnGameState::StreetManager::UpdateUpcomingStreets        @ 0x82350A88
//   BrnGameState::StreetManager::FindUpcomingStreetsFromRoute @ 0x8234EF90
//   BrnGameState::StreetManager::CalculateDistanceToTurning   @ 0x823172F0
// (SendUpcomingRoadMessage @ 0x82348798 is BLOCKED -- see the structured
//  report: its 368-byte GUI message payload has no committed type and its
//  leading vtable/type-descriptor pointer (unk_82F30000) is an un-named
//  un-exported global, so a faithful store-for-store build would fabricate a
//  layout + symbol. It is only CALLED here, through the frozen-header decl.)
//
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (never raw-offset casts). All magic float
// immediates are the values attested in the image rodata.
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"
#include "GameSource/GameState/StreetData/BrnStreetManagerDebugComponent.h"
#include "GameSource/GameState/BrnGameEvents.h"
#include "GameSource/GameState/BrnGameActions.h"
#include "GameSource/GameState/Progression/BrnProgressionManager.h"
#include "GameSource/GameState/BrnGameStateModule.h"
#include "GameShared/GameClasses/RenderWare/Math/RwMathVectorTemplates.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"                 // OutputBuffer, GetGuiOutputQueue (13312 game-action queue)
#include "GameSource/World/AI/SharedIO/BrnAICarOutputInterface.h"     // AICarOutputInterface (player route + node index)
#include "GameSource/World/AI/Route/BrnRoute.h"                       // BrnAI::Route / RouteNode
#include "GameSource/World/EntityModules/RaceCarEntityModule/SharedIO/BrnRaceCarEntityModuleOutputInterface.h" // RCEntityActiveRaceCarOutputInterface / RaceCarState
#include "SharedClasses/AI/AISectionsResourceType.h"                  // BrnAI::AISectionsData / AISection
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"      // CgsModule::Event / VariableEventQueue::AddEvent
#include "GameShared/GameClasses/Development/CgsStrStream.h"          // CgsDev::StrStream (dev assert message)
#include "GameShared/GameClasses/Core/CgsAssert.h"                    // CGS_ASSERT + KI_MESSAGEBUFFERSIZE
#include "BrnCommonTypes.h"                                           // Vector3 (rw::math::vpu, POD lanes)

#include <cmath>   // sqrtf

// ---------------------------------------------------------------------------
// Forward declarations for the two rw::math::fpu scalar-vector helpers the
// route walk calls in its dev asserts. The frozen RwMathVectorTemplates.h homes
// only IsValid<>; the DecFIGS DWARF spells IsSimilar<>/IsZero<> in the same
// namespace and their bodies land with that TU (spec: "declare-only calls fine").
// ---------------------------------------------------------------------------
namespace rw { namespace math { namespace fpu {
    template <typename Type>
    bool IsSimilar( const Vector3Template<Type>& lrA, const Vector3Template<Type>& lrB, Type lfTolerance );
    template <typename Type>
    bool IsZero( const Vector3Template<Type>& lrVector, Type lfTolerance );
} } }

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x823172F0. Planar (y-dropped) distance from the player to the current
    // route's turn node (this->miNewRoadNodeIndex - 2). Returns 0.0f (flt_82001CC0)
    // on the no-route / empty-route early-outs.
    // -----------------------------------------------------------------------
    f32 StreetManager::CalculateDistanceToTurning(
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface )
    {
        CGS_ASSERT( lpLastAICarOutputInterface != NULL, "lpLastAICarOutputInterface != NULL" );

        const s32 liPlayerRoadTargetNodeIndex = miNewRoadNodeIndex - 2;
        if ( liPlayerRoadTargetNodeIndex >= 0 )
        {
            // GetPlayerRoute() folds to the interface pointer itself (mPlayerRoute @ +0);
            // view it as the now-committed BrnAI::Route (same address the X360 asserts twice).
            const BrnAI::Route* lpPlayerRoute =
                reinterpret_cast<const BrnAI::Route*>( lpLastAICarOutputInterface->GetPlayerRoute() );
            CGS_ASSERT( lpPlayerRoute != NULL, "lpPlayerRoute != NULL" );

            if ( lpPlayerRoute->GetNodeCount() >= 1 )
            {
                const BrnAI::RouteNode* lpPlayerRoadTargetRouteNode =
                    lpPlayerRoute->GetNode( liPlayerRoadTargetNodeIndex );
                CGS_ASSERT( lpPlayerRoadTargetRouteNode != NULL, "lpPlayerRoadTargetRouteNode != NULL" );

                // Node position onto the ground plane: (x, 0, y).
                const f32 lfTargetNodeX = lpPlayerRoadTargetRouteNode->GetX();
                const f32 lfTargetNodeZ = lpPlayerRoadTargetRouteNode->GetY();

                // Player position with its vertical lane dropped (v15[1] = 0).
                Vector3 lPlayerPosition = lpActiveRaceCarInterface->GetPlayerPosition();
                lPlayerPosition.y = 0.0f;

                const f32 lfDeltaX = lPlayerPosition.x - lfTargetNodeX;
                const f32 lfDeltaY = lPlayerPosition.y - 0.0f;
                const f32 lfDeltaZ = lPlayerPosition.z - lfTargetNodeZ;
                const f32 lfDistanceSquared = ( lfDeltaX * lfDeltaX )
                                            + ( lfDeltaY * lfDeltaY )
                                            + ( lfDeltaZ * lfDeltaZ );
                // vsel zero-guard: a zero squared-length selects 0 rather than 0 * (1/0).
                const f32 lfDistanceToTurning = ( lfDistanceSquared == 0.0f ) ? 0.0f : sqrtf( lfDistanceSquared );
                return lfDistanceToTurning;
            }
        }

        return 0.0f;   // flt_82001CC0
    }

    // -----------------------------------------------------------------------
    // @ 0x82350A88. Per-frame upcoming-streets pump: resolve the player's AI
    // section -> road index, junction-timeout bookkeeping, then the recursive /
    // route walks and the shortcut-flag game action (type 287).
    // (liReserved is carried by the X360 arity but the body never reads it.)
    // -----------------------------------------------------------------------
    void StreetManager::UpdateUpcomingStreets(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            GameStateModuleIO::OutputBuffer* lpOutput,
            f32 lfSimTimeStep,
            s32 liReserved,
            bool lbUseRoute )
    {
        (void)liReserved;

        bool lbFoundRoad = false;

        CGS_ASSERT( lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex() < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                    "mePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

        const ::EActiveRaceCarIndex lePlayerActiveRaceCarIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
        bool lbPlayerCarActive = false;
        if ( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID )
        {
            lbPlayerCarActive = lpActiveRaceCarInterface->IsPlayerCarActive();
        }

        if ( lbPlayerCarActive )
        {
            CGS_ASSERT( lePlayerActiveRaceCarIndex != ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        "Player car index hasn't been set" );
            CGS_ASSERT( lePlayerActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID,
                        "lePlayerActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID" );
            CGS_ASSERT( lePlayerActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT,
                        "lePlayerActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT" );

            const u16 luRaceCarAISection = lpActiveRaceCarInterface->GetRaceCarAISection( lePlayerActiveRaceCarIndex );
            const BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luRaceCarAISection );
            miCurrentPlayerRoadIndex = liRoadIndex;

            if ( liRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
            {
                // Junction path: invalidate the upcoming/hopeful roads and age the junction timer.
                const f32 lfNewTimeInJunction = mfTimeInJunction + lfSimTimeStep;

                miNewestForwardPlayerRoadIndex = BrnStreetData::KI_INVALID_ROAD_INDEX;
                miNewestLeftPlayerRoadIndex    = BrnStreetData::KI_INVALID_ROAD_INDEX;
                miNextFreeSectionSlot          = 0;
                miNewestRightPlayerRoadIndex   = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mLeftRoadIndex                 = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mRightRoadIndex                = BrnStreetData::KI_INVALID_ROAD_INDEX;
                mfHopefulRoadTimer             = 0.0f;
                mfTimeInJunction               = lfNewTimeInJunction;
                mbRightRoadHighlighted         = false;
                mbLeftRoadHighlighted          = false;
                mbNeedToUpdateUpcomingRoads    = true;
                miOriginalInterStateRoadIndex  = BrnStreetData::KI_INVALID_ROAD_INDEX;

                if ( lfNewTimeInJunction > KF_TIME_IN_JUNCTION_TO_KILL_UPCOMING )
                {
                    SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, false, lpActiveRaceCarInterface );
                }
            }
            else
            {
                miLastPlayerRoadIndex = liRoadIndex;
                lbFoundRoad           = true;
                mfTimeInJunction      = 0.0f;
            }

            if ( luRaceCarAISection != 0x7FFF )
            {
                if ( luRaceCarAISection >= mpAISectionData->muNumSections )
                {
                    char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                    CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                    lStrStream << "AI section index too large. Section = ";
                    lStrStream << static_cast<u32>( luRaceCarAISection );
                    lStrStream << " count = ";
                    lStrStream << mpAISectionData->muNumSections;
                    lStrStream << "\n";
                    CgsDev::Assert::BeginAssert();
                    CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                    CgsDev::Assert::EndAssert();
                }

                const BrnAI::AISection* lpAISection = mpAISectionData->GetAISection( luRaceCarAISection );

                if ( lbFoundRoad )
                {
                    mbPlayerOnInterstateExit = ( lpAISection->mx8Flags >> 7 ) != 0;
                    FindUpcomingStreetsByRecursiveWalking( lpActiveRaceCarInterface, lpOutput,
                                                           lpLastAICarOutputInterface, lfSimTimeStep,
                                                           luRaceCarAISection, !lbUseRoute );
                    if ( lbUseRoute )
                    {
                        FindUpcomingStreetsFromRoute( lpActiveRaceCarInterface, lpLastAICarOutputInterface,
                                                      lpOutput, lfSimTimeStep );
                    }
                }

                // Shortcut-flag GUI game action (type 287): toggle when the player's
                // shortcut membership changes (AISection flag bit 0).
                if ( mbPlayerIsInAShortcut )
                {
                    if ( ( lpAISection->mx8Flags & 1 ) == 0 )
                    {
                        const u8 lu8InShortcut = 0;
                        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
                        lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lu8InShortcut ), 287, 1 );
                        mbPlayerIsInAShortcut = false;
                    }
                }
                else if ( ( lpAISection->mx8Flags & 1 ) != 0 )
                {
                    const u8 lu8InShortcut = 1;
                    CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
                    lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lu8InShortcut ), 287, 1 );
                    mbPlayerIsInAShortcut = true;
                }
            }
        }
    }

    // -----------------------------------------------------------------------
    // @ 0x8234EF90. Route-guided upcoming-streets walk: step the player's route
    // nodes ahead, classify each transition left/right against the section
    // exit-right vector, handle the interstate special cases, and push the
    // hopeful road onto the left/right sign when the guidance conditions hold.
    // -----------------------------------------------------------------------
    void StreetManager::FindUpcomingStreetsFromRoute(
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            GameStateModuleIO::OutputBuffer* lpOutput,
            f32 lfSimTimeStep )
    {
        const BrnAI::Route* lpPlayerRoute =
            reinterpret_cast<const BrnAI::Route*>( lpLastAICarOutputInterface->GetPlayerRoute() );

        const BrnAI::Route::Status leStatus = lpPlayerRoute->GetStatus();
        if ( leStatus == BrnAI::Route::E_STATUS_UNINITIALISED || leStatus == BrnAI::Route::E_STATUS_BLOCKED )
        {
            return;
        }

        const BrnStreetData::RoadIndex liCurrentRoad = miCurrentPlayerRoadIndex;
        mbLeftRoadHighlighted  = false;
        mbRightRoadHighlighted = false;

        const bool lbCurrentRoadIsInterstate =
            ( liCurrentRoad == KI_INTERSTATE_SECTION_1 || liCurrentRoad == KI_INTERSTATE_SECTION_2
           || liCurrentRoad == KI_INTERSTATE_SECTION_3 || liCurrentRoad == KI_INTERSTATE_SECTION_4 );

        // Reset the sign locks unless there is a valid current road AND a known
        // upcoming left/right road: (curr == -1) || (left == -1 && right == -1).
        if ( liCurrentRoad != BrnStreetData::KI_INVALID_ROAD_INDEX
          && ( mLeftRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX
            || mRightRoadIndex != BrnStreetData::KI_INVALID_ROAD_INDEX ) )
        {
            const s32 liCurrentRouteNode = lpLastAICarOutputInterface->GetPlayerRouteNodeIndex();

            BrnStreetData::RoadIndex liLeftRoadTracker  = BrnStreetData::KI_INVALID_ROAD_INDEX;
            BrnStreetData::RoadIndex liRightRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
            s32 liLeftRunCount  = 0;
            s32 liRightRunCount = 0;

            const s32 liNodeCount = lpPlayerRoute->GetNodeCount();
            mfGuidanceLockTime = lfSimTimeStep + mfGuidanceLockTime;

            const s32 liLookaheadEnd = liCurrentRouteNode + KI_ROUTE_ROAD_PREDICTION_LOOKAHEAD_NODES;
            if ( liCurrentRouteNode < liLookaheadEnd )   // overflow guard
            {
                const s32 liLoopEnd = liNodeCount - KI_NUM_END_OF_ROUTE_IGNORE_NODES;
                s32 liNodeIndex = liCurrentRouteNode;

                while ( liNodeIndex < liLoopEnd )
                {
                    const s32 liNextNodeIndex = liNodeIndex + 1;
                    const BrnAI::RouteNode* lpNode     = lpPlayerRoute->GetNode( liNodeIndex );
                    const BrnAI::RouteNode* lpNextNode = lpPlayerRoute->GetNode( liNextNodeIndex );

                    const u16 luSectionIndex = lpNode->GetSectionIndex();
                    // The X360 makes this first (discarded) GetAISection call for its
                    // bounds-assert side effect before resolving the road index.
                    mpAISectionData->GetAISection( luSectionIndex );

                    BrnStreetData::RoadIndex liRoadIndex = GetRoadIndexFromAISectionIndex( luSectionIndex );
                    const bool lbNextRoadIsInterstate =
                        ( liRoadIndex == KI_INTERSTATE_SECTION_1 || liRoadIndex == KI_INTERSTATE_SECTION_2
                       || liRoadIndex == KI_INTERSTATE_SECTION_3 || liRoadIndex == KI_INTERSTATE_SECTION_4 );

                    if ( !lbCurrentRoadIsInterstate && lbNextRoadIsInterstate )
                    {
                        goto LABEL_DONE;
                    }

                    const bool lbSectionIsInterstateExit =
                        ( ( mpAISectionData->GetAISection( luSectionIndex )->mx8Flags >> 7 ) != 0 );
                    if ( lbCurrentRoadIsInterstate && lbSectionIsInterstateExit )
                    {
                        miOriginalInterStateRoadIndex = liRoadIndex;
                        liRoadIndex = -2;
                    }

                    const BrnStreetData::RoadIndex liCurrentRoadNow = miCurrentPlayerRoadIndex;
                    // Interstate pair 12/44 followed by road 43 terminates the walk.
                    if ( ( liCurrentRoadNow == 12 || liCurrentRoadNow == 44 ) && liRoadIndex == 43 )
                    {
                        goto LABEL_HIGHLIGHT_FROM_LOCKS;
                    }

                    const f32 lfNodeX = lpNode->GetX();
                    const f32 lfNodeY = lpNode->GetY();
                    const f32 lfNextX = lpNextNode->GetX();
                    const f32 lfNextY = lpNextNode->GetY();

                    rw::math::fpu::Vector3Template<f32> lNodePos;
                    lNodePos.Set( lfNodeX, 0.0f, lfNodeY );
                    rw::math::fpu::Vector3Template<f32> lNextNodePos;
                    lNextNodePos.Set( lfNextX, 0.0f, lfNextY );

                    if ( liRoadIndex == liCurrentRoadNow || liRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
                    {
                        // Still on the current road: refresh the section exit-right vector.
                        if ( GetRoadIndexFromAISectionIndex( lpNextNode->GetSectionIndex() ) == miCurrentPlayerRoadIndex )
                        {
                            // Perpendicular of the node->nextNode segment on the ground plane.
                            mCurrentPlayerSectionExitRight.Set( -( lfNodeY - lfNextY ), 0.0f, ( lfNodeX - lfNextX ) );

                            CGS_ASSERT( !rw::math::fpu::IsZero( mCurrentPlayerSectionExitRight, 0.00000011920929f ),
                                        "!RwMathFPU::IsZero( mCurrentPlayerSectionExitRight )" );

                            const f32 lfX = mCurrentPlayerSectionExitRight.X();
                            const f32 lfY = mCurrentPlayerSectionExitRight.Y();
                            const f32 lfZ = mCurrentPlayerSectionExitRight.Z();
                            const f32 lfInvLength = 1.0f / sqrtf( ( lfZ * lfZ ) + ( ( lfX * lfX ) + ( lfY * lfY ) ) );
                            mCurrentPlayerSectionExitRight.Set( lfX * lfInvLength, lfY * lfInvLength, lfZ * lfInvLength );
                        }
                    }
                    else
                    {
                        // A turn: classify it left / right against the exit-right vector.
                        CGS_ASSERT( !rw::math::fpu::IsSimilar( lNodePos, lNextNodePos, 0.000015258789f ),
                                    "!RwMathFPU::IsSimilar( lNodePos, lNextNodePos )" );

                        const f32 lfInvLength = 1.0f
                            / sqrtf( ( ( lfNextX - lfNodeX ) * ( lfNextX - lfNodeX ) )
                                   + ( ( ( lfNextY - lfNodeY ) * ( lfNextY - lfNodeY ) )
                                     + ( ( 0.0f - 0.0f ) * ( 0.0f - 0.0f ) ) ) );
                        rw::math::fpu::Vector3Template<f32> lNodeVector;
                        lNodeVector.Set( ( lfNextX - lfNodeX ) * lfInvLength,
                                         ( 0.0f - 0.0f ) * lfInvLength,
                                         ( lfNextY - lfNodeY ) * lfInvLength );

                        CGS_ASSERT( rw::math::fpu::IsValid( lNodeVector ), "RwMathFPU::IsValid( lNodeVector )" );

                        const f32 lfDot =
                            ( lNodeVector.X() * mCurrentPlayerSectionExitRight.X() )
                          + ( ( mCurrentPlayerSectionExitRight.Z() * lNodeVector.Z() )
                            + ( mCurrentPlayerSectionExitRight.Y() * lNodeVector.Y() ) );

                        if ( lfDot >= 0.0f )
                        {
                            // Left turn.
                            liRightRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
                            liRightRunCount    = 0;

                            s32 liPreviousLeftRunCount;
                            if ( liRoadIndex == liLeftRoadTracker )
                            {
                                liPreviousLeftRunCount = liLeftRunCount;
                            }
                            else
                            {
                                miNewRoadNodeIndex = liNodeIndex;
                                mLastNode.Set( 0.0f, 0.0f, 0.0f );
                                liLeftRoadTracker = liRoadIndex;
                                liPreviousLeftRunCount = 0;
                                mSecondLastNode.Set( 0.0f, 0.0f, 0.0f );
                            }
                            liLeftRunCount = liPreviousLeftRunCount + 1;

                            if ( ( liPreviousLeftRunCount + 1 >= KI_MIN_NODES_FOR_DECISION || lbSectionIsInterstateExit )
                              && miHopefulLeftPlayerRoadIndex == liLeftRoadTracker )
                            {
                                mbLeftRoadHighlighted = true;
                                mLastNode.Set( lfNextX, 0.0f, lfNextY );
                                mSecondLastNode.Set( lfNodeX, 0.0f, lfNodeY );
                                miLastRoadNodeIndex = liNodeIndex;

                                if ( mbLockLeftSign && mfGuidanceLockTime < 1.0f )
                                {
                                    mbLeftRoadHighlighted = true;
                                    goto LABEL_SEND;
                                }

                                const f32 lfDistanceToTurning = CalculateDistanceToTurning( lpLastAICarOutputInterface,
                                                                                            lpActiveRaceCarInterface );
                                const ::EActiveRaceCarIndex lePlayerIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
                                const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
                                    lpActiveRaceCarInterface->GetRaceCarState( lePlayerIndex );
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfMaxSpeedMPH ) * 4.0f ) > lfDistanceToTurning
                                    || lfDistanceToTurning < 50.0f )
                                  && lfDistanceToTurning != 0.0f )
                                {
                                    mfGuidanceLockTime = 0.0f;
                                    mbLockRightSign    = false;
                                    mbLockLeftSign     = true;
                                    goto LABEL_SEND;
                                }
                            }
                        }
                        else
                        {
                            // Right turn.
                            liLeftRoadTracker = BrnStreetData::KI_INVALID_ROAD_INDEX;
                            liLeftRunCount    = 0;

                            s32 liPreviousRightRunCount;
                            if ( liRoadIndex == liRightRoadTracker )
                            {
                                liPreviousRightRunCount = liRightRunCount;
                            }
                            else
                            {
                                miNewRoadNodeIndex = liNodeIndex;
                                mLastNode.Set( 0.0f, 0.0f, 0.0f );
                                liRightRoadTracker = liRoadIndex;
                                liPreviousRightRunCount = 0;
                                mSecondLastNode.Set( 0.0f, 0.0f, 0.0f );
                            }
                            liRightRunCount = liPreviousRightRunCount + 1;

                            if ( ( liPreviousRightRunCount + 1 >= KI_MIN_NODES_FOR_DECISION || lbSectionIsInterstateExit )
                              && miHopefulRightPlayerRoadIndex == liRightRoadTracker )
                            {
                                mbRightRoadHighlighted = true;
                                mLastNode.Set( lfNextX, 0.0f, lfNextY );
                                mSecondLastNode.Set( lfNodeX, 0.0f, lfNodeY );
                                miLastRoadNodeIndex = liNodeIndex;

                                if ( mbLockRightSign && mfGuidanceLockTime < 1.0f )
                                {
                                    mbRightRoadHighlighted = true;
                                    goto LABEL_SEND;
                                }

                                const f32 lfDistanceToTurning = CalculateDistanceToTurning( lpLastAICarOutputInterface,
                                                                                            lpActiveRaceCarInterface );
                                const ::EActiveRaceCarIndex lePlayerIndex = lpActiveRaceCarInterface->GetPlayerActiveRaceCarIndex();
                                const BrnPhysics::Vehicle::RaceCarState* lpPlayerRaceCarState =
                                    lpActiveRaceCarInterface->GetRaceCarState( lePlayerIndex );
                                if ( ( ( ( 0.44704f * lpPlayerRaceCarState->mfMaxSpeedMPH ) * 4.0f ) > lfDistanceToTurning
                                    || lfDistanceToTurning < 50.0f )
                                  && lfDistanceToTurning != 0.0f )
                                {
                                    mfGuidanceLockTime = 0.0f;
                                    mbLockLeftSign     = false;
                                    mbLockRightSign    = true;
                                    goto LABEL_SEND;
                                }
                            }
                        }
                    }

                    // loc_8234F5C8 -- advance, bounded by the lookahead window.
                    liNodeIndex = liNextNodeIndex;
                    if ( liNodeIndex >= liLookaheadEnd )
                    {
                        break;
                    }
                }
            }

            goto LABEL_HIGHLIGHT_FROM_LOCKS;
        }

    // loc_8234F5E4
        mbLockLeftSign     = false;
        mbLockRightSign    = false;
        mfGuidanceLockTime = 0.0f;

    LABEL_HIGHLIGHT_FROM_LOCKS:   // loc_8234F5F8
        mbRightRoadHighlighted = mbLockRightSign;
        mbLeftRoadHighlighted  = mbLockLeftSign;

    LABEL_SEND:                   // loc_8234F608
        SendUpcomingRoadMessage( lpOutput, lpLastAICarOutputInterface, true, lpActiveRaceCarInterface );

    LABEL_DONE:                   // loc_8234F620
        return;
    }
}
