#include "GameSource/Network/Parameters/BrnNetworkEventScoreData.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/Development/CgsStrStream.h"

#include "lobbytagfield.h"   // TagFieldSetStructure
#include <cstring>            // std::memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::EventScoreUploadData::Construct              @ 0x82584AE0
//   BrnNetwork::EventScoreUploadData::SetScoreData           @ 0x82584AF0  (AddEventScore)
//   BrnNetwork::EventScoreUploadData::SerialiseToString      @ 0x82584CA0
//   BrnNetwork::EventScoreUploadData::GetDataSize            @ 0x82584D38
//   BrnNetwork::EventScoreUploadData::`vector deleting dtor' @ 0x8254B468

namespace BrnNetwork
{
    EventScoreData::EventScoreData()
    {
        Construct();
    }

    EventScoreData::~EventScoreData()
    {
    }

    // Construct @ 0x82584AE0 -- zero the whole 184-byte payload after the vptr.
    void EventScoreData::Construct()
    {
        std::memset( &maScoreboardIndex[0], 0, 184 );
    }

    // AddEventScore (X360 SetScoreData @ 0x82584AF0).
    bool EventScoreData::AddEventScore( s32 liScoreboardIndex, s32 liScore, s32 liGameMode )
    {
        CGS_ASSERT( liScoreboardIndex >= 0, "liScoreboardIndex >= 0" );
        CGS_ASSERT( miNumScores >= 0, "mEventScoreData.miNumScores >= 0" );
        CGS_ASSERT( miNumScores < KI_MAX_EVENT_SCORES_TO_UPLOAD,
                    "mEventScoreData.miNumScores < KI_MAX_EVENT_SCORES_TO_UPLOAD" );

        if ( liGameMode == 5 )
        {
            CGS_ASSERT( liScoreboardIndex < 35,
                        "liScoreboardIndex < KI_NUM_BURN_ROUTE_EVENT_SCOREBOARDS" );
        }
        else if ( liGameMode == 7 )
        {
            CGS_ASSERT( liScoreboardIndex < 14,
                        "liScoreboardIndex < KI_NUM_STUNT_RUN_EVENT_SCOREBOARDS" );
        }
        else
        {
            char lacMessageBuffer[CgsDev::Assert::KI_MESSAGEBUFFERSIZE];
            CgsDev::StrStream lStream( lacMessageBuffer, CgsDev::Assert::KI_MESSAGEBUFFERSIZE );
            lStream << "Trying to upload score data for game mode without a leaderboard: "
                    << liGameMode << "\n";
            CgsDev::Assert::BeginAssert();
            CgsDev::Assert::FireAssert( lacMessageBuffer,
                                        "..\\..\\..\\GameSource\\Network/Parameters/BrnNetworkEventScoreData.cpp",
                                        96 );
            CgsDev::Assert::EndAssert();
        }

        maScoreboardIndex[miNumScores] = liScoreboardIndex;
        maScore[miNumScores]           = liScore;
        maGameMode[miNumScores]        = liGameMode;
        ++miNumScores;

        return miNumScores == KI_MAX_EVENT_SCORES_TO_UPLOAD;
    }

    // SerialiseToString @ 0x82584CA0.
    void EventScoreData::SerialiseToString( char* lpcRecord, s32 liRecLen )
    {
        lpcRecord[0] = 0;

        const char* lpcPattern = GetPattern();
        const s32   liLength   = static_cast<s32>( GetDataSize() );
        void*       lpData     = GetData();
        TagFieldSetStructure( lpcRecord, liRecLen, "EVSCORE", lpData, liLength, lpcPattern );
    }

    // GetDataSize @ 0x82584D38 -- returns 184.
    u32 EventScoreData::GetDataSize() const
    {
        return 184;
    }
}
