#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationModuleIO.h"

// CgsPhysics::PhysicsSimulationIO::OutputBuffer::Destruct
// Reconstructed from BURNOUT_X360_ARTIST.XEX @ 0x828A5F38.
//
// Behaviour: zero the two scalar controls and drop every live event from the four embedded
// output queues (a bare miLength = 0 per queue -- BaseEventQueue<T>::Clear), then chain to
// the base IOBuffer destructor. The X360 stores are, in order:
//     *(this + 4)      = 0.0f   -> mfTimeStepUsed
//     *(this + 131144) = 0      -> mDriveSpyQueue.miLength        (+0x20040 + 8)
//     *(this + 128056) = 0      -> mJointSpyQueue.miLength        (+0x1F430 + 8)
//     *(this + 38440)  = 0      -> mContactSpyQueue.miLength      (+0x9620  + 8)
//     *(this + 24)     = 0      -> mUpdateRigidBodyQueue.miLength (+0x10    + 8)
//     *(this + 8)      = 0      -> muMaxIterationsUsed
//
// ⚠️⚠️ REWRITTEN 2026-08-06 (the spy wave). The previous TU re-declared IOBuffer and
// OutputBuffer as LOCAL SLICE COPIES and poked the six CONSOLE byte offsets through
// reinterpret_cast -- an [[odr-forks-link-silently]] shape that was byte-correct only while
// every member before +0x20040 kept its console size. This wave widens OutUpdateRigidBody's
// payload to the full host rw::physics::RigidBody (pointer lanes widen on x64), which moves
// all three trailing queues; the raw offsets would have silently scribbled into the middle
// of mContactSpyQueue's event storage. Named members against the real header are the fix;
// each named store is one of the attested console stores above.
//
// ⚠️ Destruct is deliberately NOT declared in the committed OutputBuffer (no in-scope caller
// dispatches it; the console reaches it through CgsModule teardown). Defining it requires a
// declaration, so it is now declared through the one the header grows below -- see the
// header's Destruct note.

namespace CgsPhysics
{
    namespace PhysicsSimulationIO
    {
        void OutputBuffer::Destruct()
        {
            mfTimeStepUsed = 0.0f;                 // stfs 4(this)
            mDriveSpyQueue.Clear();                // stw 0, 0x20048(this) -- console offset
            mJointSpyQueue.Clear();                // stw 0, 0x1F438(this)
            mContactSpyQueue.Clear();              // stw 0, 0x9628(this)
            mUpdateRigidBodyQueue.Clear();         // stw 0, 0x18(this)
            muMaxIterationsUsed = 0;               // stw 0, 8(this)
            CgsModule::IOBuffer::Destruct();       // the base-chain tail call
        }
    }
}
