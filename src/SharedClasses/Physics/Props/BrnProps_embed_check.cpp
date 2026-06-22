// Embed/ODR sanity check for the BrnPhysics-Props group: pulls in both group homes so
// the compile gate exercises the header set together (PropEntityID layout + the prop
// queue/resource-ptr facades). No runtime entry points — translation-unit only.
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

namespace
{
    // Touch the PropEntityID surface so the accessors are name-checked at compile time.
    void BrnProps_EmbedCheck()
    {
        BrnWorld::PropEntityID lId(static_cast<u32>(0x03000000u)); // owner=PROP
        lId.SetEntityIndex(0);
        lId.SetPartIndex(0);
        (void)lId.GetEntityIndex();
        (void)lId.GetPartIndex();
        (void)lId.GetValue();
    }
    void (*const KpfnBrnPropsEmbedCheck)() = &BrnProps_EmbedCheck;
}
