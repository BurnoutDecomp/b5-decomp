#include "GameSource/Network/SharedIO/BrnNetworkModuleInGamePlayerStatusInterface.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData::Clear      @ 0x823555A8
//   BrnNetwork::BrnNetworkModuleIO::InGamePlayerStatusData::operator=  @ 0x823628C8
//
// The InGamePlayerStatusData full layout is now defined in the promoted header
// BrnNetworkModuleInGamePlayerStatusInterface.h (included above), which uses the real committed
// NetworkPlayerStats (BrnNetworkPlayerStats.h) and LiveRevengeRelationship
// (BrnNetworkLiveRevengeRelationship.h, grown to 120B) types -- NOT local trap stubs. Member
// layout recovered from the DecFIGS DWARF (BrnNetworkModuleInGamePlayerStatusInterface.h:63).

namespace BrnNetwork
{
    namespace BrnNetworkModuleIO
    {
        // Resets one player's in-game status record to its empty defaults: the player
        // stats sub-object is cleared, the live-revenge relationship is zeroed, the name
        // buffer is emptied (only macName[0] is null-terminated -- the X360 asm stores a
        // single zero byte at +256, NOT a full 16-byte clear of PlayerName), and every
        // id / active-race-car index is set to "none" (-1) with the status enums returned
        // to their defaults. meDistrict/mbIsHost/mbIsLocalPlayer/mbIsInLocalGameWorld are
        // NOT touched here -- the X360 asm (0x823555A8) never stores to +296/+301/+302/+303
        // in this function, so those fields retain their prior value.
        void InGamePlayerStatusData::Clear()
        {
            mPlayerName.macName[0]        = 0;                                   // +256 (single byte, not a full memset)
            mNetworkPlayerID              = -1;                                  // +272
            meActiveRaceCarIndex          = E_ACTIVE_RACE_CAR_NONE;              // +276
            meVOIPStatus                  = 0;                                   // +280
            meCameraStatus                = static_cast<ECameraStatus>(KI_CAMERA_STATUS_DEFAULT); // +284
            mMarkedManPlayerID            = -1;                                  // +288
            meMarkedManActiveRaceCarIndex = E_ACTIVE_RACE_CAR_NONE;              // +292
            mbMarkedMan                   = false;                               // +300

            mPlayerStats.Clear();
            mLiveRevengeRelationship.Construct();
        }

        // X360 0x823628C8 (ledger stub "InGamePl").
        // Member-wise copy assignment for one player's in-game status record. The X360 body
        // is the compiler's lowering of a default member-wise operator=: it forwards the stats
        // sub-object to NetworkPlayerStats::operator= (0x82355C50), block-copies the
        // LiveRevengeRelationship (+136..+256, 120B) and PlayerName (+256, 16B), then copies the
        // scalar id/enum/bool tail (+272..+303). Reversed back into named-member assignments;
        // behaviour is identical to the inlined memcpy/word-store sequence. (The X360 also copies
        // the +304 trailing pad word of the 312-byte record stride; that is inert alignment with
        // no named member and is left as default padding.)
        InGamePlayerStatusData& InGamePlayerStatusData::operator=(const InGamePlayerStatusData& lOther)
        {
            mPlayerStats                  = lOther.mPlayerStats;                  // +0   (NetworkPlayerStats::operator= @0x82355C50)
            mLiveRevengeRelationship      = lOther.mLiveRevengeRelationship;      // +136 (120B block copy, lands PlayerName at +256)
            mPlayerName                   = lOther.mPlayerName;                   // +256 (16B)
            mNetworkPlayerID              = lOther.mNetworkPlayerID;              // +272
            meActiveRaceCarIndex          = lOther.meActiveRaceCarIndex;          // +276
            meVOIPStatus                  = lOther.meVOIPStatus;                  // +280
            meCameraStatus                = lOther.meCameraStatus;               // +284
            mMarkedManPlayerID            = lOther.mMarkedManPlayerID;            // +288
            meMarkedManActiveRaceCarIndex = lOther.meMarkedManActiveRaceCarIndex; // +292
            meDistrict                    = lOther.meDistrict;                   // +296
            mbMarkedMan                   = lOther.mbMarkedMan;                   // +300
            mbIsHost                      = lOther.mbIsHost;                      // +301
            mbIsLocalPlayer               = lOther.mbIsLocalPlayer;              // +302
            mbIsInLocalGameWorld          = lOther.mbIsInLocalGameWorld;         // +303
            return *this;
        }

