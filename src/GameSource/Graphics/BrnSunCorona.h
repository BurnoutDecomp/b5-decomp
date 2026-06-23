#pragma once

#include "types.hpp"
#include "rw/rwcore_structs.h"   // rw::Resource, rw::IResourceAllocator

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnSunCorona::Construct @ 0x824009B0  (EXECUTED in the boot trace)
//   BrnSunCorona::Destruct  @ 0x823F8298
//
// The sun-corona graphics object: it owns the vertex descriptor and the four shader programs
// (occlusion vertex/pixel + flare vertex/pixel) used to draw the screen-space sun flare, plus the
// corona's tunable parameters (sizes, brightness, screen position) and visibility flags. Construct
// allocates the descriptor + programs through the renderengine resource pipeline and seeds the
// parameter fields; Destruct releases the descriptor and frees the object's resource block through
// the allocator. Owned by BrnRendererModule (mSunCorona); built/torn by BrnRendererModule::Construct
// / ::Destruct.
//
// Layout is the DWARF struct (GameSource/Graphics/BrnSunCorona.h) verified against the X360 asm
// store offsets. Member accesses are BY NAME. The corona's hand-vectorised
// ComputeSunPositionOnScreen / RenderOccludedFlare / GenerateOcclusionBuffer paths are NOT part of
// this TU's two in-scope functions and are intentionally not declared here.

namespace renderengine
{
    // The renderengine resource objects the corona holds. Forward-declared as opaque here so this
    // header does not pull in (and clash between) the two committed renderengine::VertexDescriptor
    // homes (pc/gcm/renderengine/VertexDescriptor.h and renderstates.h); the corona only ever holds
    // these as pointers. The construction call surface is declared locally in BrnSunCorona.cpp.
    struct VertexDescriptorObject;   // the created vertex-format object (mpVertexDescriptor)
    struct ProgramBufferObject;      // a compiled shader program wrapper
    struct ProgramVariableHandleObject;  // a named shader-constant handle
}

struct BrnSunCorona
{
    // 0x824009B0 -- allocate the vertex descriptor + four shader programs through the renderengine
    // resource pipeline, look up the two pixel-shader constant handles, and seed the corona's tunable
    // parameters. `lpAllocator` is the rw resource allocator the module supplies.
    u32 Construct(rw::IResourceAllocator* lpAllocator);

    // 0x823F8298 -- release the vertex descriptor and free the object's resource block through the
    // allocator.
    void Destruct();

    // ---- layout (DWARF GameSource/Graphics/BrnSunCorona.h, verified vs the X360 store offsets) ----
    rw::IResourceAllocator*                  mpAllocator;                                  // +0x00
    rw::Resource                             mVertexDescriptorResource;                    // +0x04 (X360 word 1..5)
    renderengine::VertexDescriptorObject*    mpVertexDescriptor;                           // +0x18
    renderengine::ProgramBufferObject*       mpOcclusionVertexProgram;                     // +0x1C
    renderengine::ProgramBufferObject*       mpOcclusionPixelProgram;                      // +0x20
    renderengine::ProgramVariableHandleObject* mOcclusionPixelVariableHandleUvStartOffset; // +0x24
    renderengine::ProgramBufferObject*       mpFlareVertexProgram;                         // +0x28
    renderengine::ProgramBufferObject*       mpFlarePixelProgram;                          // +0x2C
    renderengine::ProgramVariableHandleObject* mFlarePixelVariableHandleColourAndPower;    // +0x30
    f32                                      mfSunVectorYMultiplier;                       // +0x34 (0.5)
    f32                                      mfOcclusionSize;                              // +0x38 (2.0)
    f32                                      mfSunFlarePow;                                // +0x3C (2.0)
    f32                                      mfSunBrightness;                              // +0x40 (0.30000001)
    f32                                      mfSunSize;                                    // +0x44 (0.30000001)
    f32                                      mfXPos;                                       // +0x48 (0.0)
    f32                                      mfYPos;                                       // +0x4C (0.0)
    bool                                     mbVisible;                                    // +0x50 (false)
    bool                                     mbRenderSunCorona;                            // +0x51 (true)
};
