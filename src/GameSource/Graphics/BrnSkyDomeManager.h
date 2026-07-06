// BrnSkyDomeManager.h  -- reconstructed from DecFIGS DWARF (BrnSkyDomeManager.h) and gated
// against the X360 image (Prepare@0x82408DF0 arg offsets: main VB/IB/Draw @ this+0/+4/+8,
// cube VB/IB/Draw @ this+0x18/+0x1C/+0x20; RaySphereDistance stores horizon@+0x30,
// zenith@+0x34). DrawIndexedParameters is the 16-byte 4xu32 struct already committed in
// GameShared/.../Dispatch/renderablemesh.h. IndexBuffer is renderengine::IndexBuffer
// (pc/gcm/renderengine/IndexBuffer.h). Member byte offsets are the X360 32-bit layout.
#pragma once

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/Dispatch/renderablemesh.h"   // DrawIndexedParameters (16B)
#include "pc/gcm/renderengine/VertexBuffer.h"
#include "pc/gcm/renderengine/IndexBuffer.h"

namespace rw { class IResourceAllocator; namespace math { namespace fpu {
    template<class T> struct Vector3Template; } } }
namespace BrnGraphics { class Im3dSkyDome; enum EEnvironmentMapFace; }
class Texture;
class BrnShaderConstantsFrame;

// BrnSkyDomeManager.h:43
class BrnSkyDomeManager
{
public:
    void Construct();                                                   // BrnSkyDomeManager.h:47
    bool Prepare(BrnGraphics::Im3dSkyDome*, rw::IResourceAllocator*);   // BrnSkyDomeManager.h:52
    bool Release(rw::IResourceAllocator*);                             // BrnSkyDomeManager.h:56
    void Render(BrnGraphics::Im3dSkyDome*, const Texture*, const Texture*, const BrnShaderConstantsFrame*); // :63
    void RenderToEnvironmentMap(BrnGraphics::EEnvironmentMapFace, BrnGraphics::Im3dSkyDome*,
                                const Texture*, const Texture*, const BrnShaderConstantsFrame*);            // :71

private:
    // BrnSkyDomeManager.h:78
    float RaySphereDistance(const rw::math::fpu::Vector3Template<float>& lDirection,
                            const rw::math::fpu::Vector3Template<float>& lSphereCenter,
                            float lSphereRadius) const;
    // BrnSkyDomeManager.h:88
    void CreateGeometry(BrnGraphics::Im3dSkyDome* lpRenderer, rw::IResourceAllocator* lpAlloc,
                        renderengine::VertexBuffer** lppVertexBuffer,
                        renderengine::IndexBuffer** lppIndexBuffer,
                        DrawIndexedParameters* lpDrawIndexedParams,
                        int32_t liSliceCount, int32_t liSectorCount);

    renderengine::VertexBuffer* mpMainVertexBuffer;   // +0x00
    renderengine::IndexBuffer*  mpMainIndexBuffer;    // +0x04
    DrawIndexedParameters       mMainDrawParameters;  // +0x08 (16B)
    renderengine::VertexBuffer* mpCubeVertexBuffer;   // +0x18
    renderengine::IndexBuffer*  mpCubeIndexBuffer;    // +0x1C
    DrawIndexedParameters       mCubeDrawParameters;  // +0x20 (16B)
    float                       mrHorizonDistance;    // +0x30
    float                       mrZenithDistance;     // +0x34
};
