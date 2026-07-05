#ifndef CGS_SERVER_INTERFACE_GAME_PARAMS_X360_H
#define CGS_SERVER_INTERFACE_GAME_PARAMS_X360_H

#include "types.hpp"
#include "../Components/CgsServerInterfaceGameParams.h"   // ServerInterfaceGameParamsBase

// ===========================================================================
// CgsNetwork::ServerInterfaceGameParamsX360
//   Home: GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/
//         CgsServerInterfaceGameParamsX360.{h,cpp}
//
// The X360 (and PC-remaster) platform leaf of ServerInterfaceGameParamsBase. On
// top of the shared game-parameter block it carries the Xbox LIVE matchmaking
// "context" array -- the (contextId, value) pairs the title publishes to the
// session so the matchmaker can filter on ranked/unranked (and any additional
// game-mode) properties. Its overrides forward the (de)serialisation to the base
// but first seed / maintain that context array.
//
// LAYOUT (X360 asm):
//   the base ends at +0xF8 (muRandomSeed at +0xF4); the X360 leaf adds:
//     +0xF8  maRankedContexts[10]   (RankedContext, 8 bytes each = 80 bytes)
//     +0x148 miContextCount         (s32) -- live entry count in maRankedContexts
//     +0x14C miPropertyCount        (s32) -- second counter, left 0 by Prepare
//
//   Prepare @ 0x82877810 -- chain to base, then zero both counters, XMemSet the
//   80-byte context array to 0, and append two default contexts:
//       maRankedContexts[0] = { 0x800A, 0 }   (KU_RANKED_CONTEXT_ID)
//       maRankedContexts[1] = { 0x800B, 0 }
//     bumping miContextCount to 2. (The stores use ((count+31)*8) as the byte
//     offset of the id and (count*8 + 0xFC) for the value -- i.e. the array
//     lives at +0xF8 with an 8-byte stride.)
//
//   SetRankedGame @ 0x825411B0 -- set/clear the ranked bit (0x400) in the base
//   muGameFlags, then call CgsNetwork::AmendRankedContexts to (re)publish the
//   0x800A context with value (lbRanked == 0), passing &miContextCount and the
//   context array. (ASM passes r4 = this+0x148 (count), r5 = this+0xF8 (array).)
// ===========================================================================

namespace CgsNetwork
{
    // A single Xbox LIVE matchmaking context entry (id paired with its value).
    // NOTE: the canonical definition currently lives (headerless) in
    // CgsNetworkRankedContexts.cpp alongside AmendRankedContexts; this mirrors it
    // so the X360 leaf can size its array. The consolidator should reconcile the
    // two (ideally by promoting RankedContext + AmendRankedContexts into a shared
    // CgsNetworkRankedContexts.h that both TUs include).
    struct RankedContext
    {
        u32 muContextId;   // +0x00
        u32 muValue;       // +0x04
    };

    // Set (or overwrite) the ranked context entry (id 0x800A) in lpaContexts,
    // appending a fresh slot when absent. Returns lbRanked. Bodied in
    // CgsNetworkRankedContexts.cpp @ 0x828778F0.
    char AmendRankedContexts(char lbRanked, s32* lpiCount, RankedContext* lpaContexts);

    // Capacity of the LIVE context array cleared by Prepare (80 bytes / 8).
    const s32 KI_GAMEPARAMS_MAX_CONTEXTS = 10;

    struct ServerInterfaceGameParamsX360 : public ServerInterfaceGameParamsBase
    {
    public:
        // @ X360 0x82877810 -- chain to base Prepare; on success seed the two
        // default LIVE contexts. Returns false if the base Prepare failed.
        virtual bool Prepare();

        // @ X360 0x828778E8 -- pure forward to the base (the X360 leaf adds no
        // per-game serialisation work here; the override exists only to sit in
        // the leaf's vtable slot).
        virtual void SerialiseFromGame(const void* lpGame);

        // @ X360 0x828778E0 -- pure forward to the base.
        virtual void SerialiseToString(char* lpcRecord, s32 liRecLen) const;

        // @ X360 0x825411B0 -- set/clear the ranked flag and re-publish the
        // 0x800A LIVE context accordingly.
        virtual void SetRankedGame(bool lbRanked) /* override */;

        // Member-wise copy (asm @ 0x82558DB0): chain to the base operator= then
        // copy the leaf's context array + both counters, in that order.
        ServerInterfaceGameParamsX360& operator=(const ServerInterfaceGameParamsX360& lrOther);

        // ADDITIVE GROW (flagged by the ServerInterfaceGamesX360 group): read accessors
        // so the games component can read the LIVE context array + slot counts by name.
        // CreateGame / JoinGame (X360 0x8288C178 / 0x8288C268) drive XUserSetContext and
        // the ConnAPI 'slot' control off these; layout unchanged.
        const RankedContext& GetContext(s32 li) const { return maRankedContexts[li]; }
        s32 GetContextCount() const     { return miContextCount; }
        s32 GetContextUserIndex() const { return miPropertyCount; }   // +0x14C used as XUserSetContext user index
        s32 GetNumPublicSlots() const   { return miNumPublicSlots; }  // base +224
        s32 GetNumPrivateSlots() const  { return miNumPrivateSlots; } // base +228

    protected:
        RankedContext maRankedContexts[KI_GAMEPARAMS_MAX_CONTEXTS]; // +0xF8
        s32           miContextCount;                              // +0x148
        s32           miPropertyCount;                             // +0x14C
    };
}

#endif // CGS_SERVER_INTERFACE_GAME_PARAMS_X360_H
