#include "types.hpp"

#include <cstring>              // memcpy
#include "rw/rwcore_structs.h"  // rw::BaseResourceDescriptors<5> + its operator+=
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"  // shared decl surface

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::ProgramBuffer::GetResourceDescriptor    @ 0x82B634B0
//   renderengine::ProgramBuffer::GetVariableHandleByName  @ 0x82B609B0
//   renderengine::ProgramBuffer::Initialize               @ 0x82B62478
//   renderengine::ProgramBuffer::Release                  @ 0x82B625F8
//   renderengine::ProgramBuffer::Xbox2CreateConstantTable @ 0x82B608A8
//
// A ProgramBuffer is the render-engine vendor wrapper around a single compiled Xenos
// shader (vertex or pixel) plus the named-variable (constant) table extracted from its
// microcode. It is the program-shaped sibling of the committed renderengine::TextureState
// (states/texturestate.cpp); like the other state objects it exposes the same
// GetResourceDescriptor / Initialize / Release surface so the rw resource system can size,
// build and tear it down, and adds GetVariableHandleByName for shader-constant lookup.
//
// LAYOUT (X360 asm authoritative; no DWARF/leak source exists for this vendor TU):
//
//   ProgramBufferData  (the runtime object built in-place at the physical resource block)
//     +0x00  u32  muShaderType   shader-kind flag: != 0 -> pixel shader, == 0 -> vertex
//     +0x04  u16  mu16NumVariables  count of named-variable descriptors
//     +0x06  u16  mu16Pad6          (zeroed)
//     +0x08  u32  muMicrocodeSize   size of the embedded microcode header block
//     +0x0C  u32  muMicrocodePart3  4th XGMICROCODE_SHADER_PARTS word (carried verbatim)
//     +0x10  u32  muPhysicalPart    pointer to the physical (GPU) microcode part
//     +0x14  ...  microcode header bytes, then the variable-descriptor array
//                 (at muPhysicalPart-relative +0x14) followed by the interned names
//
//   ProgramVariableDescriptor  (8 bytes; one per named shader constant)
//     +0x00  u32  muNameOffset  byte offset (post-fixup: char* pointer) of the name string
//     +0x04  u8   mu8RegisterSet
//     +0x05  u8   mu8RegisterIndex
//     +0x06  u8   mu8RegisterCount
//     +0x07  u8   (padding)
//
//   ProgramBufferParameters  (the unpack source, `a2`)
//     +0x00  u32  muFunction       compiled-shader function ptr; if 0 the microcode parts
//                                  are supplied inline at +0x0C..+0x18 instead
//     +0x04  u32  muShaderType     vertex/pixel flag copied to the runtime +0x00
//     +0x08  u32  muReserved8
//     +0x0C  u32  muMicrocodePart0  inline XGMICROCODE_SHADER_PARTS (used when muFunction==0)
//     +0x10  u32  muMicrocodePart1  (== microcode size)
//     +0x14  u32  muMicrocodePart2
//     +0x18  u32  muMicrocodePart3
//     +0x1C  u32  muConstantTableSrc   pre-baked variable-table blob (used when present)
//     +0x20  u32  muConstantTableSize  size of that blob (0 -> rebuild from microcode)
//     +0x24  u32  muNumVariables       descriptor count for the pre-baked blob
//
//   ProgramResourceLayout  (the `a1` handed to Initialize)
//     +0x00  ProgramBufferData*  the object slot (also the +0x00 pointer the loop seeds)
//     +0x08  void*               the physical resource block memcpy / register target
//
// Stores follow the X360 asm exactly (it overrides DWARF / Hex-Rays naming).

// The decl surface (XG microcode externs + the renderengine Program* types + ProgramBuffer class)
// lives in the shared header programbuffer.h, included above; the bodies follow.

