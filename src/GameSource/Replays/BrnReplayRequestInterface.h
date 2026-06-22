#pragma once

// Canonical home for BrnReplays::ReplayIO::RequestInterface.
// DWARF home: GameSource/Replays/BrnReplayRequestInterface.h:46.
//
// A replay-IO request interface: a fixed array of BaseSerialiser* slots, one per
// serialiser id. Mirrors the committed BrnReplays::ReplayIO::StatusInterface small
// struct pattern (see BrnReplayStatusInterface.h). Each per-frame IO buffer holds
// one of these; Append() merges another interface's registered serialisers into
// this one (OR of the pointer slots) and RegisterSerialiser() installs a serialiser
// into the slot keyed by its id.
//
// LAYOUT (X360 asm authoritative):
//   @0x00  BaseSerialiser* mapSerialisers[KI_MAX_SERIALISERS]   (KI_MAX_SERIALISERS == 11)
//   sizeof == 11 * 4 == 0x2C
//
// X360-vs-DWARF discrepancy (X360 asm wins per project rule):
//   * Array size / id count: the leaked PS3 source (BrnReplayShared.h) declares
//     KI_MAX_SERIALISERS == E_ID_COUNT == 5 and mapSerialisers[5]. The X360 build
//     uses ELEVEN slots:
//       - Append @0x823A6868 initialises its loop counter to 11 (`li r21, 0xB`) and
//         predecrements once per slot, so it ORs exactly 11 pointer slots.
//       - RegisterSerialiser @0x821F34A0 range-asserts the serialiser id with
//         `cmpwi r11, 0xB` (the "meId < E_ID_COUNT" guard), i.e. E_ID_COUNT == 11.
//     The X360 ESerialiserId enum therefore grew six ids beyond the five named in
//     the Feb-2007 leak (E_ID_RACECAR_ENTITY..E_ID_GAME_MODULE). The six extra ids
//     are NOT named in any recovered source; modelling the array as 11 slots is the
//     asm fact, the missing id names are HONEST gaps (see KI_MAX_SERIALISERS below).

#include "types.hpp"

namespace BrnReplays
{
    // Minimal owning forward-slice of BaseSerialiser for this TU. BaseSerialiser
    // has its own DWARF home (GameSource/Replays/BrnReplayBaseSerialiser.h) that is
    // not yet reconstructed in-tree; RequestInterface only stores its pointer and
    // reads its id via GetId(), so a minimal interface suffices here. When the full
    // BaseSerialiser TU lands it should own/replace this slice (do NOT fork it --
    // grow the canonical home and include it). The id accessor reads meId at +0x28
    // (X360 asm: `lwz r11, 0x28(r31)` in RegisterSerialiser).
    class BaseSerialiser;

    namespace ReplayIO
    {
        // X360 slot count. The X360 Append loop runs 11 iterations and
        // RegisterSerialiser asserts id < 11; the leaked PS3 value is 5. X360 wins.
        // (Six of the eleven ids are unnamed X360 additions -- honest gap.)
        static const s32 KI_MAX_SERIALISERS = 11;

        // DWARF: BrnReplayRequestInterface.h:46
        struct RequestInterface
        {
            // ---- layout ----
            BaseSerialiser* mapSerialisers[KI_MAX_SERIALISERS]; // @0x00

            // ---- ledger methods owned by this TU ----

            // Append @ 0x823A6868 -- merges another request interface's registered
            // serialiser slots into this one: for each slot, asserts that the two
            // interfaces do not both have a serialiser registered ("only one of each
            // type"), then ORs the two slot pointers together.
            void Append(const RequestInterface* lpOther);

            // RegisterSerialiser @ 0x821F34A0 -- asserts the serialiser's id is in
            // range and that its slot is empty ("already registered"), then stores
            // the serialiser into mapSerialisers[id].
            void RegisterSerialiser(BaseSerialiser* lpSerialiser);
        };
    }
}
