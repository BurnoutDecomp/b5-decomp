#include "types.hpp"

// CgsNetwork ranked-matchmaking helpers (part of the class:CgsNetwork
// translation unit).
//
//   AmendRankedContexts               @ 0x828778F0
//   ServerInterfaceGameSearchParamsX360 (GameSearchParamsBase::operator=)
//                                     @ 0x825505A8
//
// Faithful to the X360 asm.

namespace CgsNetwork
{
    // X360 LIVE context id for the "ranked vs unranked" property.
    const u32 KU_RANKED_CONTEXT_ID = 0x800A;   // 32778

    // A single LIVE context entry: an id paired with its value.
    struct RankedContext
    {
        u32 muContextId;   // +0x00
        u32 muValue;       // +0x04
    };

    // ---- AmendRankedContexts @ 0x828778F0 -------------------------------------
    // Adds (or overwrites) the KU_RANKED_CONTEXT_ID entry in the context array.
    // Scans the existing lpiCount entries for the LAST matching id; if absent,
    // appends a fresh slot (incrementing the count). Writes the ranked context id
    // and a value of (lbRanked == 0). Returns lbRanked unchanged (the asm threads
    // r3 straight through).
    char AmendRankedContexts(char lbRanked, s32* lpiCount, RankedContext* lpaContexts)
    {
        const s32 liOldCount = *lpiCount;
        s32       liSlot      = -1;

        if (liOldCount > 0)
        {
            for (s32 li = 0; li < liOldCount; ++li)
            {
                if (lpaContexts[li].muContextId == KU_RANKED_CONTEXT_ID)
                {
                    liSlot = li;
                }
            }
        }

        if (liSlot == -1)
        {
            liSlot   = liOldCount;
            *lpiCount = liOldCount + 1;
        }

        lpaContexts[liSlot].muContextId = KU_RANKED_CONTEXT_ID;
        lpaContexts[liSlot].muValue     = (lbRanked == 0) ? 1u : 0u;
        return lbRanked;
    }

    // ---- ServerInterfaceGameSearchParamsX360 @ 0x825505A8 ---------------------
    // GameSearchParamsBase::operator= on the X360 variant: a flat field-by-field
    // copy of the ~0x6C-byte parameter block from lpSource into lpDest (the asm
    // copies offsets 0x04..0x68; offset 0x14 is a single byte, the rest are 32-bit
    // words). Modelled as a faithful word/byte copy preserving every field. The
    // leading 4 bytes (vptr/typetag at +0x00) are not copied, matching the asm.
    struct GameSearchParamsX360
    {
        u32 muField00;   // +0x00 (not copied -- typically vptr/tag)
        u32 muField04;   // +0x04
        u32 muField08;   // +0x08
        u32 muField0C;   // +0x0C
        u32 muField10;   // +0x10
        u8  mu8Field14;  // +0x14
        u8  mPad15[3];   // +0x15 padding to the next word boundary
        u32 maWords18[20]; // +0x18..0x64 (twenty 32-bit words copied pairwise)
        u32 muField68;   // +0x68
    };

    GameSearchParamsX360* ServerInterfaceGameSearchParamsX360(GameSearchParamsX360* lpDest,
                                                              const GameSearchParamsX360* lpSource)
    {
        lpDest->muField04  = lpSource->muField04;
        lpDest->muField08  = lpSource->muField08;
        lpDest->muField0C  = lpSource->muField0C;
        lpDest->muField10  = lpSource->muField10;
        lpDest->mu8Field14 = lpSource->mu8Field14;
        for (s32 li = 0; li < 20; ++li)
        {
            lpDest->maWords18[li] = lpSource->maWords18[li];
        }
        lpDest->muField68 = lpSource->muField68;
        return lpDest;
    }
}
