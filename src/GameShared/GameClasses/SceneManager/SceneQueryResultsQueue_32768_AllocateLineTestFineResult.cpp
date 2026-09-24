#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_SceneQueryResultsQueue.h"

// Explicit instantiation of the X360-emitted out-of-line member
//   CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::AddLineTestFineResult
//   @ 0x828C4A08   (DWARF CgsSceneManagerModuleIO.h:434)
// (ledger id: class:CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueu). The template body
// is inline in the header above; this TU forces the SizeBytes==32768 specialisation the
// fine-line-test producer path (SceneManagerModule::ProcessLineTestFine) uses. Mirrors the
// committed sibling SceneQueryResultsQueue_32768_AddTriangleCollisionLineTestNearestResult.cpp.
// The FILE keeps its old name: the member was spelled AllocateLineTestFineResult (inferred from
// its caller while the IDA symbol was truncated) until 2026-09-24, when the DWARF spelling landed.
template CgsSceneManager::LineTestIntersection*
CgsSceneManager::SceneManagerIO::OutSceneQueryResultsQueue<32768>::AddLineTestFineResult(
    CgsSceneManager::SceneQueryId, s32);
