#include "GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceTypeIds.h"
#include "rw/rwcore_structs.h"
#include "GameShared/GameClasses/System/Resource/CgsResourceLoadBase.h"   // CgsResource::GetLoadBase
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
    // E_RESOURCETYPE_RW_SHADER_PROGRAM_BUFFER (see the header note).
    static const uint32_t KU_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_ID = 0x12;

    uint32_t RwShaderProgramBufferResourceType::GetTypeID() const
    {
        return KU_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_ID;
    }

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

    // Faithful to X360 0x828A8D70. The streamed program buffer's named-variable table stores
    // each name as a FILE-RELATIVE offset and its physical (GPU) microcode part as a
    // secondary-segment-relative offset; FixUp turns both into real addresses, then hands the
    // microcode to the GPU.
    //
    //   *(a2+16) += a3[2]                       muPhysicalPart += m_baseResources[2]
    //   v7 = a2 + *(a2+8) + 20                  descriptor table = data + microcodeSize + 0x14
    //   for (i < *(a2+4)) { *v7 += *a3; v7 += 2 }   muNameOffset += m_baseResources[0]
    //                                              (v7 is _DWORD*, += 2 == the 8-byte stride)
    //   then XPhysicalAlloc(microcodePart3) + memcpy + XGRegister{Vertex,Pixel}Shader(data+0x14)
    //
    // The rw::Resource segment mapping is the committed one (SmallResource::ConvertToRWResource
    // maps small[1] -> rw[2]): m_baseResources[0] is MAIN memory, [2] the GRAPHICS/secondary
    // block.
    void RwShaderProgramBufferResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        renderengine::ProgramBufferData* lpData = static_cast<renderengine::ProgramBufferData*>(lpResource);

        const u32 luMainBase      = CgsResource::GetLoadBase(lrResource);
        const u32 luSecondaryBase =
            static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));

        lpData->muPhysicalPart += luSecondaryBase;

        renderengine::ProgramVariableDescriptor* lpVariable =
            reinterpret_cast<renderengine::ProgramVariableDescriptor*>(
                reinterpret_cast<u8*>(lpData) + lpData->muMicrocodeSize + 0x14);
        const u32 luNumVariables = lpData->mu16NumVariables;
        for (u32 luIndex = 0; luIndex < luNumVariables; ++luIndex, ++lpVariable)
            lpVariable->muNameOffset += luMainBase;

        // FLAG PC-platform leaf: the X360 tail is the Xenos GPU registration --
        //     block = XPhysicalAlloc(muMicrocodePart3, -1, 32, 1028);
        //     memcpy(block, muPhysicalPart, muMicrocodePart3); muPhysicalPart = block;
        //     XGRegister{Vertex,Pixel}Shader(data + 0x14, block);
        // There is no Xenos microcode on this build: the converted platform-4 program buffer
        // carries a bare D3D9 SM3 blob at +0x14 (muMicrocodeSize == its byte length) and
        // muMicrocodePart3 == 0, i.e. no physical part to copy or register
        // (tools/assets/shaders/FORMAT_MAP.md section 5). The PC set-shader leaf
        // (D3DDevice_Set{Vertex,Pixel}Shader in pc/gcm/renderengine/XenonD3D9Shims.cpp) creates
        // the D3D9 shader object straight from data+0x14, which is exactly the pointer
        // XGRegister*Shader takes here -- so the registration is a no-op on PC and the payload
        // must stay where it is. The type assert IS kept (it is data validation, not XDK).
        CGS_ASSERT(lpData->muShaderType == 0 || lpData->muShaderType == 1,
                   "lpProgramBuffer->m_type == renderengine::ProgramBuffer::TYPE_PIXEL");
    }

    // Faithful to X360 0x828A8D00: the exact inverse of FixUp (the X360 body opens with a
    // STUB() placeholder for the un-registration leg, then de-relocates every name offset by
    // m_baseResources[0] and muPhysicalPart by m_baseResources[2]).
    void RwShaderProgramBufferResourceType::FixDown(void* lpResource, const rw::Resource& lrResource) const
    {
        renderengine::ProgramBufferData* lpData = static_cast<renderengine::ProgramBufferData*>(lpResource);

        const u32 luMainBase      = CgsResource::GetLoadBase(lrResource);
        const u32 luSecondaryBase =
            static_cast<u32>(reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]));

        renderengine::ProgramVariableDescriptor* lpVariable =
            reinterpret_cast<renderengine::ProgramVariableDescriptor*>(
                reinterpret_cast<u8*>(lpData) + lpData->muMicrocodeSize + 0x14);
        const u32 luNumVariables = lpData->mu16NumVariables;
        for (u32 luIndex = 0; luIndex < luNumVariables; ++luIndex, ++lpVariable)
            lpVariable->muNameOffset -= luMainBase;

        lpData->muPhysicalPart -= luSecondaryBase;
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
