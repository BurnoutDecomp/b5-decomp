#include "GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "rw/rwcore_structs.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"

// CgsResource::RwShaderProgramBufferResourceType member functions, reconstructed from
// BURNOUT_X360_ARTIST.XEX. The runtime resource is a renderengine::ProgramBuffer block
// (ProgramBufferData header). Reconstructed here:
//   GetSerialisedResourceDescriptor  0x828A9910
//   ReBase                           0x828A8E90
//
// ReBase re-registers the compiled microcode with the GPU via the XDK entry points
// XGRegisterVertexShader / XGRegisterPixelShader (declared extern "C" in programbuffer.h;
// declared-not-defined honest externs for the PC compile-only build).

namespace CgsResource
{
    ResourceDescriptor RwShaderProgramBufferResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        // Faithful to X360 0x828A9910. The runtime resource is a renderengine::ProgramBuffer
        // block (ProgramBufferData header at +0x00). This sizes the *serialised* form: the
        // microcode + the named-variable descriptor table + every interned name string.
        const renderengine::ProgramBufferData* lpData =
            static_cast<const renderengine::ProgramBufferData*>(lpResource);

        // Seed the accumulator: slot0 = {0x20, 0x10}, slots 1..4 = identity {0, 1}.
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0].m_size      = 0x20;
        lDescriptor.m_baseResourceDescriptors[0].m_alignment = 0x10;
        for (u32 luIndex = 1; luIndex < 5; ++luIndex)
        {
            lDescriptor.m_baseResourceDescriptors[luIndex].m_size      = 0;
            lDescriptor.m_baseResourceDescriptors[luIndex].m_alignment = 1;
        }

        // Each ProgramVariableDescriptor (8-byte stride) at ProgramBufferData + microcodeSize + 0x14
        // carries a (post-fixup) name pointer at +0x00; accumulate {strlen(name)+1, 1} per name.
        const renderengine::ProgramVariableDescriptor* lpVariable =
            reinterpret_cast<const renderengine::ProgramVariableDescriptor*>(
                reinterpret_cast<const u8*>(lpData) + lpData->muMicrocodeSize + 0x14);
        const u32 luNumVariables = lpData->mu16NumVariables;
        for (u32 luIndex = 0; luIndex < luNumVariables; ++luIndex, ++lpVariable)
        {
            const char* lpName = reinterpret_cast<const char*>(lpVariable->muNameOffset);
            u32 luLength = 0;
            while (lpName[luLength] != '\0')
                ++luLength;

            ResourceDescriptor lNameEntry;
            lNameEntry.m_baseResourceDescriptors[0].m_size      = luLength + 1;
            lNameEntry.m_baseResourceDescriptors[0].m_alignment = 1;
            for (u32 luSlot = 1; luSlot < 5; ++luSlot)
            {
                lNameEntry.m_baseResourceDescriptors[luSlot].m_size      = 0;
                lNameEntry.m_baseResourceDescriptors[luSlot].m_alignment = 1;
            }
            lDescriptor += lNameEntry;
        }

        // Accumulate the descriptor table + microcode block: {numVariables*8 + microcodeSize, 0x10}.
        ResourceDescriptor lBodyEntry;
        lBodyEntry.m_baseResourceDescriptors[0].m_size      = (luNumVariables << 3) + lpData->muMicrocodeSize;
        lBodyEntry.m_baseResourceDescriptors[0].m_alignment = 0x10;
        for (u32 luSlot = 1; luSlot < 5; ++luSlot)
        {
            lBodyEntry.m_baseResourceDescriptors[luSlot].m_size      = 0;
            lBodyEntry.m_baseResourceDescriptors[luSlot].m_alignment = 1;
        }
        lDescriptor += lBodyEntry;

        // slot2 (the physical microcode part) is written directly: {microcodePart3, 0x20}.
        lDescriptor.m_baseResourceDescriptors[2].m_size      = lpData->muMicrocodePart3;
        lDescriptor.m_baseResourceDescriptors[2].m_alignment = 0x20;
        return lDescriptor;
    }

    void RwShaderProgramBufferResourceType::ReBase(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
                                                   ResourceDescriptor& /*lrSize*/, s32 liMemType) const
    {
        // Faithful to X360 0x828A8E90. Only the main-memory pass (memType 0) rebases the block;
        // any other pass is a no-op here.
        if (liMemType != 0)
            return;

        renderengine::ProgramBufferData* lpData = static_cast<renderengine::ProgramBufferData*>(lpResource);

        // Delta between the destination and source main-memory bases; added to every variable
        // name offset so the interned-name pointers survive the move.
        const s32 liDelta = reinterpret_cast<s32>(lrDest.m_baseResources[0])
                          - reinterpret_cast<s32>(lrSource.m_baseResources[0]);

        renderengine::ProgramVariableDescriptor* lpVariable =
            reinterpret_cast<renderengine::ProgramVariableDescriptor*>(
                reinterpret_cast<u8*>(lpData) + lpData->muMicrocodeSize + 0x14);
        const u32 luNumVariables = lpData->mu16NumVariables;
        for (u32 luIndex = 0; luIndex < luNumVariables; ++luIndex, ++lpVariable)
            lpVariable->muNameOffset += liDelta;

        // Re-register the shader with the GPU at its new home: vertex when shaderType == 0,
        // otherwise pixel (only TYPE_PIXEL == 1 is expected for the non-vertex case).
        if (lpData->muShaderType == 0)
        {
            XGRegisterVertexShader(reinterpret_cast<D3DVertexShader*>(reinterpret_cast<u8*>(lpData) + 0x14),
                                   reinterpret_cast<void*>(lpData->muPhysicalPart));
        }
        else
        {
            CGS_ASSERT(lpData->muShaderType == 1,
                       "lpProgramBuffer->m_type == renderengine::ProgramBuffer::TYPE_PIXEL");
            XGRegisterPixelShader(reinterpret_cast<D3DPixelShader*>(reinterpret_cast<u8*>(lpData) + 0x14),
                                  reinterpret_cast<void*>(lpData->muPhysicalPart));
        }
    }
}
