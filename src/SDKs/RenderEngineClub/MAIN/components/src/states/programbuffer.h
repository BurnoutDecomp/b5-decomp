#ifndef RENDERENGINE_PROGRAMBUFFER_H
#define RENDERENGINE_PROGRAMBUFFER_H

#include "types.hpp"
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5>

// renderengine::ProgramBuffer -- the render-engine vendor wrapper around a single compiled Xenos
// shader (vertex or pixel) plus the named-variable (constant) table extracted from its microcode.
// Declaration surface for the home (states/programbuffer.cpp) and its consumers (the post-fx
// effects that create programs and look up shader-constant handles -- DepthOfField, Tint, the
// PfxHelper program factory). The bodies + the X360 layout commentary live in programbuffer.cpp.
//
// LAYOUT (X360 asm authoritative; no DWARF/leak source exists for this vendor TU). See
// programbuffer.cpp for the full per-offset notes; the field comments below are the summary.

// --- Xenon Direct3D microcode SDK (platform externs) ----------------------------------------
// The canonical Xbox 360 D3D microcode/shader entry points the vendor renderengine links against.
// No project home models them; declared here as the minimal extern surface the bodies call.
// Signatures match the XDK xgraphics.h / d3d9 shader API.
//
// XGMICROCODE_SHADER_PARTS is the 8-word "shader parts" header XGGetMicrocodeShaderParts fills;
// only the four leading words this TU reads/copies are named (the rest is opaque padding).
struct XGMICROCODE_SHADER_PARTS
{
    u32 muPart0;  // +0x00
    u32 muSize;   // +0x04  microcode size (used to bump the descriptor base)
    u32 muPart2;  // +0x08
    u32 muPart3;  // +0x0C
};
struct D3DVertexShader;                   // opaque XDK shader objects
struct D3DPixelShader;

extern "C"
{
    void* XGGetMicrocodeShaderParts(const void* pFunction, XGMICROCODE_SHADER_PARTS* pParts);
    s32   XGMicrocodeGetConstantTable(const void* pFunction, void** ppConstantTable, u32* pSize);
    void  XGSetVertexShaderHeader(D3DVertexShader* pShader, u32 cbShaderSize, const XGMICROCODE_SHADER_PARTS* pParts);
    void  XGSetPixelShaderHeader(D3DPixelShader* pShader, u32 cbShaderSize, const XGMICROCODE_SHADER_PARTS* pParts);
    void  XGRegisterVertexShader(D3DVertexShader* pShader, void* pPhysicalPart);
    void  XGRegisterPixelShader(D3DPixelShader* pShader, void* pPhysicalPart);
    void* XMemCpy(void* pDest, const void* pSrc, u32 uCount);   // Xenon block-copy intrinsic
}

namespace renderengine
{
    // The microcode "shader parts" header is XGMICROCODE_SHADER_PARTS; Initialize and
    // GetResourceDescriptor copy it verbatim and read its size / part3 words.
    using ProgramMicrocodeParts = XGMICROCODE_SHADER_PARTS;

    struct ProgramVariableDescriptor
    {
        u32 muNameOffset;     // +0x00 name string offset (post-fixup: char*)
        u8  mu8RegisterSet;   // +0x04
        u8  mu8RegisterIndex; // +0x05
        u8  mu8RegisterCount; // +0x06
        u8  mu8Pad7;          // +0x07
    };

    // The 4-byte shader-constant handle GetVariableHandleByName fills (post-fx effects store one
    // per named constant they look up).
    struct ProgramVariableHandle
    {
        u8 mu8RegisterSet;    // +0x00 <- descriptor +0x04
        u8 mu8RegisterIndex;  // +0x01 <- descriptor +0x05
        u8 mu8ShaderType;     // +0x02 <- ProgramBufferData +0x00
        u8 mu8RegisterCount;  // +0x03 <- descriptor +0x06 (0 when not found)
    };

    struct ProgramBufferData
    {
        u32 muShaderType;     // +0x00 != 0 -> pixel, == 0 -> vertex
        u16 mu16NumVariables; // +0x04
        u16 mu16Pad6;         // +0x06
        u32 muMicrocodeSize;  // +0x08
        u32 muMicrocodePart3; // +0x0C
        u32 muPhysicalPart;   // +0x10
        // +0x14: microcode header bytes, then the ProgramVariableDescriptor[] + interned names.
    };

    struct ProgramBufferParameters
    {
        u32 muFunction;          // +0x00
        u32 muShaderType;        // +0x04
        u32 muReserved8;         // +0x08
        u32 muMicrocodePart0;    // +0x0C  inline parts (used when muFunction == 0)
        u32 muMicrocodePart1;    // +0x10  (microcode size)
        u32 muMicrocodePart2;    // +0x14
        u32 muMicrocodePart3;    // +0x18
        u32 muConstantTableSrc;  // +0x1C
        u32 muConstantTableSize; // +0x20
        u32 muNumVariables;      // +0x24
    };

    struct ProgramResourceLayout
    {
        ProgramBufferData* mpData;          // +0x00
        u32                muReserved4;     // +0x04
        void*              mpPhysicalBlock; // +0x08
    };

    static_assert(sizeof(ProgramVariableDescriptor) == 8, "ProgramVariableDescriptor must be 8 bytes");
    static_assert(sizeof(ProgramMicrocodeParts) == 16, "ProgramMicrocodeParts must be 16 bytes");

    class ProgramBuffer
    {
    public:
        static rw::BaseResourceDescriptors<5>* GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor,
                                                                     const ProgramBufferParameters* lpParams);
        static u32  GetVariableHandleByName(const ProgramBufferData* lpData, const u8* lpName,
                                            ProgramVariableHandle* lpHandle);
        static ProgramBufferData* Initialize(ProgramResourceLayout* lpLayout, const ProgramBufferParameters* lpParams);
        static void Release(ProgramBufferData* lpData);
        static u32  Xbox2CreateConstantTable(const void* lpFunction, void* lpDestTable, u32* lpTotalSize);
    };
}

#endif // RENDERENGINE_PROGRAMBUFFER_H
