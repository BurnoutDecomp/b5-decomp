#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"                  // BaseEventQueue<T>::GetEvent(s32) const (inline generic)
#include "GameShared/GameClasses/Physics/CgsPhysicsSimulationIO_Events.h"     // CgsPhysics::PhysicsSimulationIO::InAddJoint (192-byte element)

// CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>::GetEvent(s32) const  @ 0x8289D968
// Reconstructed from BURNOUT_X360_ARTIST.XEX. The checked const element accessor body is already
// inline in CgsBaseEventQueue.h (const T& GetEvent(s32) const, decl :270); this is the thin explicit
// instantiation. Called by CgsPhysics::PhysicsSimulationModule::ProcessAddJointQueue to walk the
// add-joint request queue.
//
// The X360 body asserts mpEvents != NULL (CgsBaseEventQueue.h:272), liIndex < GetLength() (:274)
// and liIndex >= 0 (:275) -- the CONST-overload assert-line triple -- then returns &mpEvents[liIndex].
// The element index math (slwi r11,r30,1; add r11,r30,r11 == liIndex*3; slwi r11,r11,6 == *64 ==
// liIndex*192) gives a 192-byte (0xC0) stride == sizeof(InAddJoint) (Construct offset map
// (196208-189280-16)/36 == 192).
template const CgsPhysics::PhysicsSimulationIO::InAddJoint&
CgsModule::BaseEventQueue<CgsPhysics::PhysicsSimulationIO::InAddJoint>::GetEvent(s32) const;
