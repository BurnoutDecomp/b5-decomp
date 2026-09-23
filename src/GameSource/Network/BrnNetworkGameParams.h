#ifndef BRN_NETWORK_GAME_PARAMS_H
#define BRN_NETWORK_GAME_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.h"
#include "GameSource/Network/Parameters/BrnNetworkPlayerParamsClass.h"   // BrnNetwork::PlayerParams (maPlayerParams)

// ===========================================================================
// BrnNetwork::GameParams
//   Home: GameSource/Network/BrnNetworkGameParams.{h,cpp}
//
// The Burnout game-side game-parameter object: the game leaf of the DirtySock
// game-params hierarchy. CORRECTED BASE (was ServerInterfaceGameParamsBase):
//
//   CgsNetwork::ServerInterfaceGameParamsBase   (CgsServerInterfaceGameParams.h)
//       <- CgsNetwork::ServerInterfaceGameParamsX360   (platform leaf, ends +0x150)
//           <- BrnNetwork::GameParams                  (this type)
//
// proven by BrnNetwork::GameParams::operator= @ 0x82566A00, which chains to
// CgsNetwork::ServerInterfaceGameParamsX360::operator= @ 0x82558DB0 (matches the
// committed CgsServerInterfaceGameParamsX360.cpp) before copying its own leaf
// storage -- so GameParams IS an X360 leaf. The X360 leaf's last member
// (miPropertyCount) sits at +0x14C, so the base ends exactly at +0x150.
//
// LEAF LAYOUT (console offsets; base ends at +0x150):
//   +0x150  mGameData (GameData, 0x20)   the replicated payload GetData hands out.
//                                        muUser1 (+0x15C) is the packed game-params
//                                        word; muUser3 (+0x164) holds the locality
//                                        SetLocality stores.
//   +0x170  maPlayerParams[7]            seven PlayerParams (0xA0 each): the stack
//                                        constructions store the PlayerParams vtable
//                                        into each slot, the deleting destructor walks
//                                        them, and operator= copies each slot's
//                                        [+4 .. +0xA0) member span.
//   object end = 0x150 + 0x20 + 7*0xA0 = 0x5D0.
//
// The packed search-data word is the inherited muCustomFlags (+0xE8). Every accessor
// reaches both words by name, so the host layout (wider vptrs) stays correct.
// ===========================================================================

namespace BrnNetwork
{
    // Seven per-player PlayerParams records at +0x170 (console stride 0xA0 ==
    // sizeof(PlayerParams) there; the deleting-destructor walk and operator= loop).
    const s32 KI_GAMEPARAMS_PLAYER_SLOTS       = 7;
    const s32 KI_GAMEPARAMS_PLAYER_SLOT_STRIDE = 0xA0;

