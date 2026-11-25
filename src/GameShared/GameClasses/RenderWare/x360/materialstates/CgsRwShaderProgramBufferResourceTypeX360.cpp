#include "GameShared/GameClasses/RenderWare/x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.h"
#include "GameShared/GameClasses/Core/CgsAssertMessageAt.h"
#include <cstring>
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
// FixUp allocates GPU microcode storage. FixUp and ReBase both
// register the shader through the XDK's XGRegister shader APIs.

namespace CgsResource
{
    static const char KAC_ARTIST_SHADER_RESOURCE[] =
        "d:\\p4\\b5_main\\burnout\\main\\code\\gameshared\\gameclasses\\renderware\\x360/materialstates/CgsRwShaderProgramBufferResourceTypeX360.cpp";

    // E_RESOURCETYPE_RW_SHADER_PROGRAM_BUFFER (see the header note).
    static const uint32_t KU_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_ID = 0x12;

    uint32_t RwShaderProgramBufferResourceType::GetTypeID() const
    {
        return KU_RW_SHADER_PROGRAM_BUFFER_RESOURCE_TYPE_ID;
    }

    ResourceDescriptor RwShaderProgramBufferResourceType::GetSerialisedResourceDescriptor(const void* lpResource) const
    {
        const renderengine::ProgramBufferData* lpData = static_cast<const renderengine::ProgramBufferData*>(lpResource);
        ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor(0x20, 0x10);
        const renderengine::ProgramVariableDescriptor* lpVariable = lpData->GetVariableDescriptors();
        for (u32 luIndex = 0; luIndex < lpData->mu16NumVariables; ++luIndex, ++lpVariable)
            lDescriptor += rw::ResourceDescriptor(static_cast<u32>(std::strlen(lpVariable->mpcName.Get())) + 1, 1);

        lDescriptor += rw::ResourceDescriptor(lpData->muMicrocodeSize
            + sizeof(renderengine::ProgramVariableDescriptor) * lpData->mu16NumVariables, 16);
        lDescriptor.m_baseResourceDescriptors[2] = rw::BaseResourceDescriptor(lpData->muPhysicalSize, D3DSHADER_ALIGNMENT);
        return lDescriptor;
    }

    // ARTIST 828A8D70 resolves the serialized pointers, then gives the native
    // shader a separate GPU allocation with the XDK's required alignment.
    void RwShaderProgramBufferResourceType::FixUp(void* lpResource, const rw::Resource& lrResource) const
    {
        renderengine::ProgramBufferData* lpData = static_cast<renderengine::ProgramBufferData*>(lpResource);
        const uintptr_t luSecondaryBase = reinterpret_cast<uintptr_t>(lrResource.m_baseResources[2]);
        const u32 luMainBase = CgsResource::GetLoadBase(lrResource);
        renderengine::ProgramVariableDescriptor* lpVariable = lpData->GetVariableDescriptors();
        lpData->mpPhysicalPart.muSlot += luSecondaryBase;
        for (u32 luIndex = 0; luIndex < lpData->mu16NumVariables; ++luIndex, ++lpVariable)
            lpVariable->mpcName.muSlot += luMainBase;

        if (lpData->miShaderType == 0)
        {
            void* lpPhysical = XPhysicalAlloc(lpData->muPhysicalSize, MAXULONG_PTR, D3DSHADER_ALIGNMENT,
                PAGE_READWRITE | PAGE_WRITECOMBINE);
            std::memcpy(lpPhysical, lpData->mpPhysicalPart.Get(), lpData->muPhysicalSize);
            lpData->mpPhysicalPart.muSlot = reinterpret_cast<uintptr_t>(lpPhysical);
            XGRegisterVertexShader(reinterpret_cast<D3DVertexShader*>(lpData + 1), lpPhysical);
        }
        else
        {
            CGS_ASSERT(lpData->miShaderType == 1,
                CGS_ASSERT_MESSAGE_AT("lpProgramBuffer->m_type == renderengine::ProgramBuffer::TYPE_PIXEL",
                    KAC_ARTIST_SHADER_RESOURCE, 291));
            void* lpPhysical = XPhysicalAlloc(lpData->muPhysicalSize, MAXULONG_PTR, D3DSHADER_ALIGNMENT,
                PAGE_READWRITE | PAGE_WRITECOMBINE);
            std::memcpy(lpPhysical, lpData->mpPhysicalPart.Get(), lpData->muPhysicalSize);
            lpData->mpPhysicalPart.muSlot = reinterpret_cast<uintptr_t>(lpPhysical);
            XGRegisterPixelShader(reinterpret_cast<D3DPixelShader*>(lpData + 1), lpPhysical);
        }
    }

    // Faithful to X360 0x828A8D00: the exact inverse of FixUp (the X360 body opens with a
    // STUB() placeholder for the un-registration leg, then de-relocates every name offset by
    // m_baseResources[0] and mpPhysicalPart.muSlot by m_baseResources[2]).
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
            lpVariable->mpcName.muSlot -= luMainBase;

        lpData->mpPhysicalPart.muSlot -= luSecondaryBase;
    }

    void RwShaderProgramBufferResourceType::ReBase(void* lpResource, rw::Resource& lrSource, rw::Resource& lrDest,
                                                   ResourceDescriptor& /*lrSize*/, s32 liMemType) const
    {
        if (liMemType != 0)
            return;

        renderengine::ProgramBufferData* lpData = static_cast<renderengine::ProgramBufferData*>(lpResource);
        renderengine::ProgramVariableDescriptor* lpVariable = lpData->GetVariableDescriptors();
        const u32 luDelta = CgsResource::GetLoadBase(lrDest) - CgsResource::GetLoadBase(lrSource);
        for (u32 luIndex = 0; luIndex < lpData->mu16NumVariables; ++luIndex, ++lpVariable)
            lpVariable->mpcName.muSlot = luDelta + lpVariable->mpcName.muSlot;

        if (lpData->miShaderType == 0)
        {
            XGRegisterVertexShader(reinterpret_cast<D3DVertexShader*>(lpData + 1), lpData->mpPhysicalPart.Get());
        }
        else
        {
            CGS_ASSERT(lpData->miShaderType == 1,
                CGS_ASSERT_MESSAGE_AT("lpProgramBuffer->m_type == renderengine::ProgramBuffer::TYPE_PIXEL",
                    KAC_ARTIST_SHADER_RESOURCE, 374));
            XGRegisterPixelShader(reinterpret_cast<D3DPixelShader*>(lpData + 1), lpData->mpPhysicalPart.Get());
        }
    }
}
