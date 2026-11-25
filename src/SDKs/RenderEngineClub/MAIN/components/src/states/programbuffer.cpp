#include <cstring>
#include "SDKs/RenderEngineClub/MAIN/components/src/states/programbuffer.h"
#include "SDKs/RenderEngineClub/MAIN/components/src/device.h"
#include <d3dx9.h>

namespace renderengine
{
    // ARTIST 82B608A8 appends the names after the compact descriptor array.
    // A null destination measures the complete table without copying it.
    u32 ProgramBuffer::Xbox2CreateConstantTable(const void* lpFunction, void* lpDestTable, u32* lpTotalSize)
    {
        u32 luTotalSize = 0;
        u32 luCount = 0;
        const D3DXSHADER_CONSTANTTABLE* lpUCodeTable = 0;
        DWORD luUCodeTableSize = 0;

        if (SUCCEEDED(XGMicrocodeGetConstantTable(lpFunction, &lpUCodeTable, &luUCodeTableSize))
            && lpUCodeTable != 0 && lpUCodeTable->Constants != 0)
        {
            luCount = lpUCodeTable->Constants;
            luTotalSize = sizeof(ProgramVariableDescriptor) * luCount;
            const char* lpTable = reinterpret_cast<const char*>(lpUCodeTable);
            const D3DXSHADER_CONSTANTINFO* lpConstant =
                reinterpret_cast<const D3DXSHADER_CONSTANTINFO*>(lpTable + lpUCodeTable->ConstantInfo);
            ProgramVariableDescriptor* lpVariables = static_cast<ProgramVariableDescriptor*>(lpDestTable);
            for (u32 luIndex = 0; luIndex < luCount; ++luIndex, ++lpConstant)
            {
                const char* lpName = lpTable + lpConstant->Name;
                const u32 luNameLength = static_cast<u32>(std::strlen(lpName));
                if (lpDestTable != 0)
                {
                    ProgramVariableDescriptor* lpVariable = &lpVariables[luIndex];
                    char* lpDestName = static_cast<char*>(lpDestTable) + luTotalSize;
                    lpVariable->mpcName.muSlot = reinterpret_cast<u32>(lpDestName);
                    lpVariable->mu8RegisterSet = static_cast<u8>(lpConstant->RegisterSet);
                    lpVariable->mu8RegisterCount = static_cast<u8>(lpConstant->RegisterCount);
                    lpVariable->mu8RegisterIndex = static_cast<u8>(lpConstant->RegisterIndex);
                    std::memcpy(lpDestName, lpName, luNameLength + 1);
                    lpTable = reinterpret_cast<const char*>(lpUCodeTable);
                }
                luTotalSize += luNameLength + 1;
            }
        }

        if (lpTotalSize != 0)
            *lpTotalSize = luTotalSize;
        return luCount;
    }

    // ARTIST 82B634B0 writes a returned descriptor, with separate cached and GPU lanes.
    rw::ResourceDescriptor ProgramBuffer::GetResourceDescriptor(const ProgramBufferParameters& lrParameters)
    {
        ProgramMicrocodeParts lParts;
        u32 luConstantTableSize = 0;
        if (lrParameters.mpFunction != 0)
        {
            XGGetMicrocodeShaderParts(lrParameters.mpFunction, &lParts);
            Xbox2CreateConstantTable(lrParameters.mpFunction, 0, &luConstantTableSize);
        }
        else
        {
            lParts = lrParameters.mMicrocodeParts;
            luConstantTableSize = lrParameters.muConstantTableSize;
        }

        const u32 luProgramSize = lParts.cbCachedPartSize + luConstantTableSize + sizeof(ProgramBufferData);
        rw::ResourceDescriptor lDescriptor;
        lDescriptor.m_baseResourceDescriptors[0] = rw::BaseResourceDescriptor(luProgramSize, 16);
        lDescriptor.m_baseResourceDescriptors[2] = rw::BaseResourceDescriptor(lParts.cbPhysicalPartSize, D3DSHADER_ALIGNMENT);
        return lDescriptor;
    }

