#ifndef RENDERENGINE_PROGRAMBUFFER_H
#define RENDERENGINE_PROGRAMBUFFER_H

#include "types.hpp"
#include "GameShared/GameClasses/Graphics/CgsSerialisedPtr.h"
#include <xtl.h>
#include <xgraphics.h>
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programvariablehandle.h"

// renderengine::ProgramBuffer -- the render-engine vendor wrapper around a single compiled Xenos
// shader (vertex or pixel) plus the named-variable (constant) table extracted from its microcode.
// Declaration surface for the home (states/programbuffer.cpp) and its consumers (the post-fx
// effects that create programs and look up shader-constant handles -- DepthOfField, Tint, the
// PfxHelper program factory). The bodies + the X360 layout commentary live in programbuffer.cpp.
//
// LAYOUT (X360 asm authoritative; no DWARF/reference source exists for this vendor TU). See
// programbuffer.cpp for the full per-offset notes; the field comments below are the summary.

// Use the actual XDK microcode, shader and memory declarations.

namespace renderengine
{
    // The native microcode parts header has two pointers and two DWORD sizes; Initialize and
    // GetResourceDescriptor copy it verbatim and read its size / part3 words.
    typedef XGMICROCODESHADERPARTS ProgramMicrocodeParts;

    struct ProgramVariableDescriptor
    {
        CgsGraphics::Ptr32<const char> mpcName;
        u8 mu8RegisterIndex;
        u8 mu8RegisterSet;
        u8 mu8RegisterCount;
        u8 mu8Padding;
    };

    struct ProgramBufferData
    {
        s32 miShaderType;
        u16 mu16NumVariables;
        u16 mu16Pad6;
        u32 muMicrocodeSize;
        u32 muPhysicalSize;
        CgsGraphics::Ptr32<u8> mpPhysicalPart;

        ProgramVariableDescriptor* GetVariableDescriptors()
        {
            return reinterpret_cast<ProgramVariableDescriptor*>(reinterpret_cast<u8*>(this + 1) + muMicrocodeSize);
        }
        const ProgramVariableDescriptor* GetVariableDescriptors() const
        {
            return reinterpret_cast<const ProgramVariableDescriptor*>(reinterpret_cast<const u8*>(this + 1) + muMicrocodeSize);
        }
    };

    struct ProgramBufferParameters
    {
        const void* mpFunction;
        s32 miShaderType;
        u32 muProgramSize;
        ProgramMicrocodeParts mMicrocodeParts;
        const void* mpConstantTable;
        u32 muConstantTableSize;
        u32 muNumVariables;
    };

    CGS_STATIC_ASSERT((sizeof(ProgramBufferData) == 0x14), "ARTIST program buffer header");
    CGS_STATIC_ASSERT((sizeof(ProgramBufferParameters) == 0x28), "ARTIST program buffer parameters");
    CGS_STATIC_ASSERT((offsetof(ProgramBufferParameters, mMicrocodeParts) == 0x0C), "ARTIST inline microcode parts");
    CGS_STATIC_ASSERT((offsetof(ProgramBufferParameters, mpConstantTable) == 0x1C), "ARTIST prebuilt constant table");
    CGS_STATIC_ASSERT((sizeof(ProgramVariableDescriptor) == 8), "ProgramVariableDescriptor must be 8 bytes");
    CGS_STATIC_ASSERT((sizeof(ProgramMicrocodeParts) == 16), "ProgramMicrocodeParts must be 16 bytes");

    class ProgramBuffer
    {
    public:
        static rw::ResourceDescriptor GetResourceDescriptor(const ProgramBufferParameters& lrParameters);
        static bool GetVariableHandleByName(const ProgramBufferData* lpData, const u8* lpName,
                                            ProgramVariableHandle* lpHandle);
        static ProgramBufferData* Initialize(rw::Resource* lpResource, const ProgramBufferParameters* lpParams);
        static void Release(ProgramBufferData* lpData);
        static u32  Xbox2CreateConstantTable(const void* lpFunction, void* lpDestTable, u32* lpTotalSize);
    };

}

#endif // RENDERENGINE_PROGRAMBUFFER_H
