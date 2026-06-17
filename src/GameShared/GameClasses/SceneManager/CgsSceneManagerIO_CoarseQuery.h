#pragma once

// MINIMAL SLICE for the RaceCarEntityModuleIO IO-buffer unlock; full layout
// reconstructed by SceneCoarseQueryQueue's own TU (DWARF home
// CgsSceneManagerIO_CoarseQuery.h). Size 16400 (DWARF-derived: see below).
//
// In BrnRaceCarEntityModuleIO.h, OutputBuffer_PostScene declares
//   typedef InputBuffer_Query::InSmCoarseQueryQueue SceneCoarseQueryQueue;  // :77
// and embeds it BY VALUE (mSceneCoarseQueryQueue, :387). InSmCoarseQueryQueue is
//   typedef CgsSceneManager::SceneManagerIO::InCoarseQueryQueue<16384> ...   (CgsSceneManagerModuleIO.h:247)
// and InCoarseQueryQueue<16384> : public VariableEventQueue<16384,16>
// (CgsSceneManagerIO_CoarseQuery.h:90) adds NO data members (only SphereTest/
// FrustumTest/VolumeTest methods). So sizeof == sizeof(VariableEventQueue<16384,16>):
//   bool mbIsConstructed (+0) + char macData[16384] (+1, byte-aligned, no alignas)
//   + s32 miBufferWritePos + s32 miLength + s32 miFirstEventOffset
//   = 1 + 16384 + 12 -> round to 4 -> 16400 bytes.
// It carries Vector3/Matrix44-bearing query events, so the slice is alignas(16).
// Per the stub rules the real generic VariableEventQueue<> + the coarse-query event
// element cascade are intentionally NOT pulled in; a complete sized blob unlocks the
// buffer (the IO header only takes &member). Full layout belongs to this type's own TU.

#include "types.hpp"

namespace CgsSceneManager
{
namespace SceneManagerIO
{
    struct alignas(16) SceneCoarseQueryQueue
    {
        unsigned char maReserved[16400];
    };
}
}