        // ================================================================
        // MarkedManInterface::CheckForMarkedManTakedown  (X360 0x82355758)
        // ================================================================
        // Returns whether the aggressor's currently-marked-man victim (as recorded in this
        // per-aggressor table) equals the supplied victim slot -- i.e. this takedown was of the
        // aggressor's marked man. Both car slots are range-asserted (> INVALID && < COUNT) as on the
        // X360; the baked assert-message strings are reproduced verbatim (file path + line dropped per
        // convention). The equality return is the X360 cntlzw/extrwi BOOL idiom.
        bool MarkedManInterface::CheckForMarkedManTakedown(
                ::EActiveRaceCarIndex leAggressorActiveRaceCarIndex,
                ::EActiveRaceCarIndex leVictimActiveRaceCarIndex) const
        {
            CGS_ASSERT((leAggressorActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leAggressorActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leAggressorActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leAggressorActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");
            CGS_ASSERT((leVictimActiveRaceCarIndex > ::E_ACTIVE_RACE_CAR_INDEX_INVALID) &&
                       (leVictimActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                       "( leVictimActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( leVictimActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

            return maMarkedManActiveRaceCarIndex[leAggressorActiveRaceCarIndex] == leVictimActiveRaceCarIndex;
        }

        // ================================================================
        // MarkedManInterface::SetFromPlayerStatusInterface  (X360 0x82355670)
        // ================================================================
        // Rebuilds this per-aggressor marked-man table from an in-game player-status interface: for
        // each of the interface's miNumPlayers records, indexes this table by that player's own
        // active-race-car slot and stores that player's marked-man target slot. The X360 body walks
        // the record array by raw offset (record.meActiveRaceCarIndex @ +276,
        // record.meMarkedManActiveRaceCarIndex @ +292, stride 312) and reads miNumPlayers @ +0x9E4;
        // reversed here into the named accessors -- identical bytes. The loop index and each record's
        // active-race-car slot are range-asserted exactly as on the X360; assert strings reproduced
        // verbatim (file path + line dropped). The store static_casts the BrnNetwork:: field into the
        // global ::EActiveRaceCarIndex table element. Returns *this.
        MarkedManInterface& MarkedManInterface::SetFromPlayerStatusInterface(
                const InGamePlayerStatusInterface& lPlayerStatusInterface)
        {
            const s32 liNumPlayers = lPlayerStatusInterface.GetNumPlayers();
            for (s32 liIndex = 0; liIndex < liNumPlayers; ++liIndex)
            {
                CGS_ASSERT(liIndex >= 0, "liIndex >= 0");
                CGS_ASSERT(liIndex < liNumPlayers, "liIndex < miNumPlayers");

                const InGamePlayerStatusData* lpPlayerStatusData =
                        lPlayerStatusInterface.GetPlayerStatusData(liIndex);

                CGS_ASSERT((lpPlayerStatusData->meActiveRaceCarIndex > E_ACTIVE_RACE_CAR_NONE) &&
                           (lpPlayerStatusData->meActiveRaceCarIndex < ::E_ACTIVE_RACE_CAR_INDEX_COUNT),
                           "( lpPlayerStatusData->meActiveRaceCarIndex > E_ACTIVE_RACE_CAR_INDEX_INVALID ) && ( lpPlayerStatusData->meActiveRaceCarIndex < E_ACTIVE_RACE_CAR_INDEX_COUNT )");

                maMarkedManActiveRaceCarIndex[lpPlayerStatusData->meActiveRaceCarIndex] =
                        static_cast<::EActiveRaceCarIndex>(lpPlayerStatusData->meMarkedManActiveRaceCarIndex);
            }
            return *this;
        }
    } // namespace BrnNetworkModuleIO
} // namespace BrnNetwork