namespace renderengine
{
    // X360 0x82B608A8.
    // Pulls the constant (named-variable) table out of the compiled microcode. For each entry it
    // appends the NUL-terminated name into lpDestTable (when non-null) and writes the 8-byte
    // descriptor {nameOffset, regSet, regIndex, regCount}. Returns the entry count; *lpTotalSize
    // receives the total bytes consumed by the appended name strings.
    u32 ProgramBuffer::Xbox2CreateConstantTable(const void* lpFunction, void* lpDestTable, u32* lpTotalSize)
    {
        u32  luTotalSize = 0;
        u32  luCount = 0;
        void* lpUCodeTable = nullptr;
        u32   luUCodeTableSize = 0;

        if (XGMicrocodeGetConstantTable(lpFunction, &lpUCodeTable, &luUCodeTableSize) >= 0 && lpUCodeTable)
        {
            u8* lpTableBase = static_cast<u8*>(lpUCodeTable);
            const u32 luNumEntries = *reinterpret_cast<const u32*>(lpTableBase + 0xC);
            if (luNumEntries)
            {
                luCount = luNumEntries;
                luTotalSize = 8 * luNumEntries;  // names are appended past the descriptor array

                u8* lpDest = static_cast<u8*>(lpDestTable);
                const u32 luArrayOffset = *reinterpret_cast<const u32*>(lpTableBase + 0x10);
                // Each microcode constant record is 0x14 bytes; field -8 (i.e. +0x0C of the record)
                // is the name's byte offset within the microcode table; +0x10 / +0x12 carry the
                // register set/index/count packed as the entry's metadata bytes.
                const u16* lpRecord = reinterpret_cast<const u16*>(lpTableBase + luArrayOffset + 8);

                u32 luRemaining = luNumEntries;
                ProgramVariableDescriptor* lpEntry = reinterpret_cast<ProgramVariableDescriptor*>(lpDest);
                do
                {
                    // asm reads the name offset as a 32-bit word at r31-8 (lwz -8(r31)).
                    const u8* lpName = lpTableBase + *reinterpret_cast<const u32*>(lpRecord - 4);
                    const u8* lpScan = lpName;
                    while (*lpScan++)
                        ;
                    const u32 luNameLen = static_cast<u32>(lpScan - lpName - 1);

                    if (lpDestTable)
                    {
                        // X360 0x82B60960: entry+0 (muNameOffset) = lpDestTable + runningTotal -- the
                        // ABSOLUTE address of the name just copied (a 32-bit pointer on X360; the 64-bit
                        // host truncates it into the u32 field, a documented platform floor, same idiom as
                        // muPhysicalPart below). The three metadata bytes are the lhz reads at -2/-4/0:
                        // entry+4 = rec[-1], entry+5 = rec[-2], entry+6 = rec[0] (asm 0x82B60964-6C).
                        lpEntry->muNameOffset     = reinterpret_cast<u32>(static_cast<u8*>(lpDestTable) + luTotalSize);
                        lpEntry->mu8RegisterSet   = static_cast<u8>(lpRecord[-1]);
                        lpEntry->mu8RegisterIndex = static_cast<u8>(lpRecord[-2]);
                        lpEntry->mu8RegisterCount = static_cast<u8>(lpRecord[0]);
                        std::memcpy(static_cast<u8*>(lpDestTable) + luTotalSize, lpName, luNameLen + 1);
                        lpTableBase = static_cast<u8*>(lpUCodeTable);  // reload (asm re-reads var_50)
                    }

                    --luRemaining;
                    luTotalSize += luNameLen + 1;
                    lpRecord += 10;  // advance 0x14 bytes (10 u16s)
                    ++lpEntry;
                }
                while (luRemaining);
            }
        }

        if (lpTotalSize)
            *lpTotalSize = luTotalSize;
        return luCount;
    }

