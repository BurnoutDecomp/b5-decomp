#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include "lobbytagfield.h"   // TagFieldSetStructure
#include <xtl.h>              // XMemSet

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::EventScoreUploadData::Construct              @ 0x82584AE0
//   BrnNetwork::EventScoreUploadData::SetScoreData           @ 0x82584AF0
//   BrnNetwork::EventScoreUploadData::SerialiseToString      @ 0x82584CA0
//   BrnNetwork::EventScoreUploadData::GetDataSize            @ 0x82584D38
//   BrnNetwork::EventScoreUploadData::`vector deleting dtor' @ 0x8254B468

// ARTIST's asserts name this file through the network unity TU (0x82584B28).
#define CGS_ARTIST_FILE "d:\\p4\\b5_main\\burnout\\main\\code\\gamesource\\unity\\../Network/Parameters/BrnNetworkEventScoreData.cpp"

namespace BrnNetwork
{
    // Construct @ 0x82584AE0 -- zero the whole 184-byte payload after the vptr.
    void EventScoreUploadData::Construct()
    {
        XMemSet( &mEventScoreData, 0, sizeof( mEventScoreData ) );
    }

    // SetScoreData @ 0x82584AF0.
    bool EventScoreUploadData::SetScoreData( s32 liScoreboardIndex, s32 liScore, s32 liGameMode )
    {
        CGS_ASSERT_AT( liScoreboardIndex >= 0, "liScoreboardIndex >= 0", CGS_ARTIST_FILE, 75 );
        CGS_ASSERT_AT( mEventScoreData.miNumScores >= 0, "mEventScoreData.miNumScores >= 0",
                       CGS_ARTIST_FILE, 76 );
        CGS_ASSERT_AT( mEventScoreData.miNumScores < KI_MAX_EVENT_SCORES_TO_UPLOAD,
                       "mEventScoreData.miNumScores < KI_MAX_EVENT_SCORES_TO_UPLOAD", CGS_ARTIST_FILE, 77 );

        // Stunt run (line 84) before burning route (line 90), the unsupported mode last (96).
        switch ( liGameMode )
        {
        case 7:
            CGS_ASSERT_AT( liScoreboardIndex < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS,
                           "liScoreboardIndex < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS", CGS_ARTIST_FILE, 84 );
            break;
        case 5:
            CGS_ASSERT_AT( liScoreboardIndex < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS,
                           "liScoreboardIndex < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS", CGS_ARTIST_FILE, 90 );
            break;
        default:
            CGS_FAIL_STREAM_AT( "Trying to upload score data for game mode without a leaderboard: "
                                         << liGameMode << "\n", CGS_ARTIST_FILE, 96 );
            break;
        }

        mEventScoreData.maScoreboardIndex[mEventScoreData.miNumScores] = liScoreboardIndex;
        mEventScoreData.maScore[mEventScoreData.miNumScores]           = liScore;
        mEventScoreData.maGameMode[mEventScoreData.miNumScores]        = liGameMode;
        ++mEventScoreData.miNumScores;

        return mEventScoreData.miNumScores == KI_MAX_EVENT_SCORES_TO_UPLOAD;
    }

    // SerialiseToString @ 0x82584CA0. The virtuals are called right to left (GetPattern,
    // GetDataSize, then the const GetData at slot +0x10), as the argument list evaluates.
    void EventScoreUploadData::SerialiseToString( char* lpcRecord, s32 liRecLen ) const
    {
        lpcRecord[0] = 0;
        TagFieldSetStructure( lpcRecord, liRecLen, "EVSCORE",
                              GetData(), static_cast<s32>( GetDataSize() ), GetPattern() );
    }

    // GetDataSize @ 0x82584D38 -- returns 184.
    u32 EventScoreUploadData::GetDataSize() const
    {
        return sizeof( EventScoreData );
    }

    // The serialisation pattern: one long field, repeated ("l*"). The vtable slot shares its body
    // with GameParams::GetPattern.
    const char* EventScoreUploadData::GetPattern() const
    {
        return "l*";
    }

    // The payload the pattern describes starts right after the vptr (+0x04).
    void* EventScoreUploadData::GetData()
    {
        return &mEventScoreData;
    }

    const void* EventScoreUploadData::GetData() const
    {
        return &mEventScoreData;
    }
}
