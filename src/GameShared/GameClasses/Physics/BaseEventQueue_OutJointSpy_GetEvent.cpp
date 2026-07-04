#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::GetEvent(s32) (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::OutJointSpy (48-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy>::GetEvent(s32)  @ 0x8259D518
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked (non-const) element accessor body is
// already inline in CgsBaseEventQueue.h (T& GetEvent(s32), decl :290); this is the thin explicit
// instantiation. Called by BrnPhysics::PhysicsModule::Update to drain the output joint-spy queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:292), liIndex < GetLength() (:294)
// and liIndex >= 0 (:295) -- the non-const-overload assert-line triple -- then returns
// &mpEvents[liIndex]. The element index math (slwi r11,r30,1; add r11,r30,r11 == liIndex*3;
// slwi r11,r11,4 == *16 == liIndex*48) gives a 48-byte (0x30) stride == sizeof(OutJointSpy).
template CgsPhysics::PhysicsSimulationIO::OutJointSpy&
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::OutJointSpy>::GetEvent(s32);
