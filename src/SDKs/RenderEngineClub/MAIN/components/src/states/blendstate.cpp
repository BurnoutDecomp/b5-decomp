#include "types.hpp"

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

    class BlendState
    {
    public:
        void* GetParameters(const u32* pMaterial, void* pParams);
        void* GetResourceDescriptor(void* pOut);
        void* Initialize(void** ppMaterial, const void* pParams);
    };

    void* BlendState::GetParameters(const u32* pMaterial, void* pParams)
    {
        uintptr_t lOut = reinterpret_cast<uintptr_t>(pParams);
        auto Word = [lOut](int liOff) -> u32&  { return *reinterpret_cast<u32*>(lOut + liOff); };
        auto Byte = [lOut](int liOff) -> u8&   { return *reinterpret_cast<u8*>(lOut + liOff); };

        Byte(48) = 0;
        // Per-channel blend factors: copy and flag if any differs from the default.
        for (int liChannel = 0; liChannel < 4; ++liChannel)
        {
            Word(liChannel * 4) = pMaterial[liChannel];
            if (pMaterial[liChannel] != KU_DEFAULT_BLEND_FACTOR)
                Byte(48) = 1;
        }

        Word(20) = pMaterial[4];
        Word(24) = pMaterial[5];
        Word(28) = pMaterial[6];
        Word(32) = pMaterial[7];
        Word(36) = pMaterial[8];
        Word(44) = pMaterial[9];
        Byte(49) = pMaterial[10] != 0;
        Byte(50) = pMaterial[11] != 0;
        Byte(51) = pMaterial[12] != 0;
        Byte(52) = pMaterial[13] != 0;
        Byte(53) = pMaterial[14] != 0;
        Word(16) = pMaterial[15];
        Word(40) = pMaterial[17];
        Byte(54) = pMaterial[16] != 0;
        return const_cast<u32*>(pMaterial);
    }

    void* BlendState::GetResourceDescriptor(void* pOut)
    {
        u32* lpOut = reinterpret_cast<u32*>(pOut);
        for (int liEntry = 0; liEntry < 5; ++liEntry)
        {
            lpOut[0] = 0;
            lpOut[1] = 1;
            lpOut += 2;
        }
        u32* lpBase = reinterpret_cast<u32*>(pOut);
        lpBase[0] = 0x4C;   // 64-bit store {high=0x4C, low=4}
        lpBase[1] = 4;
        return pOut;
    }

    void* BlendState::Initialize(void** ppMaterial, const void* pParams)
    {
        u32* lpMat = reinterpret_cast<u32*>(*ppMaterial);
        uintptr_t lIn = reinterpret_cast<uintptr_t>(pParams);
        auto Word = [lIn](int liOff) -> u32 { return *reinterpret_cast<const u32*>(lIn + liOff); };
        auto Byte = [lIn](int liOff) -> u8  { return *reinterpret_cast<const u8*>(lIn + liOff); };

        if (Byte(48))
        {
            for (int liChannel = 0; liChannel < 4; ++liChannel)
                lpMat[liChannel] = Word(liChannel * 4);
        }
        else
        {
            lpMat[0] = KU_DEFAULT_BLEND_FACTOR;
            lpMat[1] = KU_DEFAULT_BLEND_FACTOR;
            lpMat[2] = KU_DEFAULT_BLEND_FACTOR;
            lpMat[3] = KU_DEFAULT_BLEND_FACTOR;
        }

        lpMat[4]  = Word(20);
        lpMat[5]  = Word(24);
        lpMat[6]  = Word(28);
        lpMat[7]  = Word(32);
        lpMat[8]  = Word(36);
        lpMat[9]  = Word(44);
        lpMat[10] = Byte(49);
        lpMat[11] = Byte(50);
        lpMat[12] = Byte(51);
        lpMat[13] = Byte(52);
        lpMat[14] = Byte(53);
        lpMat[15] = Word(16);
        lpMat[17] = Word(40);
        lpMat[18] = 1;
        lpMat[16] = Byte(54);
        return lpMat;
    }
}