    // ---- bit-field placements (all base-bit/num-bit pairs are read directly off the
    // extrwi/insrwi operands of the accessor asm). Two packed dwords:
    //   * KU_BRN_GAMESEARCHDATA_* -- the game-search-data word (muCustomFlags, +0xE8).
    //   * KI_GAME_PARAMS_*        -- the leaf game-params word (mGameData.muUser1, +0x15C).
    const u32 KU_BRN_GAMESEARCHDATA_SECURITY_NUM_BITS            = 2;
    const u32 KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_BASE_BIT         = 17;
    const u32 KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_BASE_BIT = 23;
    const u32 KU_BRN_GAMESEARCHDATA_VEC_LEVEL_BASE_BIT          = 13;
    const u32 KU_BRN_GAMESEARCHDATA_VEC_LEVEL_NUM_BITS           = 4;
    const u32 KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_BASE_BIT     = 30;
    const u32 KU_BRN_GAMESEARCHDATA_VEHICLE_CHOICE_NUM_BITS      = 1;
    const u32 KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_BASE_BIT     = 27;
    const u32 KU_BRN_GAMESEARCHDATA_RUNNER_CRASHES_NUM_BITS      = 3;
    const u32 KI_GAME_PARAMS_ROUNDS_BASE_BIT                    = 11;
    const u32 KI_GAME_PARAMS_ROUNDS_NUM_BITS                     = 4;
    const u32 KI_GAME_PARAMS_TIME_LIMIT_BASE_BIT               = 15;
    const u32 KI_GAME_PARAMS_TIME_LIMIT_NUM_BITS                = 5;
    const u32 KI_GAME_PARAMS_NETWORK_VERSION_BASE_BIT          = 0;
    const u32 KI_GAME_PARAMS_NETWORK_VERSION_NUM_BITS           = 3;
    const u32 KI_GAME_PARAMS_VEHICLE_LEVEL_BASE_BIT            = 3;
    const u32 KI_GAME_PARAMS_VEHICLE_LEVEL_NUM_BITS             = 4;
    const u32 KI_GAME_PARAMS_INFINITE_BOOST_BASE_BIT           = 7;
    const u32 KI_GAME_PARAMS_INFINITE_BOOST_NUM_BITS            = 1;
    const u32 KI_GAME_PARAMS_LAPS_BASE_BIT                     = 8;
    const u32 KI_GAME_PARAMS_LAPS_NUM_BITS                      = 3;
    const u32 KU_BRN_GAMESEARCHDATA_SECURITY_BASE_BIT           = 0;
    const u32 KU_BRN_GAMESEARCHDATA_SKILL_BASE_BIT              = 2;
    const u32 KU_BRN_GAMESEARCHDATA_SKILL_NUM_BITS               = 6;
    const u32 KU_BRN_GAMESEARCHDATA_TRAFFIC_ON_NUM_BITS          = 1;
    const u32 KU_BRN_GAMESEARCHDATA_TRAFFIC_CHECKING_ON_NUM_BITS = 1;
    const u32 KU_BRN_GAMESEARCHDATA_BOOST_TYPE_BASE_BIT         = 24;
    const u32 KU_BRN_GAMESEARCHDATA_BOOST_TYPE_NUM_BITS          = 3;
    const u32 KU_BRN_GAMESEARCHDATA_GAMEMODE_BASE_BIT           = 8;
    const u32 KU_BRN_GAMESEARCHDATA_GAMEMODE_NUM_BITS            = 5;
    const u32 KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_BASE_BIT  = 18;
    const u32 KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_NUM_BITS   = 5;

    // Game modes are packed biased by this value (the E_GAME_MODE_* enum base).
    const s32 KI_GAME_PARAMS_GAME_MODE_BIAS                     = 10;

    class GameParams : public CgsNetwork::ServerInterfaceGameParamsX360
    {
    public:
        // The replicated payload (+0x150, 0x20 bytes).
        struct GameData
        {
            s32 miGameLevel;     // +0x00
            s32 miSkillLevel;    // +0x04
            s32 miRulesSet;      // +0x08
            u32 muUser1;         // +0x0C  packed game-params word
            u32 muUser2;         // +0x10
            u32 muUser3;         // +0x14  locality
            u32 muUser4;         // +0x18
            u32 muUser5;         // +0x1C
        };

        // X360 @ 0x82567338 (`scalar deleting destructor'). Reinstalls the seven
        // embedded sub-object vtables + the primary vtable as the object dies.
        // Implicitly virtual via the base's virtual destructor.
        virtual ~GameParams();

        // Member-wise copy. X360 @ 0x82566A00: chain to the X360 base operator=
        // then copy the leaf's +0x150 block and the seven +0x170 slot records.
        GameParams& operator=(const GameParams& lrOther);

        // The ServerInterfaceStructureInterface pure-virtuals the game leaf provides
        // (replicated-payload pattern + data accessors); declared here so GameParams
        // is instantiable. Bodies homed in the behavioural game-params TU.
        // (GetPatternLength is not overridden: the console vtable keeps the base default.)
        virtual const char* GetPattern() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

        // Reset to the prepared/default state (declared-only). Overrides the X360
        // base's virtual `bool Prepare()` (CgsServerInterfaceGameParamsX360.h:66); the
        // corrected base makes this an override, so the signature matches the base's
        // bool return rather than the earlier standalone void form.
        virtual bool Prepare() override;

