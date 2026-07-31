#pragma once

// =============================================================================
// GameSource/Graphics/ImmediateMode/BrnIm3d.h
//
// BrnGraphics::Im3dSkyDome -- the immediate-mode 3D renderer behind the sky dome.
// It is the ImRenderer<Im3dSkyDomeVertex> instantiation (bodied in
// GameShared/GameClasses/Graphics/ImmediateMode/CgsIm3dSkyDome.cpp) plus the
// sky/cloud shader-variable handle table and the clouds sampler state.
//
// Shape from the DecFIGS DWARF (GameSource/Graphics/ImmediateMode/BrnIm3d.h:50):
//     struct BrnGraphics::Im3dSkyDome : public CgsGraphics::Im3dBase<Im3dSkyDomeVertex>
//     {
//         ProgramVariableHandle[16] maStateHandles;   // BrnIm3d.h:101
//         const SamplerState *      mpCloudsSamplerState;  // BrnIm3d.h:103
//         void Construct(rw::IResourceAllocator *);        // BrnIm3d.h:56
//         void SetConstants(Vector3Plus, Vector3, Vector4, Vector4, Vector4,
//                           Vector3, const Texture *, const Texture *,
//                           Vector4 x8);                   // BrnIm3d.h:75
//     };
// The committed decomp folds CgsGraphics::Im3dBase<V> into CgsGraphics::ImRenderer<V>
// (CgsImRenderer.h) -- that template already carries the descriptor + the two program
// tables + SetTransform/Begin/EndRendering this class needs -- so the base is spelled
// ImRenderer<Im3dSkyDomeVertex> here. FLAG: Im3dBase<V> is not a separate committed
// type; if it is later split out, this base changes and nothing else does.
//
// X360 addresses (BURNOUT_X360_ARTIST.XEX):
//     BrnGraphics::Im3dSkyDome::Construct     @ 0x82406598
//     BrnGraphics::Im3dSkyDome::SetConstants  @ 0x823FFEF0
//
// The handle table's 16 slots are pinned by Construct's GetVariableHandleByName calls
// (this+0xC0 .. this+0xFC, 4 bytes apart) and consumed in the same order by
// SetConstants. They are the sky shader's named constants:
//     [ 0] +0xC0 "ViewPositionAndSkyScale"   [ 8] +0xE0 "g_scatteringCoefficients"
//     [ 1] +0xC4 "KeyLightDirAndXZLength"    [ 9] +0xE4 "g_densitySampler"
//     [ 2] +0xC8 "TopColourDrk"              [10] +0xE8 "g_lightSampler"
//     [ 3] +0xCC "HorColourPow"              [11] +0xEC "g_darkColour"
//     [ 4] +0xD0 "SunColourPow"              [12] +0xF0 "g_liteColour"
//     [ 5] +0xD4 "HorBleedSclPow"            [13] +0xF4 "g_layerCloudiness"
//     [ 6] +0xD8 "g_domeRanges"              [14] +0xF8 "g_layerInvFeather"
//     [ 7] +0xDC "g_textureScaleAndOffsets"  [15] +0xFC "g_layerAlphas"
// plus the base class's "worldViewProj" handle at +0x58 (written by SetTransform's
// shader-state path). mpCloudsSamplerState is the +0x100 store at the end of Construct.
// =============================================================================

#include "types.hpp"
#include "BrnCommonTypes.h"                                                     // Vector3 / Vector3Plus / Vector4
#include "GameShared/GameClasses/Graphics/ImmediateMode/CgsImRenderer.h"        // CgsGraphics::ImRenderer<V>
#include "GameShared/GameClasses/Graphics/VertexDescriptors/CgsIm3dSkyDomeVertex.h"  // Im3dSkyDomeVertex
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"     // renderengine::ProgramVariableHandle

namespace rw { class IResourceAllocator; }
namespace renderengine { class SamplerState; class Texture; }

namespace BrnGraphics
{
    // The number of named shader constants the sky program exposes (DWARF
    // ProgramVariableHandle[16], BrnIm3d.h:101; Construct fills all sixteen).
    const u32 KU_SKYDOME_STATE_HANDLE_COUNT = 16u;

    // BrnIm3d.h:50
    class Im3dSkyDome : public CgsGraphics::ImRenderer<Im3dSkyDomeVertex>
    {
    public:
        // BrnIm3d.h:56 -- @0x82406598. Build the renderer (one vertex/pixel program pair
        // for the sky dome), resolve the sixteen named shader constants on it, and create
        // the clouds sampler state.
        void Construct(rw::IResourceAllocator* lpAllocator);

        // BrnIm3d.h:75 -- @0x823FFEF0. Push one frame's sky/cloud constants and the two
        // cloud textures at the bound program. Parameter order is the X360 register order
        // (v1..v6 then the two texture pointers in r4/r5 then v7..v13 + one stack vector);
        // it matches the DecFIGS mangled signature exactly.
        void SetConstants(Vector3Plus lViewPositionAndSkyScale,
                          Vector3     lKeyLightDirection,
                          Vector4     lSky_TopColourDrk,
                          Vector4     lSky_HorColourPow,
                          Vector4     lSky_SunColourPow,
                          Vector3     lSky_HorBleedSclPow,
                          const renderengine::Texture* lpLayer0Density,
                          const renderengine::Texture* lpLayer0Lighting,
                          Vector4     lFogScattering,
                          Vector4     lCloudDarkColour0,
                          Vector4     lCloudLiteColour0,
                          Vector4     lCloudDomeRanges,
                          Vector4     lCloudTextureScaleAndOffsets0,
                          Vector4     lCloudLayerDensity,
                          Vector4     lCloudLayerInvFeather,
                          Vector4     lCloudLayerOpacity);

        // ADDITIVE (not an X360 symbol): expose the bound vertex descriptor object (the base
        // template's this+0x10) so BrnSkyDomeManager::Render can read the sky-dome vertex stride
        // out of it -- which is exactly what the X360 does inline (`lwz r11, 0x10(r29)`). The base
        // keeps the member protected, so the accessor lives on the derived class rather than
        // widening the shared ImRenderer<V> template.
        const void* GetVertexDescriptorData() const { return mpVertexDescriptor; }

        // ADDITIVE (not an X360 symbol): whether Construct actually got the sky-dome
        // program pair onto slot 0. The console always does (the binaries are guest
        // .data); on PC they come from the converted SkyDomeProgramsPC images, and a
        // failed adoption must skip the whole pass -- BeginRendering would otherwise
        // bind a null vertex program and shadow::Device::FlushVertexProgramState would
        // dereference it.
        bool HasPrograms() const
        {
            return mapVertexProgramBuffer[0] != 0 && mapPixelProgramBuffer[0] != 0;
        }

    private:
        // BrnIm3d.h:101 -- the sixteen named sky/cloud shader-constant handles (see the
        // banner for the slot->name map).
        renderengine::ProgramVariableHandle maStateHandles[KU_SKYDOME_STATE_HANDLE_COUNT];

        // BrnIm3d.h:103 -- the sampler state both cloud textures are bound through.
        const renderengine::SamplerState* mpCloudsSamplerState;
    };
}
