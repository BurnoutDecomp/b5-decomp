#pragma once

// CgsSceneManager::SceneManagerIO::SceneCoarseQueryQueue -- the coarse-query input
// queue the module IO buffers embed by value.
//
// In BrnRaceCarEntityModuleIO.h, OutputBuffer_PostScene declares
//   typedef InputBuffer_Query::InSmCoarseQueryQueue SceneCoarseQueryQueue;  // :77
// and embeds it BY VALUE (mSceneCoarseQueryQueue, :387). InSmCoarseQueryQueue is
//   typedef CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<16384> ...   (CgsSceneManagerModuleIO.h:247)
// and InCoarseQueryQueue<16384> : public VariableEventQueue<16384,16> adds NO data
// members (only the SphereTest/FrustumTest/VolumeTest enqueue helpers). So this name is
// the real queue type, not a stand-in: sizeof == sizeof(VariableEventQueue<16384,16>)
//   bool mbIsConstructed (+0) + char macData[16384] (+1, byte-aligned, no alignas)
//   + s32 miBufferWritePos + s32 miLength + s32 miFirstEventOffset
//   = 1 + 16384 + 12 -> round to 4 -> 16400 bytes,
// which is the span the sized blob that stood here occupied, so the collapse is
// size-neutral. It gains Construct/Append/AddEvent, which is what lets
// WorldModule::BridgeRaceCarModuleToSceneModule_PostScene merge it into the scene
// query input buffer's own coarse queue.

#include "types.hpp"
#include "GameShared/GameClasses/SceneManager/CgsSceneManagerIO_CoarseQueryQueue.h" // InCoarseQueryQueue<N>

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    typedef InCoarseQueryQueue<16384> SceneCoarseQueryQueue;
}
}
