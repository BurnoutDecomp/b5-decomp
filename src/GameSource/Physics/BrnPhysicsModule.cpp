#include "GameSource/Physics/BrnPhysicsModule.h"

// BrnPhysics::PhysicsModule::PhysicsModule  (default constructor, X360 @0x827E5400)
// [boot-trace: EXECUTED in the goal trace -- the only function of this TU that ran.]
//
// X360 store-for-store (asm spine):
//   *this              = &off_820CE500          // base ModuleSingleBuffered vtable
//   RWMutex(this+0x10,  0, 1)                   // base mInputMutex  (initial owner 0, lock-count 1)
//   RWMutex(this+0x118, 0, 1)                   // base mOutputMutex
//   *this              = off_820D12E8           // derived PhysicsModule vtable
//   PhysicsSimulationModule::ctor(this+0x230)   // embedded mSimulationModule
//   VehicleManager::ctor(this+0x4AA0)           // embedded mVehicleManager
//   *(this+0x63630)    = off_820CDF60           // contained-interface vtable
//   *(this+0x63684 +0) = 0                      // intrusive-list head int 0
//   *(this+0x63684 +4) = 0                      // intrusive-list head int 1
//   *(this+0x63684 +8) = 0                      // intrusive-list head int 2
//   *(this+0x63684 +0) = 0                      // (asm re-stores head int 0; reproduced)
//   *(this+0x63684 +C) = this+0x63684           // list next  -> self (empty circular list)
//   *(this+0x63684+10) = this+0x63684           // list prev  -> self
//   *(this+0x63684+14) = this+0x63684           // list iter  -> self
//   *(this+0x63684+18) = 0                      // list count 0
//
// In human C++ the two vtable writes + the two RWMutex constructions are the
// ModuleSingleBuffered base sub-object's own construction (it owns those mutexes),
// and the embedded mSimulationModule / mVehicleManager are constructed via the
// implicit member-construction order. This body therefore only has to reproduce
// the trailing contained-interface stamp + empty-list initialisation, which lands
// on the asm-sized placeholder member by name.
//
// mSimulationModule -- RESOLVED 2026-08-03 (was FLAG/DEFERRED). The X360 ctor
// chains CgsPhysics::PhysicsSimulationModule::PhysicsSimulationModule @0x827DF1E0
// on the embedded member at +0x230. That class now has a real, byte-closing
// layout (CgsPhysicsSimulationModule.h), so the placeholder is folded and the
// chain is the implicit member construction: it really does run at boot now, and
// it really does seed mBodyData's 200 rw::physics::Inertia records and
// mJointData's 36 JointLimits.
//
// FLAG -- still DEFERRED: mVehicleManager's own constructor (X360 @0x827E4D58) is
// deferred in its home TU (BrnVehicleManagerPlayerStats.cpp) because that class is
// padding-modelled; the embed therefore default-constructs trivially here. It
// folds in when that pass lands.

namespace BrnPhysics
{
    PhysicsModule::PhysicsModule()
    {
        // Base (ModuleSingleBuffered: vtable + mInputMutex/mOutputMutex) and the
        // embedded mVehicleManager are constructed automatically before this body.

        // Contained-interface sub-object: stamp its vtable, then empty-initialise
        // the intrusive list so head == tail == iter point back at the list itself
        // (an empty circular list) and the count is zero.
        // FLAG: vtable symbol off_820CDF60 is not reconstructed; the slot is left
        // null here (the owning interface type / its real vtable land with that
        // type's pass). Every other store is reproduced exactly.
        mContainedList.mpVTable    = nullptr;   // X360 stamps off_820CDF60 here

        mContainedList.miListHead0 = 0;
        mContainedList.miListHead1 = 0;
        mContainedList.miListHead2 = 0;

        void* lpListSelf           = &mContainedList.miListHead0;
        mContainedList.mpListNext  = lpListSelf;
        mContainedList.mpListPrev  = lpListSelf;
        mContainedList.mpListIter  = lpListSelf;
        mContainedList.miListCount = 0;
    }
}