    // X360 0x82B634B0.
    // Sizes the rw resource for a ProgramBuffer: slot0 = {microcodeSize + constTableSize + 0x14,
    // align 0x10}, slot2 = {parts[3], align 0x20}, the remaining slots {0,1}. When the params carry
    // a compiled function the constant-table size comes from Xbox2CreateConstantTable(measure-only);
    // otherwise the inline microcode words are used directly.
    rw::BaseResourceDescriptors<5>* ProgramBuffer::GetResourceDescriptor(rw::BaseResourceDescriptors<5>* lpDescriptor,
                                                                         const ProgramBufferParameters* lpParams)
    {
        u32 luConstTableSize;
        u32 luMicrocodeSize;
        u32 luPart3;

        if (lpParams->muFunction == 0)
        {
            // Inline microcode parts at params +0x0C..+0x18.
            luConstTableSize = lpParams->muConstantTableSize;  // a2[8]
            luMicrocodeSize  = lpParams->muMicrocodePart1;     // a2[4]
            luPart3          = lpParams->muMicrocodePart3;     // a2[6]
        }
        else
        {
            ProgramMicrocodeParts lParts;
            XGGetMicrocodeShaderParts(reinterpret_cast<const void*>(lpParams->muFunction), &lParts);
            u32 luTotal = 0;
            ProgramBuffer::Xbox2CreateConstantTable(reinterpret_cast<const void*>(lpParams->muFunction), nullptr, &luTotal);
            luConstTableSize = luTotal;
            luMicrocodeSize  = lParts.muSize;
            luPart3          = lParts.muPart3;
        }

        for (u32 luIndex = 0; luIndex < 5; ++luIndex)
        {
            lpDescriptor->m_baseResourceDescriptors[luIndex].m_size = 0;
            lpDescriptor->m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }

        // slot0: the whole program block (microcode header + appended constant table + 0x14 prefix).
        lpDescriptor->m_baseResourceDescriptors[0].m_size      = luMicrocodeSize + luConstTableSize + 0x14;
        lpDescriptor->m_baseResourceDescriptors[0].m_alignment = 0x10;
        // slot2: the physical microcode part.
        lpDescriptor->m_baseResourceDescriptors[2].m_size      = luPart3;
        lpDescriptor->m_baseResourceDescriptors[2].m_alignment = 0x20;
        return lpDescriptor;
    }

    // X360 0x82B62478.
    // Builds the ProgramBufferData in-place at the physical resource block. Unpacks the microcode
    // header (from a compiled function via XGGetMicrocodeShaderParts, or the inline params words),
    // installs the shader header + registers it with the GPU (vertex vs pixel by params shader-type),
    // then copies (or rebuilds) the named-variable constant table immediately past the microcode.
    ProgramBufferData* ProgramBuffer::Initialize(ProgramResourceLayout* lpLayout, const ProgramBufferParameters* lpParams)
    {
        ProgramMicrocodeParts lParts;
        if (lpParams->muFunction != 0)
        {
            XGGetMicrocodeShaderParts(reinterpret_cast<const void*>(lpParams->muFunction), &lParts);
        }
        else
        {
            lParts.muPart0 = lpParams->muMicrocodePart0;  // a2[3]
            lParts.muSize  = lpParams->muMicrocodePart1;  // a2[4]
            lParts.muPart2 = lpParams->muMicrocodePart2;  // a2[5]
            lParts.muPart3 = lpParams->muMicrocodePart3;  // a2[6]
        }

        ProgramBufferData* lpData = lpLayout->mpData;
        void*              lpPhysical = lpLayout->mpPhysicalBlock;

        lpData->muShaderType     = 0;
        lpData->mu16NumVariables = 0;
        lpData->mu16Pad6         = 0;
        lpData->muMicrocodeSize  = 0;
        lpData->muMicrocodePart3 = 0;
        lpData->muPhysicalPart   = 0;

        lpData->muShaderType     = lpParams->muShaderType;  // a2[1]
        lpData->muMicrocodeSize  = lParts.muSize;
        lpData->muMicrocodePart3 = lParts.muPart3;
        lpData->muPhysicalPart   = reinterpret_cast<u32>(lpPhysical);

        // The shader-header dest (lpData+0x14) is used only by the XGSet*/XMemCpy/XGRegister calls below.
        u8* lpHeaderDest = reinterpret_cast<u8*>(lpData) + 0x14;
        // X360 0x82B62510: memcpy(Dst = lpPhysical /*a1[2]*/, Src = (void*)lParts.muPart2, Size = lParts.muPart3).
        // (The asm computes the +0x14 header dest AFTER this copy; do NOT use it as this memcpy's dest.)
        std::memcpy(lpPhysical, reinterpret_cast<const void*>(lParts.muPart2), lParts.muPart3);

        if (lpParams->muShaderType != 0)
        {
            if (lpParams->muFunction != 0)
                XGSetPixelShaderHeader(reinterpret_cast<D3DPixelShader*>(lpHeaderDest), lParts.muSize, &lParts);
            else
                XMemCpy(lpHeaderDest, reinterpret_cast<const void*>(lParts.muPart0), lParts.muSize);  // asm Src = (void*)parts.muPart0
            XGRegisterPixelShader(reinterpret_cast<D3DPixelShader*>(lpHeaderDest), lpPhysical);
        }
        else
        {
            if (lpParams->muFunction != 0)
                XGSetVertexShaderHeader(reinterpret_cast<D3DVertexShader*>(lpHeaderDest), lParts.muSize, &lParts);
            else
                XMemCpy(lpHeaderDest, reinterpret_cast<const void*>(lParts.muPart0), lParts.muSize);  // asm Src = (void*)parts.muPart0
            XGRegisterVertexShader(reinterpret_cast<D3DVertexShader*>(lpHeaderDest), lpPhysical);
        }

        // Variable-descriptor array sits at physicalPart-relative +0x14 (past the microcode).
        u8* lpVarTable = reinterpret_cast<u8*>(lpData) + lpData->muMicrocodeSize + 0x14;
        if (lpParams->muConstantTableSize != 0)
        {
            // Pre-baked blob: copy verbatim and trust the supplied variable count.
            std::memcpy(lpVarTable, reinterpret_cast<const void*>(lpParams->muConstantTableSrc),
                        lpParams->muConstantTableSize);
            lpData->mu16NumVariables = static_cast<u16>(lpParams->muNumVariables);
        }
        else
        {
            // Rebuild from the compiled microcode.
            XGGetMicrocodeShaderParts(reinterpret_cast<const void*>(lpParams->muFunction), &lParts);
            lpData->mu16NumVariables = static_cast<u16>(
                ProgramBuffer::Xbox2CreateConstantTable(reinterpret_cast<const void*>(lpParams->muFunction),
                                                        lpVarTable, nullptr));
        }

        return lpData;
    }

