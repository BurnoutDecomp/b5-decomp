#include "types.hpp"
#include "GameShared/GameClasses/Network/ServerInterface/DirtySock/X360/CgsServerInterfaceGameParamsX360.h"  // RankedContext, AmendRankedContexts

// CgsNetwork ranked-matchmaking helper (part of CgsNetworkUnity).
//
//   AmendRankedContexts               @ 0x828778F0
//
// Faithful to the X360 asm. RankedContext and the declaration come from the X360
// game-parameters header (one definition for the unity TU). ARTIST 0x825505A8, named
// CgsNetwork::ServerInterfaceGameSearchParamsX360, is that class's compiler-generated
// copy assignment, emitted where the BrnNetwork parameters copy it; it has no source here.

namespace CgsNetwork
{
    // X360 LIVE context id for the "ranked vs unranked" property.
    const u32 KU_RANKED_CONTEXT_ID = 0x800A;   // 32778

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
}
