#include "GameShared/GameClasses/Module/CgsEventQueue.h"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_EventAddForCollision.h"

// CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision, 1536>::Construct
//   @ X360 0x822E1E60 (dossier:
//   "class:CgsSceneManager::SceneManagerIO::InEventAddForCollision,1536>::Constru").
//
// Fixed-capacity (1536) event-queue instantiation: the derived EventQueue<T,1536>::Construct
// points the BaseEventQueue<T> base at its inline maEvents buffer (base is 12 bytes, padded
// to the 16-byte element alignment -> the buffer lives at +0x10, exactly the asm's
// `addi r30,r31,0x10`), stores the max length (1536 == 0x600, the asm's `li r11,0x600;
// stw r11,4(r31)`) and clears the live count (`stw r10(=0),8(r31)`). The lpEventBuffer
// != NULL tripwire (CgsBaseEventQueue.h:160) is reproduced by BaseEventQueue<T>::Construct.
// This queue is InSceneUpdateInterface::InAddForCollisionQueue
// (DWARF CgsSceneManagerIO_SceneUpdate.h:266); its Construct is reached from
// InSceneUpdateInterface::Construct.
//
// Thin explicit instantiation only (the shared generic bodies live in CgsEventQueue.h /
// CgsBaseEventQueue.h). Just Construct is in this TU's ledger; the queue's other members
// stay un-instantiated to match the X360 ledger.
template void CgsModule::EventQueue<CgsSceneManager::SceneManagerIO::InEventAddForCollision, 1536>::Construct();
