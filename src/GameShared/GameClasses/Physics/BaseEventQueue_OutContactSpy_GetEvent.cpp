#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::GetEvent(s32) (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutContactSpy (112-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::GetEvent(s32)  @ 0x8259D3C8
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked (non-const) element accessor body is
// already inline in CgsBaseEventQueue.h (T& GetEvent(s32), decl :290); this is the thin explicit
// instantiation. Called by BrnPhysics::PhysicsModule::Update to drain the output contact-spy queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:292), liIndex < GetLength() (:294)
// and liIndex >= 0 (:295) -- the non-const-overload assert-line triple -- then returns
// &mpEvents[liIndex]. The element index math (mulli r11,r29,0x70 == liIndex*112) gives a 112-byte
// stride == sizeof(OutContactSpy).
template CgsPhysics::PhysicsSimulationIO::OutContactSpy&
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutContactSpy>::GetEvent(s32);
