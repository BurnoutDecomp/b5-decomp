#pragma once

#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"
#include <cstddef>

// MaterialTechnique is a streamed, 40-byte resource image. Names are from
// DecFIGS CgsMaterialTechnique.h; widths/offsets are attested by ARTIST
// 827E9740, 827FBA00 and MaterialTechniqueResourceType::FixUp @828A8770.
struct ShaderConstantHandle
{
    u16 muProgramIndex;
    u8 muValueIndex;
    u8 muNumConstants;
};
static_assert(sizeof(ShaderConstantHandle) == 4, "Serialized constant binding");

namespace CgsGraphics
{
    struct ShaderTechnique;
    struct MaterialState;

    struct MaterialTechnique
    {
        Ptr32<ShaderTechnique> mpShaderTechnique;
        Ptr32<MaterialState> mpMaterialState;
        u16 mu16StateFlags;
        u16 mu16VertexProgramHash12;
        u16 mu16PixelProgramHash12;
        u16 mu16MaterialHash16;
        u16 mu16Flags2;
        u16 mu16Padding;
        u32 mNameHash;
        Ptr32<ShaderConstantHandle> mpaVertexShaderInternalConstantsHandles;
        Ptr32<ShaderConstantHandle> mpaPixelShaderInternalConstantsHandles;
        s8 mi8NumVertexShaderInternalConstants;
        s8 mi8NumPixelShaderInternalConstants;
        s8 mi8NumInternalSamplers;
        s8 mi8NumExternalSamplers;
        Ptr32<s8> mpai8SamplersIndex;
    };
    static_assert(sizeof(MaterialTechnique) == 40, "Serialized material technique");
    static_assert(offsetof(MaterialTechnique, mu16StateFlags) == 8, "Technique flags");
    static_assert(offsetof(MaterialTechnique, mpaVertexShaderInternalConstantsHandles) == 24,
                  "Technique vertex constant bindings");
}
