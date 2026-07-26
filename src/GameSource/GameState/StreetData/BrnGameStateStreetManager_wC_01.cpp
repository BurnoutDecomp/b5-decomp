// ===========================================================================
// GameSource/GameState/StreetData/BrnGameStateStreetManager_wC_01.cpp
//   (wave C partfile -- group 1 "upcoming-road message")
//
//   BrnGameState::StreetManager::SendUpcomingRoadMessage @ 0x82348798
//
// Reconstructed store-for-store from BURNOUT_X360_ARTIST.XEX against the frozen
// StreetManager layout. The wave-B "unk_82F30000 payload vtable" blocker was a
// Hex-Rays misread: that `lis r30, 0x82F3` is the CgsDev::Assert::gpcMessageBuffer
// base used by the four streamed dev warnings, NOT a store into the message. The
// message itself is a plain 368-byte stack struct (frame base 0x270+var_200)
// posted with the attested AddEvent(..., 276, 368) literals.
//
// Every store / branch / early-out has an asm counterpart; members are the
// frozen-header named fields (never raw-offset casts).
// ===========================================================================

#include "GameSource/GameState/StreetData/BrnGameStateStreetManager.h"

#include "GameSource/GameState/BrnGameStateModuleIO.h"             // GameStateModuleIO::OutputBuffer / GetGuiOutputQueue (13312 queue)
#include "GameShared/GameClasses/Module/CgsVariableEventQueue.h"   // CgsModule::Event / VariableEventQueue<13312,16>::AddEvent
#include "GameShared/GameClasses/Development/PerfMon/Cpu/CgsPerfMonCpu.h" // CgsDev::PerfMonCpu::Start/StopMonitor
#include "GameShared/GameClasses/Development/CgsStrStream.h"       // CgsDev::StrStream (dev warning messages)
#include "GameShared/GameClasses/Core/CgsAssert.h"                 // CgsDev::Assert::Begin/Fire/EndAssert + KI_MESSAGEBUFFERSIZE
#include "BrnCommonTypes.h"                                        // ::CgsID / Vector3 (rw::math::vpu, 16B lanes)

namespace
{
    // -----------------------------------------------------------------------
    // The contiguous 368-byte upcoming-road GUI message the X360 builds on the
    // stack and hands to VariableEventQueue<13312,16>::AddEvent(&msg, 276, 368).
    //
    // FLAG: the FIELD NAMES below are ours -- this GUI message has no DWARF
    // declaration and the X360 never names it. The offsets, widths, store order
    // and the (276, 368) post literals are byte-exact from the asm frame
    // (payload base = 0x270+var_200 == sp+0x70; see the per-field comments for
    // the matching `0x270+var_XX` slots).
    //
    // The two 16-byte vector slots are the two lvx128/stvx128 pairs at
    // 0x82348868 / 0x82348874 -- both fed from the SAME staging copy of
    // mFirstJunctionPosition (w lane zeroed).
    // -----------------------------------------------------------------------
    struct UpcomingRoadMessage
    {
        ::CgsID mLeftRoadId;                                            // +0x000  var_200
        ::CgsID mRightRoadId;                                           // +0x008  var_1F8
        Vector3 mJunctionPosition;                                      // +0x010  var_1F0
        Vector3 mSecondJunctionPosition;                                // +0x020  var_1E0
        f32     mfLeftSignValue;                                        // +0x030  var_1D0
        f32     mfRightSignValue;                                       // +0x034  var_1CC
        BrnStreetData::ChallengeParScoresEntry   mLeftParScore;         // +0x038  var_1C8 (40B)
        BrnStreetData::ChallengeHighScoreEntry   mLeftFriendHighScore;  // +0x060  var_1A0 (56B)
        BrnStreetData::ChallengePlayerScoreEntry mLeftUserScore;        // +0x098  var_168 (40B)
        BrnStreetData::ChallengeParScoresEntry   mRightParScore;        // +0x0C0  var_140 (40B)
        BrnStreetData::ChallengeHighScoreEntry   mRightFriendHighScore; // +0x0E8  var_118 (56B)
        BrnStreetData::ChallengePlayerScoreEntry mRightUserScore;       // +0x120  var_E0  (40B)
        s32     miCurrentRoadHighlightState;                            // +0x148  var_B8
        s32     miLeftRoadHighlightState;                               // +0x14C  var_B4
        s32     miRightRoadHighlightState;                              // +0x150  var_B0
        BrnStreetData::RoadIndex miCurrentRoadIndex;                    // +0x154  var_AC  (== miLastPlayerRoadIndex)
        BrnStreetData::RoadIndex miLeftRoadIndex;                       // +0x158  var_A8
        BrnStreetData::RoadIndex miRightRoadIndex;                      // +0x15C  var_A4
        u8      mu8LeftRoadIsInterstate;                                // +0x160  var_A0
        u8      mu8RightRoadIsInterstate;                               // +0x161  var_9F
    };
    static_assert( sizeof(UpcomingRoadMessage) == 368, "upcoming-road message is 0x170" );

