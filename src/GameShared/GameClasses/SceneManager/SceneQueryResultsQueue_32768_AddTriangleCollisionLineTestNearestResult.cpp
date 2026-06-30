#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"

// Explicit instantiation of the X360-emitted out-of-line member
//   CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::
//     AddTriangleCollisionLineTestNearestResult  @ 0x828D1E90
// (ledger id: class:CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::
//  AddTriangleCollisionLineTestNea). The template body is inline in the header above; this TU
// forces the SizeBytes==32768 specialisation the boot path uses.
template bool
CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::AddTriangleCollisionLineTestNearestResult(
    s32, s64, const void*, bool);
