#pragma once

// ============================================================================
// SDKs/Packages/Lion/Final/eauk_lion/Dev/LionRuntime/include/ParticleRender/LionBatch.h
//
// LionBatch / LionBatchArray -- the Lion (eauk_lion) particle draw-batch record and the
// fixed-capacity array of them that cParticleRender accumulates during a frame and then
// replays in cParticleRender::Dispatch.
//
// One LionBatch is a single immediate-mode draw: a contiguous run of vertices in the
// shared particle vertex buffer (start vertex + vertex count) tagged with the material
// they were emitted with. EmitterRender/EmitterCubeRender push a batch per material run
// (LionBatchArray::Append); Dispatch walks the array, binds each batch's material and
// issues the DrawVertices.
//
// LAYOUT AUTHORITY (X360 ARTIST asm, cParticleRender::Dispatch @ 0x82911E98):
//   Dispatch reads a batch through Array<LionBatch,512>::GetItem and dereferences
//     Item[0] -> start vertex   (D3DDevice_DrawVertices StartVertex)
//     Item[1] -> vertex count   (asserted > 0; D3DDevice_DrawVertices VertexCount)
//     Item[2] -> material       (RenderGroupBegin / GetVertexStride argument)
//   giving a 3-word guest record { u32 startVertex; u32 vertexCount; cParticleMaterial*
//   material; } (guest sizeof 12; the material pointer widens on the 64-bit host, so the
//   record is accessed BY NAME, never by absolute offset).
//
// The backing container is the shared CgsContainers Array<T,N> (CgsArray.h): its inline
// element buffer is followed by a live-count word initialised to the -1 "Array used before
// Construct/Clear was called" sentinel, exactly the length field Dispatch reads at guest
// +0x1800 (512 * 12) via GetLength().
// ============================================================================

#include "types.hpp"
#include "GameShared/GameClasses/Containers/CgsArray.h"   // Array<T,N> backing container
#include "GameSource/Effects/Particles/EffectsVertexBuffer.h"  // EffectsVertexBufferBatch (the base)

class cParticleMaterial;   // ParticleMaterial.h -- the material a batch was emitted with

// ----------------------------------------------------------------------------
// LionBatch -- one immediate-mode particle draw record.
//
// ⭐ IT DERIVES FROM EffectsVertexBufferBatch, AND THAT IS WHAT THE DWARF SAYS (corrected
// 2026-09-05). cParticleRender::EmitterRender's local list (DecFIGS ParticleRender.cpp:491)
// declares ONE record -- `LionBatch lBatch` -- and hands that same object to
// EffectsVertexBufferLocked::BeginBatch / ::EndBatch, whose parameter is
// EffectsVertexBufferBatch&. The X360 confirms it byte for byte: EndBatch writes the run's
// start/count into the record's words 0 and 1 (`stw` at +0x00/+0x04), then EmitterRender writes
// the material into word 2 (`stw r21, 0x3D00+var_3C98(r1)` @0x82913C58) and Appends the whole
// thing. So the first two words ARE the vertex-buffer batch, and this record is that record plus
// the material -- not two independent structs that happen to overlap.
// ----------------------------------------------------------------------------
struct LionBatch : public EffectsVertexBufferBatch
{
public:
    const cParticleMaterial* GetMaterial() const { return mpMaterial; }

    // muStartVertex / muVertexCount come from EffectsVertexBufferBatch (guest +0x00 / +0x04).
    const cParticleMaterial* mpMaterial; // guest +0x08 -- material the run was emitted with
};

// The frame's batch list. Capacity 512 matches the X360 Array<LionBatch,512u> instantiation
// (the length word Dispatch reads sits at guest +0x1800 == 512 * 12).
typedef Array<LionBatch, 512> LionBatchArray;