    // X360 0x82B609B0.
    // Linear search of the named-variable table for a name match; on hit fills lpHandle from the
    // descriptor's register bytes plus the buffer's shader-type and returns 1, else returns 0.
    u32 ProgramBuffer::GetVariableHandleByName(const ProgramBufferData* lpData, const u8* lpName,
                                               ProgramVariableHandle* lpHandle)
    {
        u32 luIndex = 0;
        const u8* lpBase = reinterpret_cast<const u8*>(lpData) + lpData->muMicrocodeSize;
        const u32 luCount = lpData->mu16NumVariables;
        const ProgramVariableDescriptor* lpEntry =
            reinterpret_cast<const ProgramVariableDescriptor*>(lpBase + 0x14);

        if (luCount)
        {
            while (true)
            {
                const u8* lpEntryName = reinterpret_cast<const u8*>(static_cast<usize>(lpEntry->muNameOffset));
                const u8* lpName2 = lpName;
                s32 luDiff;
                while (true)
                {
                    luDiff = static_cast<s32>(*lpEntryName) - static_cast<s32>(*lpName2);
                    if (*lpEntryName == 0)
                        break;
                    ++lpEntryName;
                    ++lpName2;
                    if (luDiff != 0)
                        break;
                }
                if (luDiff == 0)
                {
                    lpHandle->mu8RegisterSet   = lpEntry->mu8RegisterSet;
                    lpHandle->mu8RegisterIndex = lpEntry->mu8RegisterIndex;
                    lpHandle->mu8ShaderType    = static_cast<u8>(lpData->muShaderType);
                    lpHandle->mu8RegisterCount = lpEntry->mu8RegisterCount;
                    return 1;
                }
                ++luIndex;
                ++lpEntry;
                if (luIndex >= luCount)
                    break;
            }
        }

        lpHandle->mu8RegisterCount = 0;
        return 0;
    }

    // X360 0x82B625F8.
    // Tears the buffer down (sub_82B62088 = the renderengine resource-release helper) and marks the
    // object freed by writing -1 into the leading shader-type word. The helper lives outside this TU.
    void ProgramBuffer::Release(ProgramBufferData* lpData)
    {
        extern void ProgramBuffer_ReleaseResource(ProgramBufferData* lpData);  // sub_82B62088 (external)
        ProgramBuffer_ReleaseResource(lpData);
        lpData->muShaderType = static_cast<u32>(-1);
    }
}
