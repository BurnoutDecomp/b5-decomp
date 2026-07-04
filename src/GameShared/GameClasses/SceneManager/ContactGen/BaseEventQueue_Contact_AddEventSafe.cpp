#include "GameShared/GameClasses/Module/CgsBaseEventQueue.h"          // BaseEventQueue<T>::AddEventSafe (inline generic)
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerContact.h"  // CgsSceneManager::Contact (64-byte element, already committed)

// CgsModule::BaseEventQueue<CgsSceneManager::Contact>::AddEventSafe
//   @ X360 0x828B8D08 (dossier id "class:CgsSceneManager::Contact>").
//
// Thin explicit instantiation: the generic BaseEventQueue<T>::AddEventSafe body is
// already inline in CgsBaseEventQueue.h. The element type CgsSceneManager::Contact is
// ALREADY COMMITTED in CgsSceneManagerContact.h and is the same element the sibling
// EventQueue_Contact_16384_Construct.cpp uses -- include that header (NOT
// CgsSceneManagerTypes.h) so the whole EventQueue<Contact,16384> family resolves to
// ONE Contact definition.
//
// The X360 body matches the generic store-for-store (asm at 0x828B8D08):
//   * mpEvents != NULL tripwire (CgsBaseEventQueue.h:331; `lwz r11,0(r31)` mpEvents@+0;
//     `bne` skips the BeginAssert/FireAssert/EndAssert triplet) -- CGS_ASSERT collapse;
//   * bounds-gated full check: `lwz r11,8(r31)` (miLength@+8) vs `lwz r10,4(r31)`
//     (miMaxLength@+4); `bge` => return 0 WITHOUT appending when miLength >= miMaxLength
//     (the gated variant, unlike the AddEvent sibling that appends unconditionally);
//   * otherwise copies the 64-byte element to mpEvents[miLength] at a 64-byte stride
//     (`slwi r10,r11,6` == miLength*64; ctr=8 ld/std 64-bit block moves == 64 bytes),
//     bumps miLength (`lwz r11,8(r31); addi r11,r11,1; stw r11,8(r31)`) and returns 1
//     (`li r3,1`).
// The 64-byte stride == sizeof(CgsSceneManager::Contact) (committed alignas(16), 0x40).
// Member offsets (asm): mpEvents @+0, miMaxLength @+4, miLength @+8 -- the generic
// BaseEventQueue<T> layout. Called by CgsSceneManager::OverlapCullingModule::DoPairQuery
// to push each narrow-phase contact point into the culler's output contact queue.
template bool
CgsModule::BaseEventQueue<CgsSceneManager::Contact>::AddEventSafe(
    const CgsSceneManager::Contact&);
