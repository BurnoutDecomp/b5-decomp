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

        // Burnout game-mode discriminant (E_GAME_MODE_*); declared-only.
        s32  GameMode() const;
        void SetPreviousGameMode(s32 liGameMode);
        void SetGameMode(s32 liGameMode);

    protected:
        u8 maBlock150[0x20];                                        // +0x150
        u8 maPlayerSlots[KI_GAMEPARAMS_PLAYER_SLOTS *
                         KI_GAMEPARAMS_PLAYER_SLOT_STRIDE];         // +0x170
    };
}

#endif // BRN_NETWORK_GAME_PARAMS_H