        // Serialise the platform game record, then let each of the seven player-param
        // records read its own player entry of the lobby record.
        virtual void SerialiseFromGame(const void* lpGame) override;

        // Burnout game-mode discriminant (E_GAME_MODE_*).
        s32  GameMode() const;                               // @0x82584098

        // Inlined at every console reader (the found-game list and the game-parameter
        // event): previous game mode from search-data bits 18..22 (+10), network
        // version from the low three bits of the game-params word.
        s32  PreviousGameMode() const
        {
            return static_cast<s32>((muCustomFlags >> KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_BASE_BIT)
                                    & ((1u << KU_BRN_GAMESEARCHDATA_PREVIOUS_GAMEMODE_NUM_BITS) - 1u))
                 + KI_GAME_PARAMS_GAME_MODE_BIAS;
        }
        s32  NetworkVersion() const
        {
            return static_cast<s32>((mGameData.muUser1 >> KI_GAME_PARAMS_NETWORK_VERSION_BASE_BIT)
                                    & ((1u << KI_GAME_PARAMS_NETWORK_VERSION_NUM_BITS) - 1u));
        }

        // Store the host's locality (+0x164). The create / modify game paths call it with
        // the local player info's locality.
        void SetLocality(u32 luLocality);
        void SetPreviousGameMode(s32 liGameMode);            // @0x82583FD0
        void SetGameMode(s32 liGameMode);                    // @0x8258A790

        // --- Game-search-data (+0xE8) packed-flag accessors. Non-virtual, X360-attested. ---
        s32  BoostType() const;                              // @0x825840C8  bits 24..26
        s32  Security() const;                               // @0x82584088  bits 0..1
        s32  NumRunnerCrashes() const;                       // @0x825840E8  bits 27..29
        s32  VehicleChoice() const;                          // @0x825840D8  bit  30
        s32  VehicleLevelLimit() const;                      // @0x82584048  bits 13..16
        bool IsTrafficOn() const;                            // @0x825840A8  bit  17
        bool IsTrafficCheckingOn() const;                    // @0x825840B8  bit  23
        void SetBoostType(s32 liBoostType);                  // @0x82584018  bits 24..26
        void SetNumRunnerCrashes(s32 liNumRunnerCrashes);    // @0x82584038  bits 27..29
        void SetVehicleChoice(s32 liVehicleChoice);          // @0x82584028  bit  30 (DWARF EVehicleChoice)
        void SetVehicleLevelLimit(s32 liLevelLimit);         // @0x82583DB8  bits 13..16
        void SetSkillLevel(u32 luSkillLevel);                // bits 2..7
        void SetSecurity(s32 leSecurity);                    // bits 0..1 (BrnNetwork::EBrnGameSecurity)
        void SetTrafficOn(bool lbTrafficOn);                 // bit  17
        void SetTrafficCheckingOn(bool lbTrafficCheckingOn); // bit  23

        // --- Leaf game-params (+0x15C) packed-flag accessors. Non-virtual, X360-attested. ---
        s32  NumberRounds() const;                           // @0x82584078  bits 11..14
        s32  TimeLimit() const;                              // @0x82584068  bits 15..19
        bool InfiniteBoost() const;                          // @0x82584058  bit  7
        void SetInfiniteBoost(bool lbInfiniteBoost);         // @0x82583EA8  bit  7
        void SetNumberRounds(s32 liNumRounds);               // @0x82583F48  bits 11..14
        void SetTimeLimit(s32 liTimeLimit);                  // bits 15..19
        void SetRagerVehicleLevelLimit(s32 liLevelLimit);    // @0x82583E30  bits 3..6

    protected:
        GameData     mGameData;                                     // +0x150
        PlayerParams maPlayerParams[KI_GAMEPARAMS_PLAYER_SLOTS];    // +0x170 (0xA0 each)
    };
}

#endif // BRN_NETWORK_GAME_PARAMS_H
