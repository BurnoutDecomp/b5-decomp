#pragma once

#include "GameShared/GameClasses/Graphics/CgsShaderConstantHashTable.h"
#include "GameShared/GameClasses/Graphics/CgsShaderConstants.h"

namespace CgsGraphics
{
    struct ShaderSampler;

    // DecFIGS CgsShaderTechnique.h:73. ARTIST 827EEB30 fixes the streamed
    // 32-bit image in place, including the six embedded constant blocks.
    struct ShaderTechnique
    {
        renderengine::ProgramBuffer* mpVertexProgramBuffer;
        renderengine::ProgramBuffer* mpPixelProgramBuffer;
        ShaderConstantsInternal mInternalVertexShaderConstants;
        ShaderConstantsExternal mExternalObjectVertexShaderConstants;
        ShaderConstantsExternal mExternalGlobalVertexShaderConstants;
        ShaderConstantsInternal mInternalPixelShaderConstants;
        ShaderConstantsExternal mExternalObjectPixelShaderConstants;
        ShaderConstantsExternal mExternalGlobalPixelShaderConstants;
        ShaderConstantsCPU mCPUShaderConstants;
        ShaderConstantHashTable mShaderConstantHashTable;
        ShaderSampler* mpaSamplers;
        s8 miNumSamplers;
        char* mpacName;
    };

    CGS_STATIC_ASSERT((sizeof(ShaderTechnique) == 0x98), "ARTIST shader technique");
    CGS_STATIC_ASSERT((offsetof(ShaderTechnique, mExternalObjectVertexShaderConstants) == 0x1C),
        "ARTIST object vertex constants");
    CGS_STATIC_ASSERT((offsetof(ShaderTechnique, mExternalGlobalVertexShaderConstants) == 0x2C),
        "ARTIST global vertex constants");
    CGS_STATIC_ASSERT((offsetof(ShaderTechnique, mExternalObjectPixelShaderConstants) == 0x50),
        "ARTIST object pixel constants");
    CGS_STATIC_ASSERT((offsetof(ShaderTechnique, mExternalGlobalPixelShaderConstants) == 0x60),
        "ARTIST global pixel constants");
}
