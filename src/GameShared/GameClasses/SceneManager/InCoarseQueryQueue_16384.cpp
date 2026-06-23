#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h"

// Explicit per-method instantiation of
//   CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<16384>::FrustumTestVp
//   @ BURNOUT_X360_ARTIST.XEX 0x82746770
// (reached from WorldModule::GenerateFrustumQueries).
//
// The shared generic body lives in CgsSceneManagerIO_CoarseQueryQueue.h. This is a
// thin per-method instantiation so the sibling enqueue helpers (SphereTest /
// FrustumTest / VolumeTest) stay un-instantiated -- only FrustumTestVp is this
// instance's ledger function. The underlying VariableEventQueue<16384,16> generic
// is instantiated by its own TU (VariableEventQueue_16384_16.cpp).

template void CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<16384>::FrustumTestVp(
    CgsSceneManager::SceneQueryId,
    CgsSceneManager::SceneManagerIO::EntityTypeFlags,
    const Vector4*,
    const Matrix44&,
    u32);
