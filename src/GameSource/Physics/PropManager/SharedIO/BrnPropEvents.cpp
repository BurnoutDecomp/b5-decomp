#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"

// ===========================================================================
// BrnPhysics::Props::PropUpdateNotification out-of-line accessor reconstructed
// from BURNOUT_X360_ARTIST.XEX. (The Construct/GetPosition/GetLinearVelocity/
// GetAngularVelocity accessors are their own ledger TUs; only GetEntityId is
// owned by class:BrnPhysics::Props::PropUpdateNotification.)
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // GetEntityId @ 0x826944E0
    //   lbz r11, 0x30(this)   -> the owner byte of mEntityId (bits[24..31] on BE)
    //   cmplwi r11, 3         -> assert owner == E_ENTITYTYPE_PROP (baked
    //                            "mEntityId.GetOwner() == E_ENTITYTYPE_PROP", BrnPropEntityID.h:278)
    //   lwz r11, 0x30(this)   -> return mEntityId (the full PropEntityID word) by value
    // The owner-byte tripwire is the inlined PropEntityID::AssertIsProp (committed).
    BrnWorld::PropEntityID PropUpdateNotification::GetEntityId() const
    {
        mEntityId.AssertIsProp();
        return mEntityId;
    }
}
}
