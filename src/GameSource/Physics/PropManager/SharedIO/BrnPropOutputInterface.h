// ============================================================================
// b5-decomp/src/GameSource/Physics/PropManager/SharedIO/BrnPropOutputInterface.h
//
// BrnPhysics::Props::PropOutputInterface -- the prop-manager's per-frame OUTPUT
// interface embedded in BrnPhysics::PhysicsModuleIO::OutputBuffer. It owns four
// fixed-capacity (200) event queues that the prop physics fills each frame and that
// downstream consumers drain.
//
// Layout, member names/types and method shapes are DWARF-AUTHORITATIVE
// (references/DecFIGS/dwarfdump/.../BrnPropOutputInterface.h):
//   +0        InputBuffer::InAddRigidBodyQueue    mAddRigidBodyQueue           :73
//   +38416    InputBuffer::InRemoveRigidBodyQueue mRemoveRigidBodyQueue        :76
//   +41632    UpdatePropEventQueue                mUpdatedProps                :79
//   +64048    PropUpdateNotificationQueue         mPropUpdateNotificationQueue :81
//
// The DWARF spells the first two member types as the nested typedefs
// CgsPhysics::PhysicsSimulationIO::InputBuffer::InAddRigidBodyQueue /
// ::InRemoveRigidBodyQueue, both == EventQueue<InAddRigidBody/InRemoveRigidBody, 200>
// (CgsPhysicsSimulationModuleIO.h:73/172). We spell the underlying instantiation
// directly (identical type, identical layout).
//
// Offsets are X360-attested by Construct @ 0x825A9658 (this+0 / this+0x9610 /
// this+0xA2A0 / this+0xFA30) and by AppendUpdatedProps @ 0x826153A0 (notif @+0xFA30,
// notif.miLength @+0xFA38, notif.miMaxLength @+0xFA34). They fall out automatically
// from the EventQueue<T,200> sizes (16-byte base + 200*sizeof(T)); element sizes are
// pinned in the committed CgsPhysicsSimulationIO_Events.h (InAddRigidBody=192B,
// InRemoveRigidBody=16B) and BrnPropEvents.h (UpdatePropEvent=112B,
// PropUpdateNotification=64B):
//   sizeof(EventQueue<InAddRigidBody(192),200>)        = 16 + 38400 = 38416
//   sizeof(EventQueue<InRemoveRigidBody(16),200>)      = 16 +  3200 =  3216  (-> +41632)
//   sizeof(EventQueue<UpdatePropEvent(112),200>)       = 16 + 22400 = 22416  (-> +64048)
//   sizeof(EventQueue<PropUpdateNotification(64),200>) = 16 + 12800 = 12816
#pragma once

#include "types.hpp"

#include "GameShared/GameClasses/Module/CgsEventQueue.h"                     // CgsModule::EventQueue<T,N>
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"    // InAddRigidBody / InRemoveRigidBody
#include "GameSource/Physics/PropManager/SharedIO/BrnPropEvents.h"           // UpdatePropEvent / PropUpdateNotification

namespace BrnPhysics
{
namespace Props
{
    class PropOutputInterface
    {
    public:
        // Queue typedefs (DWARF names these InputBuffer::InAddRigidBodyQueue etc.; the
        // underlying instantiation is identical).
        typedef CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InAddRigidBody, 200>    InAddRigidBodyQueue;
        typedef CgsModule::EventQueue<CgsPhysics::PhysicsSimulationIO::InRemoveRigidBody, 200> InRemoveRigidBodyQueue;
        typedef CgsModule::EventQueue<UpdatePropEvent, 200>                                    UpdatePropEventQueue;
        typedef CgsModule::EventQueue<PropUpdateNotification, 200>                             PropUpdateNotificationQueue;

        // X360 0x825A9658.
        void Construct();                                                  // :39

        // X360 0x826153A0: merge lpUpdatedProps onto mUpdatedProps, then translate as many as
        // fit into PropUpdateNotification slots. DWARF signature (:59) takes the
        // UpdatePropEventQueue* (== const InputBuffer's UpdatePropEventQueue*).
        void AppendUpdatedProps(const UpdatePropEventQueue* lpUpdatedProps); // :59

        // Remaining DWARF-attested methods, bodied in their own TUs; not owned by this slice.
        bool Prepare();                                                    // :43
        bool Release();                                                    // :47
        void Destruct();                                                   // :51
        void Append(const PropOutputInterface* lpSource);                  // :55
        void Clear();                                                      // :62
        const UpdatePropEventQueue&        GetUpdatedProps() const;        // :65
        const PropUpdateNotificationQueue& GetUpdatePropNotifications() const; // :68

    private:
        InAddRigidBodyQueue         mAddRigidBodyQueue;            // +0      :73
        InRemoveRigidBodyQueue      mRemoveRigidBodyQueue;         // +38416  :76
        UpdatePropEventQueue        mUpdatedProps;                 // +41632  :79
        PropUpdateNotificationQueue mPropUpdateNotificationQueue;  // +64048  :81
    };
}
}