    // The interstate sentinel road id both sides store for a -2 road index. The
    // X360 materialises it inline (lis 0x6845 / ori 0x61A1 / lis 0x6800 /
    // insrdi 32,0 at 0x82348A14 and 0x82348BB4).
    const ::CgsID KU_INTERSTATE_ROAD_ID = 0x684561A168000000ULL;

    // The road-index value the walk stores for "the player is leaving the
    // interstate" (miOriginalInterStateRoadIndex holds the real road; see
    // FindUpcomingStreetsFromRoute, which writes -2 into mLeft/mRightRoadIndex).
    // Compared as a raw immediate by the X360 (cmpwi r29, -2).
    const BrnStreetData::RoadIndex KI_INTERSTATE_EXIT_ROAD_INDEX = -2;
}

namespace BrnGameState
{
    // -----------------------------------------------------------------------
    // @ 0x82348798. Packs the upcoming-road GUI message -- the left/right road
    // ids plus each side's par / friend / user score records -- and posts it on
    // the GUI output queue. Perf-monitored end to end with
    // miUpcomingRoadsSentMessagePM.
    //
    // NOTE the frozen X360 arity: lpLastAICarOutputInterface (r5) and
    // lpActiveRaceCarInterface (r7) are carried but never read by this body.
    // -----------------------------------------------------------------------
    void StreetManager::SendUpcomingRoadMessage(
            GameStateModuleIO::OutputBuffer* lpOutput,
            BrnAI::AIModuleIO::AICarOutputInterface* lpLastAICarOutputInterface,
            bool lbForce,
            BrnWorld::RaceCarEntityModuleIO::RCEntityActiveRaceCarOutputInterface* lpActiveRaceCarInterface )
    {
        (void)lpLastAICarOutputInterface;
        (void)lpActiveRaceCarInterface;

        CgsDev::PerfMonCpu::StartMonitor( miUpcomingRoadsSentMessagePM );

        // The message is deliberately NOT cleared: the -1 / -2 road-index paths
        // below leave their side's score records as whatever the stack held, and
        // the X360 does exactly the same (no memset in the prologue).
        UpcomingRoadMessage lMessage;

        // The two 16-byte vector slots: mFirstJunctionPosition promoted to a
        // 4-lane vector twice (the two staging buffers at var_210 and var_220).
        Vector3 lJunctionPosition;
        lJunctionPosition.x = mFirstJunctionPosition.X();
        lJunctionPosition.y = mFirstJunctionPosition.Y();
        lJunctionPosition.z = mFirstJunctionPosition.Z();
        lJunctionPosition.w = 0.0f;

        Vector3 lSecondJunctionPosition;
        lSecondJunctionPosition.x = mFirstJunctionPosition.X();
        lSecondJunctionPosition.y = mFirstJunctionPosition.Y();
        lSecondJunctionPosition.z = mFirstJunctionPosition.Z();
        lSecondJunctionPosition.w = 0.0f;

        lMessage.miCurrentRoadIndex = miLastPlayerRoadIndex;
        lMessage.miLeftRoadIndex    = mLeftRoadIndex;
        lMessage.miRightRoadIndex   = mRightRoadIndex;

        lMessage.mLeftRoadId  = 0;
        lMessage.mRightRoadId = 0;
        lMessage.mu8LeftRoadIsInterstate  = 0;
        lMessage.mu8RightRoadIsInterstate = 0;
        lMessage.mfLeftSignValue  = 0.0f;
        lMessage.mfRightSignValue = 0.0f;
        lMessage.miCurrentRoadHighlightState = -1;
        lMessage.miLeftRoadHighlightState    = -1;

        lMessage.mJunctionPosition       = lJunctionPosition;
        lMessage.mSecondJunctionPosition = lSecondJunctionPosition;

        // ---- left side ------------------------------------------------------
        const BrnStreetData::RoadIndex liLeftRoadIndex = mLeftRoadIndex;
        if ( liLeftRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
        {
            lMessage.mLeftRoadId  = 0;
            mbLeftRoadHighlighted = false;
        }
        else if ( liLeftRoadIndex == KI_INTERSTATE_EXIT_ROAD_INDEX )
        {
            lMessage.mu8LeftRoadIsInterstate = 1;
            lMessage.mLeftRoadId             = KU_INTERSTATE_ROAD_ID;
        }
        else
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liLeftRoadIndex );
            const ::CgsID lLeftRoadId = lpRoad->GetId();

            lMessage.mu8LeftRoadIsInterstate = 0;
            lMessage.mLeftRoadId             = lLeftRoadId;

            // Persisted save-game challenge slot for this road (miss -> -1, which
            // makes the user/friend getters early out).
            BrnStreetData::ChallengeIndex liSaveGameSlotIndex = -1;
            {
                BrnStreetData::ChallengeIndex liSlot = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lLeftRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSlot;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSlot = -1;
                        break;
                    }
                }
                liSaveGameSlotIndex = liSlot;
            }

            // Par scores index by the ROAD index; the user/friend tables by the
            // save-game slot (lbByRoadIndex == false).
            GetChallengeParScore( mLeftRoadIndex, &lMessage.mLeftParScore );
            GetChallengeUserScore( liSaveGameSlotIndex, &lMessage.mLeftUserScore, false );
            GetChallengeFriendHighScore( liSaveGameSlotIndex, &lMessage.mLeftFriendHighScore, false );

            if ( !lMessage.mLeftParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_TIME ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find time road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mLeftRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }

            if ( !lMessage.mLeftParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_CRASH ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find crash road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mLeftRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }
        }

        // ---- right side -----------------------------------------------------
        // (the right-hand warnings ALSO read "on left side" in the retail image --
        //  a copy-paste in the original source; kept verbatim.)
        const BrnStreetData::RoadIndex liRightRoadIndex = mRightRoadIndex;
        if ( liRightRoadIndex == BrnStreetData::KI_INVALID_ROAD_INDEX )
        {
            lMessage.mRightRoadId  = 0;
            mbRightRoadHighlighted = false;
        }
        else if ( liRightRoadIndex == KI_INTERSTATE_EXIT_ROAD_INDEX )
        {
            lMessage.mu8RightRoadIsInterstate = 1;
            lMessage.mRightRoadId             = KU_INTERSTATE_ROAD_ID;
        }
        else
        {
            const BrnStreetData::Road* lpRoad = GetStreetData()->GetRoad( liRightRoadIndex );
            const ::CgsID lRightRoadId = lpRoad->GetId();

            lMessage.mu8RightRoadIsInterstate = 0;
            lMessage.mRightRoadId             = lRightRoadId;

            BrnStreetData::ChallengeIndex liSaveGameSlotIndex = -1;
            {
                BrnStreetData::ChallengeIndex liSlot = 0;
                const ::CgsID* lpSlotRoadId = KAA_SAVE_GAME_CHALLENGE_ROAD_IDS;
                while ( *lpSlotRoadId != lRightRoadId )
                {
                    ++lpSlotRoadId;
                    ++liSlot;
                    if ( lpSlotRoadId >= &KAA_SAVE_GAME_CHALLENGE_ROAD_IDS[KI_MAX_CHALLENGES] )
                    {
                        liSlot = -1;
                        break;
                    }
                }
                liSaveGameSlotIndex = liSlot;
            }

            GetChallengeParScore( mRightRoadIndex, &lMessage.mRightParScore );
            GetChallengeUserScore( liSaveGameSlotIndex, &lMessage.mRightUserScore, false );
            GetChallengeFriendHighScore( liSaveGameSlotIndex, &lMessage.mRightFriendHighScore, false );

            if ( !lMessage.mRightParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_TIME ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find time road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mRightRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }

            if ( !lMessage.mRightParScore.ContainsData( BrnStreetData::E_SCORE_TYPE_CRASH ) )
            {
                CgsDev::Assert::BeginAssert();
                char lacMessage[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
                CgsDev::StrStream lStrStream( lacMessage, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
                lStrStream << "Failed to find crash road rule scores for upcoming road on left side: ";
                lStrStream << lMessage.mRightRoadId;
                lStrStream << "\n";
                CgsDev::Assert::FireAssert( lacMessage, __FILE__, __LINE__ );
                CgsDev::Assert::EndAssert();
            }
        }

        // ---- highlight states ------------------------------------------------
        if ( !lbForce )
        {
            lMessage.mfLeftSignValue             = 0.0f;
            lMessage.miLeftRoadHighlightState    = 0;
            lMessage.miRightRoadHighlightState   = 0;
            lMessage.miCurrentRoadHighlightState = 0;
        }
        else if ( mbLeftRoadHighlighted )
        {
            lMessage.miRightRoadHighlightState   = 1;
            lMessage.mRightRoadId                = 0;
            lMessage.miLeftRoadHighlightState    = 2;
            lMessage.miCurrentRoadHighlightState = 0;
        }
        else
        {
            lMessage.miCurrentRoadHighlightState = 0;
            lMessage.miLeftRoadHighlightState    = 1;
            lMessage.mLeftRoadId                 = 0;

            if ( mbRightRoadHighlighted )
            {
                lMessage.miRightRoadHighlightState = 2;
            }
            else
            {
                lMessage.mfLeftSignValue           = 0.0f;
                lMessage.miRightRoadHighlightState = 1;
                lMessage.mRightRoadId              = 0;
                lMessage.mfRightSignValue          = -1.0f;
            }
        }

        CgsModule::VariableEventQueue<13312, 16>* lpQueue = lpOutput->GetGuiOutputQueue();
        lpQueue->AddEvent( reinterpret_cast<const CgsModule::Event*>( &lMessage ), 276, 368 );

        CgsDev::PerfMonCpu::StopMonitor( miUpcomingRoadsSentMessagePM );
    }
}
