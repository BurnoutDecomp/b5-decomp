#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/SharedIO/CgsPotentialContact.h"

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>::Construct
//   @ X360 0x822E2800
//   (dossier id "class:CgsSceneManager::SceneManagerIO::PotentialContact,2048>").
//
// Fixed-capacity (2048) event-queue instantiation -- identical shape to the committed
// EventQueue<InAddPotentialContact,1024>::Construct and EventQueue<Contact,16384>::Construct
// siblings. The derived EventQueue<T,2048>::Construct points the BaseEventQueue<T> base at
// its inline maEvents buffer (base is 12 bytes, padded to the 16-byte element alignment ->
// buffer at +0x10, matching the asm's `addi r30, r31, 0x10`), stores the max length
// (2048 == 0x800, the asm's `li r11, 0x800; stw r11, 4(r31)`) and clears the live count
// (`li r10, 0; stw r10, 8(r31)`), after pointing mpEvents at &maEvents (`stw r30, 0(r31)`).
// The lpEventBuffer != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by
// BaseEventQueue<T>::Construct (vacuous here: &maEvents[0] == this + 0x10 != 0; the asm
// `cmplwi cr6,r30,0 / bne loc_822E2844` skips the assert, which the Hex-Rays
// `result == -16` misreads).
//
// This is SceneManagerIO::OutputBuffer's PotentialContact queue (X360 OutputBuffer::Construct
// is one of the 5 callers). Thin explicit instantiation only -- the shared generic bodies
// live in CgsEventQueue.h / CgsBaseEventQueue.h. The element is the already-committed 80-byte
// CgsSceneManager::SceneManagerIO::PotentialContact (SharedIO/CgsPotentialContact.h).
template void CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::PotentialContact, 2048>::Construct();
