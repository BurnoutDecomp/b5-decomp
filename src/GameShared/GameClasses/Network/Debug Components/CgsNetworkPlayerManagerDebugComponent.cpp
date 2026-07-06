#include "GameShared/GameClasses/Network/Debug Components/CgsNetworkPlayerManagerDebugComponent.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   CgsNetwork::PlayerManagerDebugComponent::_QSortMessageAndMax  @ 0x8287F370
//
// Only the self-contained qsort comparator is bodied this wave. OnActivate / DrawRow depend
// on subsystem hooks not yet grounded in-tree; DrawBar / RenderHUD did not pass verification.

namespace CgsNetwork
{
    // qsort comparator: descending order by miMaxBytes (larger first).
    // X360 0x8287F370 -- reads the int32 field at +4 (miMaxBytes) of each MessageTypeAndMax.
    //   lwz r11,4(r3); lwz r10,4(r4); cmpw cr6,r11,r10 (SIGNED)
    //   ble ->L1; else return -1        (r11 > r10  -> -1)
    //   L1: li r3,1; bltlr cr6           (r11 < r10  ->  1)
    //       li r3,0; blr                 (r11 == r10 ->  0)
    int PlayerManagerDebugComponent::_QSortMessageAndMax(const void* lpA, const void* lpB)
    {
        const s32 liMaxA = static_cast<const MessageTypeAndMax*>(lpA)->miMaxBytes;
        const s32 liMaxB = static_cast<const MessageTypeAndMax*>(lpB)->miMaxBytes;

        if (liMaxA <= liMaxB)
        {
            return (liMaxA < liMaxB) ? 1 : 0;
        }
        return -1;
    }
}
