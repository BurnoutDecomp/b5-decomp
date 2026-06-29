#include "types.hpp"

#include "SDKs/RenderEngineClub/MAIN/components/src/states/blendstate.h"  // shared decl surface

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   renderengine::BlendState::GetParameters         @ 0x82B60A50
//   renderengine::BlendState::GetResourceDescriptor @ 0x82B636B8
//   renderengine::BlendState::Initialize            @ 0x82B627C8
//
// GetParameters/Initialize are an inverse pair that pack/unpack a blend-state between
// the runtime material (a 19-word block) and a params struct (`pParams`, byte-
// addressed: dword fields + boolean bytes at 48..54). The four leading words are the
// per-channel blend factors; the default value is 65537 (0x00010001) — Initialize
// writes that when the "non-default" flag (params+48) is clear, and GetParameters
// sets the flag when any factor differs from it. The boolean bytes use the X360
// count-leading-zeros idiom ((cntlzw(x)&0x20)==0  <=>  x!=0).
//
// GetResourceDescriptor builds the standard five-entry rw descriptor with the first
// pair overwritten by {size=0x4C, align=4}.

namespace renderengine
{
    namespace
    {
        const u32 KU_DEFAULT_BLEND_FACTOR = 65537;   // 0x00010001
    }

    void* BlendState::GetParameters(const BlendMaterialState* pMaterial, BlendStateParameters* pParams)
    {
        pParams->mbHasCustomBlendFactors = 0;
        for (int liChannel = 0; liChannel < 4; ++liChannel)
        {
            pParams->maBlendFactor[liChannel] = pMaterial->maState[liChannel];
            if (pMaterial->maState[liChannel] != KU_DEFAULT_BLEND_FACTOR)
                pParams->mbHasCustomBlendFactors = 1;
        }

        pParams->muState4 = pMaterial->maState[4];
        pParams->muState5 = pMaterial->maState[5];
        pParams->muState6 = pMaterial->maState[6];
        pParams->muState7 = pMaterial->maState[7];
        pParams->muState8 = pMaterial->maState[8];
        pParams->muState9 = pMaterial->maState[9];
        pParams->mbState10 = pMaterial->maState[10] != 0;
        pParams->mbState11 = pMaterial->maState[11] != 0;
        pParams->mbState12 = pMaterial->maState[12] != 0;
        pParams->mbState13 = pMaterial->maState[13] != 0;
        pParams->mbState14 = pMaterial->maState[14] != 0;
        pParams->muState15 = pMaterial->maState[15];
        pParams->muState17 = pMaterial->maState[17];
        pParams->mbState16 = pMaterial->maState[16] != 0;
        return const_cast<BlendMaterialState*>(pMaterial);
    }

    void* BlendState::GetResourceDescriptor(void* pOut)
    {
        ResourceDescriptorEntry* lpOut = static_cast<ResourceDescriptorEntry*>(pOut);
        for (ResourceDescriptorEntry* lpEntry = lpOut; lpEntry != lpOut + 5; ++lpEntry)
        {
            lpEntry->muSize = 0;
            lpEntry->muAlignment = 1;
        }
        lpOut[0].muSize = 0x4C;
        lpOut[0].muAlignment = 4;
        return pOut;
    }

    void* BlendState::GetResourceDescriptor(void* pOut, const BlendStateParameters* /*pParams*/)
    {
        return GetResourceDescriptor(pOut);
    }

    void* BlendState::Initialize(BlendMaterialState** ppMaterial, const BlendStateParameters* pParams)
    {
        BlendMaterialState* lpMat = *ppMaterial;

        if (pParams->mbHasCustomBlendFactors)
        {
            for (int liChannel = 0; liChannel < 4; ++liChannel)
                lpMat->maState[liChannel] = pParams->maBlendFactor[liChannel];
        }
        else
        {
            for (int liChannel = 0; liChannel < 4; ++liChannel)
                lpMat->maState[liChannel] = KU_DEFAULT_BLEND_FACTOR;
        }

        lpMat->maState[4]  = pParams->muState4;
        lpMat->maState[5]  = pParams->muState5;
        lpMat->maState[6]  = pParams->muState6;
        lpMat->maState[7]  = pParams->muState7;
        lpMat->maState[8]  = pParams->muState8;
        lpMat->maState[9]  = pParams->muState9;
        lpMat->maState[10] = pParams->mbState10;
        lpMat->maState[11] = pParams->mbState11;
        lpMat->maState[12] = pParams->mbState12;
        lpMat->maState[13] = pParams->mbState13;
        lpMat->maState[14] = pParams->mbState14;
        lpMat->maState[15] = pParams->muState15;
        lpMat->maState[17] = pParams->muState17;
        lpMat->maState[18] = 1;
        lpMat->maState[16] = pParams->mbState16;
        return lpMat;
    }
}
