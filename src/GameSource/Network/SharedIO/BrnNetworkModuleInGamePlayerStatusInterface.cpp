#include "types.hpp"
#include <cstring>

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData::Clear  @ 0x823555A8
//
// Resets one player's in-game status record to its empty defaults: the player
// stats sub-object is cleared, the live-revenge relationship and name are zeroed,
// and every id / active-race-car index is set to "none" (-1) with the status
// enums returned to their defaults. Member layout recovered from the DecFIGS DWARF
// (BrnNetworkModuleInGamePlayerStatusInterface.h).

namespace BrnNetwork
{
    enum EActiveRaceCarIndex : s32 { E_ACTIVE_RACE_CAR_NONE = -1 };
    enum ECameraStatus       : s32 { E_CAMERA_STATUS_NONE = 0 };

    typedef s32 NetworkPlayerID;

    // Sub-objects (own TUs); reset helpers are trap stubs until they land.
    struct NetworkPlayerStats
    {
        u8 mPad0[136];
        void Clear();
    };
    void NetworkPlayerStats::Clear() { __debugbreak(); }

    struct LiveRevengeRelationship
    {
        // Sized so mNetworkPlayerID lands at object +272, matching the X360 stores.
        u8 mPad0[116];
    };

    struct PlayerName { char macName[20]; };

    namespace BrnNetworkModuleIO
    {
        // Default camera status (X360 stores 4 at +284 in the cleared record).
        static const s32 KI_CAMERA_STATUS_DEFAULT = 4;

        struct InGamePlayerStatusData
        {
            NetworkPlayerStats      mPlayerStats;
            LiveRevengeRelationship mLiveRevengeRelationship;
            PlayerName              mPlayerName;
            NetworkPlayerID         mNetworkPlayerID;
            EActiveRaceCarIndex     meActiveRaceCarIndex;
            s32                     meVOIPStatus;
            ECameraStatus           meCameraStatus;
            NetworkPlayerID         mMarkedManPlayerID;
            EActiveRaceCarIndex     meMarkedManActiveRaceCarIndex;
            s32                     meDistrict;
            bool                    mbMarkedMan;
            bool                    mbIsHost;
            bool                    mbIsLocalPlayer;
            bool                    mbIsInLocalGameWorld;

            void Clear();
        };

        void InGamePlayerStatusData::Clear()
        {
            mNetworkPlayerID             = -1;   // +272
            meActiveRaceCarIndex         = E_ACTIVE_RACE_CAR_NONE; // +276
            meVOIPStatus                 = 0;    // +280
            meCameraStatus               = static_cast<ECameraStatus>(KI_CAMERA_STATUS_DEFAULT); // +284
            mMarkedManPlayerID           = -1;   // +288
            meMarkedManActiveRaceCarIndex = E_ACTIVE_RACE_CAR_NONE; // +292
            meDistrict                   = 0;    // +296
            mbMarkedMan                  = false;
            mbIsHost                     = false;
            mbIsLocalPlayer              = false;
            mbIsInLocalGameWorld         = false;

            mPlayerStats.Clear();
            std::memset(&mLiveRevengeRelationship, 0, sizeof(mLiveRevengeRelationship));
            std::memset(&mPlayerName, 0, sizeof(mPlayerName));
        }
    }
}
