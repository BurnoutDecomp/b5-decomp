#include "GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h"

// ===========================================================================
// BrnPhysics::Props::PropOutputInterface bodies, reconstructed store-for-store
// from BURNOUT_X360_ARTIST.XEX (Construct @0x825A9658, AppendUpdatedProps
// @0x826153A0). Only these two accessors are owned by this slice; the remaining
// DWARF-declared methods are bodied in their own TUs.
// ===========================================================================

namespace BrnPhysics
{
namespace Props
{
    // X360 0x825A9658. Wires the four embedded fixed-capacity event queues to their inline
    // storage (each derived EventQueue<T,200>::Construct points its BaseEventQueue<T> at
    // maEvents, stores the capacity and clears the count). The asm then redundantly re-clears
    // the AddRigidBody / RemoveRigidBody / UpdatedProps live counts (but not the notification
    // queue's); reproduced faithfully via Clear().
    void PropOutputInterface::Construct()
    {
        mAddRigidBodyQueue.Construct();            // this+0
        mRemoveRigidBodyQueue.Construct();         // this+38416 (0x9610)
        mUpdatedProps.Construct();                 // this+41632 (0xA2A0)
        mPropUpdateNotificationQueue.Construct();  // this+64048 (0xFA30)

        // Redundant explicit count resets emitted by the X360 build (stw 0 @ +8 / +0xA2A8 / +0x9618).
        mAddRigidBodyQueue.Clear();
        mUpdatedProps.Clear();
        mRemoveRigidBodyQueue.Clear();
    }

    // X360 0x826153A0. Drains the queued UpdatePropEvent records: first merges them onto
    // this interface's own mUpdatedProps queue (for downstream consumers), then translates as
    // many of them as the notification queue has free space into PropUpdateNotification slots
    // (position row of the affine transform + the two velocity rows + entity id + type id).
    void PropOutputInterface::AppendUpdatedProps(const UpdatePropEventQueue* lpUpdatedProps)
    {
        mUpdatedProps.Append(*lpUpdatedProps);

        // Clamp the source count to the space still free in the notification queue
        // (miMaxLength - miLength), matching the asm's `subf r11 = maxlen - len` + `ble` clamp.
        s32 liCount = lpUpdatedProps->GetLength();
        const s32 liFree = mPropUpdateNotificationQueue.GetMaxLength()
                         - mPropUpdateNotificationQueue.GetLength();
        if (liCount > liFree)
        {
            liCount = liFree;
        }

        for (s32 liIndex = 0; liIndex < liCount; ++liIndex)
        {
            PropUpdateNotification& lrDest = mPropUpdateNotificationQueue.AddEvent();
            const UpdatePropEvent&  lrSrc  = lpUpdatedProps->GetEvent(liIndex);

            lrDest.mPosition        = lrSrc.mTransform.wAxis;   // src+0x30 -> dest+0x00
            lrDest.mLinearVelocity  = lrSrc.mLinearVelocity;    // src+0x40 -> dest+0x10
            lrDest.mAngularVelocity = lrSrc.mAngularVelocity;   // src+0x50 -> dest+0x20
            lrDest.mEntityId        = lrSrc.mEntityId;          // src+0x60 -> dest+0x30
            lrDest.mi16TypeId       = lrSrc.miTypeId;           // src+0x66 -> dest+0x34
        }
    }
}
}
