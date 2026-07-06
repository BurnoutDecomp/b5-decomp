#ifndef BRN_NETWORK_GAME_PARAMS_H
#define BRN_NETWORK_GAME_PARAMS_H

#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.h"

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
// LEAF LAYOUT (X360 asm; base ends at +0x150):
//   +0x150  maBlock150[0x20]           32-byte (8-word) block, copied word-wise
//                                        by operator=. Field layout unattested.
//   +0x170  maPlayerSlots[7 * 0xA0]     seven 0xA0-byte polymorphic sub-object
//                                        records (each carries the shared
//                                        ServerInterfaceStructureInterface base
//                                        vtable off_8207C88C at its +0). The
//                                        deleting destructor @ 0x82567338 walks
//                                        these seven slots (this+0x170..+0x530)
//                                        resetting each vtable; operator= copies
//                                        each slot's live [+4 .. +0xA0) span
//                                        (only the +0 vtable slot is skipped).
//   object end = 0x150 + 0x20 + 7*0xA0 = 0x5D0 (== the destructor's this+0x5D0
//   walk base -> object size 1488 bytes).
//
// FLAGGED: the leaf field NAMES/inner layout of maBlock150 and each maPlayerSlots
// record are NOT attested by asm or (X360) DWARF, so they are modelled as opaque,
// correctly-sized byte storage and reached by raw byte offset in operator= (same
// convention as BrnNetwork::GameResults). Only the sizes/offsets/copy-order are
// load-bearing. The mode-surface accessors (Prepare / GameMode / SetGameMode /
// SetPreviousGameMode) and the ServerInterfaceStructureInterface overrides remain
// declared-only; their bodies land with the full behavioural GameParams TU.
//
// NOTE on absolute offsets: these are the X360 32-bit-pointer layout. On a 64-bit
// host the inherited vptr(s)/pointers widen, so the byte offsets are NOT reproduced
// and are intentionally NOT static_asserted. Sizes/order are pinned by name.
// ===========================================================================

namespace BrnNetwork
{
    // Seven per-player game-param sub-records at +0x170 (stride 0xA0). Count and
    // stride are X360-attested (deleting-destructor walk + operator= loop); the
    // per-record field layout is not, so the storage stays opaque.
    const s32 KI_GAMEPARAMS_PLAYER_SLOTS       = 7;
    const s32 KI_GAMEPARAMS_PLAYER_SLOT_STRIDE = 0xA0;

    // Raw byte offset (from `this`) of the leaf game-params packed flag word: it lives
    // inside maBlock150 at +0x0C (== object +0x15C). Read/written by raw offset because
    // maBlock150's inner field layout is unattested (same convention as operator=).
    const s32 KI_GAMEPARAMS_LEAF_FLAGS_OFFSET  = 0x15C;

    // ---- X360-DWARF-attested bit-field placements (all base-bit/num-bit pairs are read
    // directly off the extrwi/insrwi operands of the accessor asm). Two packed dwords:
    //   * KU_BRN_GAMESEARCHDATA_* -- the game-search-data word (raw offset +0xE8, which is
    //     the inherited protected base member muCustomFlags).
    //   * KI_GAME_PARAMS_*        -- the leaf game-params word (raw offset +0x15C, maBlock150+0xC).
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

    class GameParams : public CgsNetwork::ServerInterfaceGameParamsX360
    {
    public:
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
        virtual const char* GetPattern() const override;
        virtual s32         GetPatternLength() const override;
        virtual u32         GetDataSize() const override;
        virtual void*       GetData() override;
        virtual const void* GetData() const override;

        // Reset to the prepared/default state (declared-only). Overrides the X360
        // base's virtual `bool Prepare()` (CgsServerInterfaceGameParamsX360.h:66); the
        // corrected base makes this an override, so the signature matches the base's
        // bool return rather than the earlier standalone void form.
        virtual bool Prepare() override;

        // Burnout game-mode discriminant (E_GAME_MODE_*).
        s32  GameMode() const;                               // @0x82584098
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

        // --- Leaf game-params (+0x15C) packed-flag accessors. Non-virtual, X360-attested. ---
        s32  NumberRounds() const;                           // @0x82584078  bits 11..14
        s32  TimeLimit() const;                              // @0x82584068  bits 15..19
        bool InfiniteBoost() const;                          // @0x82584058  bit  7
        void SetInfiniteBoost(bool lbInfiniteBoost);         // @0x82583EA8  bit  7
        void SetNumberRounds(s32 liNumRounds);               // @0x82583F48  bits 11..14
        void SetRagerVehicleLevelLimit(s32 liLevelLimit);    // @0x82583E30  bits 3..6

    private:
        // Packed bit words reached by raw byte offset (unattested inner layout, same
        // convention as operator=). search-data word @ +0xE8 (== inherited muCustomFlags);
        // game-params word @ +0x15C (== maBlock150 + 0xC).
        u32&       SearchDataWord()
        { return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(this) + 0xE8); }
        const u32& SearchDataWord() const
        { return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(this) + 0xE8); }
        u32&       GameParamsWord()
        { return *reinterpret_cast<u32*>(reinterpret_cast<u8*>(this) + 0x15C); }
        const u32& GameParamsWord() const
        { return *reinterpret_cast<const u32*>(reinterpret_cast<const u8*>(this) + 0x15C); }
        u32&       GameParamFlags()       { return GameParamsWord(); }
        const u32& GameParamFlags() const { return GameParamsWord(); }

    protected:
        u8 maBlock150[0x20];                                        // +0x150
        u8 maPlayerSlots[KI_GAMEPARAMS_PLAYER_SLOTS *
                         KI_GAMEPARAMS_PLAYER_SLOT_STRIDE];         // +0x170
    };
}

#endif // BRN_NETWORK_GAME_PARAMS_H
