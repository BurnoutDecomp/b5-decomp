#include "types.hpp"
#include "GameShared/GameClasses/Core/CgsAssert.h"

// CgsNetwork connection-record lookup helpers (part of the class:CgsNetwork
// translation unit; CgsPlayersConnectionManager.cpp:1138 / :1167 in the leak).
//
//   GetConnectionEntryForPlayer (PlayersConnectionM)        @ 0x82874890
//   FindConnectionEntryForPlayer (PlayersConnectionManager) @ 0x828749F8
//
// Both walk the manager's fixed array of 7 connection records (stride 0x204 =
// 516 bytes) looking for the record whose leading playerID field matches
// liPlayerID, and return a pointer to that record.
//
// LAYOUT FLAG: the asm bases the record array at THIS + 0x14 (offset 20), but the
// committed home CgsPlayersConnectionManager.h models a leading mPad0[252] before
// maConnections[7] (record stride 0x204 matches). The +20 vs +252 base offset is
// a genuine discrepancy between this TU's asm and the committed header. Rather
// than redefine/fork the committed class (HARD RULE) or silently grow it with an
// unverified offset, these two helpers are bodied against a faithful asm-derived
// local view (records at +0x14, stride 0x204) and the divergence is reported.
// The committed header's record-array base should be reconciled by whoever owns
// that class; this TU does not touch CgsPlayersConnectionManager.h.

namespace CgsNetwork
{
    const s32 KI_INVALID_PLAYER_ID  = -1;
    const s32 KI_CONNECTION_COUNT   = 7;
    const s32 KI_CONNECTION_STRIDE  = 0x204;   // 516 bytes per record
    const s32 KI_CONNECTION_ARRAY_OFFSET = 0x14;  // record array base within the manager

    namespace
    {
        // Address of connection record [liIndex]'s leading playerID field.
        inline s32* ConnectionEntry(void* lpManager, s32 liIndex)
        {
            u8* lpBase = static_cast<u8*>(lpManager) + KI_CONNECTION_ARRAY_OFFSET;
            return reinterpret_cast<s32*>(lpBase + KI_CONNECTION_STRIDE * liIndex);
        }
    }

    // ---- GetConnectionEntryForPlayer @ 0x82874890 -----------------------------
    // Returns the record pointer; on a miss the X360 build streams a diagnostic
    // ("Could not find player <id> conenction entry") into the assert message
    // buffer and fires the assert, then returns null. The string-stream assembly
    // (StrStreamBase::AppendFormat into CgsDev::Assert::gpcMessageBuffer) is an
    // un-homed assert-buffer side channel; FLAGGED -- it is reduced here to the
    // observable assert it builds toward.
    s32* GetConnectionEntryForPlayer(void* lpManager, s32 liPlayerID)
    {
        CGS_ASSERT(liPlayerID != KI_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        for (s32 li = 0; li < KI_CONNECTION_COUNT; ++li)
        {
            s32* lpEntry = ConnectionEntry(lpManager, li);
            if (*lpEntry == liPlayerID)
            {
                return lpEntry;
            }
        }

        // Miss: assert (the original additionally logged the player id).
        CGS_ASSERT(false, "Could not find player connection entry");
        return nullptr;
    }

    // ---- FindConnectionEntryForPlayer @ 0x828749F8 ----------------------------
    // Same scan but the not-found path only asserts "lpEntry" and returns null;
    // the early-exit at index 7 falls through into the same assert.
    s32* FindConnectionEntryForPlayer(void* lpManager, s32 liPlayerID)
    {
        CGS_ASSERT(liPlayerID != KI_INVALID_PLAYER_ID, "lPlayerID != K_INVALID_PLAYER_ID");

        s32* lpEntry = nullptr;
        s32  liIndex = 0;
        s32* lpScan  = ConnectionEntry(lpManager, 0);
        while (*lpScan != liPlayerID)
        {
            ++liIndex;
            lpScan = ConnectionEntry(lpManager, liIndex);
            if (liIndex >= KI_CONNECTION_COUNT)
            {
                CGS_ASSERT(false, "lpEntry");
                return nullptr;
            }
        }
        lpEntry = ConnectionEntry(lpManager, liIndex);
        CGS_ASSERT(lpEntry != nullptr, "lpEntry");
        return lpEntry;
    }
}
