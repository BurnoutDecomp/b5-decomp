#pragma once

#include "types.hpp"
#include "BrnCommonTypes.h"
#include "rw/rwcore_structs.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/vertexdescriptor.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include <cstddef>

class CgsRenderTarget;

// Native ARTIST824009B0 construction and DWARF BrnSunCorona.h:38.
struct BrnSunCorona
{
    void Construct(rw::IResourceAllocator* lpAllocator);
    void Destruct();
    void ComputeSunPositionOnScreen(Matrix44 lViewProjectionMatrix,
                                    Vector3 lViewPosition, Vector3 lSunDir);
    void GenerateOcclusionBuffer(CgsRenderTarget* lpOcclusionRt,
                                 CgsRenderTarget* lpSourceDepthRt);
    void RenderOccludedFlare(CgsRenderTarget* lpOcclusionRt, Vector3 lSunColour,
                             f32 lfWhiteLevel, f32 lfDebugOverrideBrightness,
                             bool lbDebugOverrideBrightness);

    rw::IResourceAllocator* mpAllocator;                                  // 00
    rw::Resource mVertexDescriptorResource;                               // 04
    renderengine::VertexDescriptorData* mpVertexDescriptor;               // 18
    renderengine::ProgramBufferData* mpOcclusionVertexProgram;             // 1C
    renderengine::ProgramBufferData* mpOcclusionPixelProgram;              // 20
    renderengine::ProgramVariableHandle mOcclusionPixelVariableHandleUvStartOffset; // 24
    renderengine::ProgramBufferData* mpFlareVertexProgram;                 // 28
    renderengine::ProgramBufferData* mpFlarePixelProgram;                  // 2C
    renderengine::ProgramVariableHandle mFlarePixelVariableHandleColourAndPower; // 30
    f32 mfSunVectorYMultiplier;                                          // 34
    f32 mfOcclusionSize;                                                 // 38
    f32 mfSunFlarePow;                                                   // 3C
    f32 mfSunBrightness;                                                 // 40
    f32 mfSunSize;                                                       // 44
    f32 mfXPos;                                                          // 48
    f32 mfYPos;                                                          // 4C
    bool mbVisible;                                                      // 50
    bool mbRenderSunCorona;                                             // 51
};

CGS_STATIC_ASSERT((sizeof(BrnSunCorona) == 0x54), "ARTIST sun corona");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mpVertexDescriptor) == 0x18), "ARTIST vertex descriptor");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mFlarePixelVariableHandleColourAndPower) == 0x30), "ARTIST flare handle");
CGS_STATIC_ASSERT((offsetof(BrnSunCorona, mbRenderSunCorona) == 0x51), "ARTIST render flag");