    // ARTIST 82B62478 constructs the cached header, registers the physical part
    // with the SDK, and installs the trailing named-variable table.
    ProgramBufferData* ProgramBuffer::Initialize(rw::Resource* lpResource, const ProgramBufferParameters* lpParams)
    {
        ProgramMicrocodeParts lParts;
        if (lpParams->mpFunction != 0)
            XGGetMicrocodeShaderParts(lpParams->mpFunction, &lParts);
        else
            lParts = lpParams->mMicrocodeParts;

        ProgramBufferData* lpData = static_cast<ProgramBufferData*>(lpResource->m_baseResources[0]);
        void* lpPhysical = lpResource->m_baseResources[2];
        std::memset(lpData, 0, sizeof(*lpData));
        lpData->miShaderType = lpParams->miShaderType;
        lpData->muMicrocodeSize = lParts.cbCachedPartSize;
        lpData->mpPhysicalPart.muSlot = reinterpret_cast<u32>(lpPhysical);
        lpData->mu16Pad6 = 0;
        lpData->muPhysicalSize = lParts.cbPhysicalPartSize;
        std::memcpy(lpPhysical, lParts.pPhysicalPart, lParts.cbPhysicalPartSize);

        if (lpParams->miShaderType == 0)
        {
            D3DVertexShader* lpShader = reinterpret_cast<D3DVertexShader*>(lpData + 1);
            if (lpParams->mpFunction != 0)
                XGSetVertexShaderHeader(lpShader, lParts.cbCachedPartSize, &lParts);
            else
                XMemCpy(lpShader, lParts.pCachedPart, lParts.cbCachedPartSize);
            XGRegisterVertexShader(lpShader, lpPhysical);
        }
        else
        {
            D3DPixelShader* lpShader = reinterpret_cast<D3DPixelShader*>(lpData + 1);
            if (lpParams->mpFunction != 0)
                XGSetPixelShaderHeader(lpShader, lParts.cbCachedPartSize, &lParts);
            else
                XMemCpy(lpShader, lParts.pCachedPart, lParts.cbCachedPartSize);
            XGRegisterPixelShader(lpShader, lpPhysical);
        }

        if (lpParams->muConstantTableSize != 0)
        {
            std::memcpy(lpData->GetVariableDescriptors(), lpParams->mpConstantTable, lpParams->muConstantTableSize);
            lpData->mu16NumVariables = static_cast<u16>(lpParams->muNumVariables);
        }
        else
        {
            XGGetMicrocodeShaderParts(lpParams->mpFunction, &lParts);
            u32 luTableSize;
            lpData->mu16NumVariables = static_cast<u16>(Xbox2CreateConstantTable(
                lpParams->mpFunction, lpData->GetVariableDescriptors(), &luTableSize));
        }
        return lpData;
    }

    // ARTIST 82B609B0 fills the native register handle from the named entry.
    bool ProgramBuffer::GetVariableHandleByName(const ProgramBufferData* lpData, const u8* lpName,
                                                ProgramVariableHandle* lpHandle)
    {
        const u32 luCount = lpData->mu16NumVariables;
        const ProgramVariableDescriptor* lpVariable = lpData->GetVariableDescriptors();
        for (u32 luIndex = 0; luIndex < luCount; ++luIndex, ++lpVariable)
        {
            if (std::strcmp(lpVariable->mpcName.Get(), reinterpret_cast<const char*>(lpName)) == 0)
            {
                lpHandle->mu8RegisterIndex = lpVariable->mu8RegisterIndex;
                lpHandle->mu8RegisterSet = lpVariable->mu8RegisterSet;
                lpHandle->mu8ShaderType = static_cast<u8>(lpData->miShaderType);
                lpHandle->mu8RegisterCount = lpVariable->mu8RegisterCount;
                return true;
            }
        }
        lpHandle->mu8RegisterCount = 0;
        return false;
    }

    void ProgramBuffer::Release(ProgramBufferData* lpData)
    {
        Device::UnSetResource(lpData);
        lpData->miShaderType = -1;
    }
}
