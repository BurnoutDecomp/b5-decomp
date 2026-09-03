#pragma once

// ============================================================================
// GameSource/Effects/Particles/Native/BrnTrailRender.h
//
// BrnParticle::Native::TrailRenderer -- turns the active skid TrailEmitters of
// one trail type into one immediate-mode triangle strip of BrnGraphics::
// SkidVertex through the BrnGraphics::Im3dSkidsRenderer the particle module
// owns, and pushes the type's start/end colours + the frame's view-projection
// into that renderer's three program constants.
//
// X360 ARTIST bodies (BrnTrailRender.cpp):
//   BeginRender @0x82284468   Render @0x82295930
//   Construct / Update / EndRender are inlined into their callers (see the .cpp)
//
// DWARF AUTHORITY (DecFIGS BrnTrailRender.h:44-80):
//   Im3dSkidsRenderer* mpRenderer (:78)  f32 mfCurrentTime (:79)
//   Matrix44 mViewProjectionMatrix (:80)
//   Construct(HeapMalloc*, Im3dSkidsRenderer*) (:50)  BeginRender(Texture*) (:54)
//   EndRender() (:57)  Render(TrailEmitter**, int32, TrailParams*, int8, float) (:65)
//   Update(float, Matrix44) (:70)
// Console layout (pinned by the ParticleModule::Prepare / BuildLionVertexBuffers
// stores at TrailSystem +102560 / +102564 / +102576): +0x00 mpRenderer,
// +0x04 mfCurrentTime, +0x10 mViewProjectionMatrix (16-aligned), sizeof 0x50.
// ============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"   // Matrix44 (rw::math::vpu)

namespace renderengine { class Texture; }
namespace CgsMemory    { class HeapMalloc; }
namespace BrnGraphics  { struct Im3dSkidsRenderer; }

namespace BrnParticle
{
namespace Native
{
    struct TrailEmitter;
    struct TrailParams;

    struct TrailRenderer
    {
        // Inlined into ParticleModule::Prepare @0x8229BEA0 (`*(this+141232) = this+37392`):
        // remember the skids renderer. The heap is not used by the X360 body.
        void Construct(CgsMemory::HeapMalloc* lpHeapMalloc, BrnGraphics::Im3dSkidsRenderer* lpRenderer);

        // @0x82284468. Start the skids renderer's immediate-mode batch, push the frame's
        // view-projection through gWorldViewProj, bind the trail render states
        // (depth-test-no-write / standard alpha blend / cull-none) and the trail texture.
        void BeginRender(renderengine::Texture* lpTexture);

        // Inlined into TrailSystem::Render @0x82295C58: the ImRenderer<V>::EndRendering
        // fold (assert active renderer == ours, clear the module static).
        void EndRender();

        // @0x82295930. Emit every emitter with >= 2 segments as a strip of quads.
        void Render(TrailEmitter** lppEmitter, s32 lnEmitterCount, TrailParams* lpParams,
                    s8 lu8TrailTypeID, const f32 lfWhiteLevel);

        // Inlined into ParticleModule::BuildLionVertexBuffers @0x8228AC20 (stores at
        // TrailSystem +102564 / +102576): latch the frame time + view-projection.
        void Update(f32 lfCurrentTime, Matrix44::InParam lViewProjMatrix);

        BrnGraphics::Im3dSkidsRenderer* mpRenderer;              // +0x00
        f32                             mfCurrentTime;           // +0x04
        Matrix44                        mViewProjectionMatrix;   // +0x10

        // [DIAG] NOT IN THE X360 BINARY. Running totals of what Render() actually handed to
        // the immediate-mode renderer, so the [trailpass] line in TrailSystem::Render can
        // state the DRAW side of the claim instead of only the LAY side. Statics, not
        // members: nothing about the console object changes. DELETE-WHEN-STABLE.
        static u32 guProbeVertices;
        static u32 guProbeDraws;
    };
}
}
