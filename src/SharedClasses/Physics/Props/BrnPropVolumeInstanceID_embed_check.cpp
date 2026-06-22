// Tiny embed check: instantiate PropVolumeInstanceID and exercise every accessor so the
// header + the out-of-line bodies are forced through the compiler together.
#include "SharedClasses/Physics/Props/BrnPropEntityID.h"

namespace
{
    void EmbedCheck()
    {
        using BrnWorld::PropEntityID;
        using BrnWorld::PropVolumeInstanceID;

        PropEntityID lEntity(0x03000000u); // owner byte == E_ENTITYTYPE_PROP
        PropVolumeInstanceID lId;
        lId.Set(lEntity, 7);
        lId.SetPropEntityId(lEntity);
        lId.SetEntityIndex(static_cast<u16>(5));
        lId.SetPartIndex(2u);
        PropEntityID lOut = lId.GetPropEntityID();
        u32 luIndex = lId.GetEntityIndex();
        (void)lOut;
        (void)luIndex;
    }
}

void BrnPropVolumeInstanceID_EmbedCheckEntryPoint()
{
    EmbedCheck();
}
