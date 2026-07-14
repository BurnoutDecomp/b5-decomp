#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"

// Explicit instantiation of the X360-emitted out-of-line member
//   CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::AllocateLineTestFineResult
//   @ 0x828C4A08
// (ledger id: class:CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueu). The template body
// is inline in the header above; this TU forces the SizeBytes==32768 specialisation the
// fine-line-test producer path (SceneManagerModule::ProcessLineTestFine) uses. Mirrors the
// committed sibling SceneQueryResultsQueue_32768_AddTriangleCollisionLineTestNearestResult.cpp.
template void*
CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::AllocateLineTestFineResult(s32, s32);
