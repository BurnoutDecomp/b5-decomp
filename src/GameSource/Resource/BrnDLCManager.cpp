#include "GameSource/Resource/BrnDLCManager.h"
#include "GameShared/GameClasses/Core/CgsAssert.h"   // CGS_ASSERT
#include <cstddef>   // offsetof
#include <cstring>   // memset

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   BrnResource::DLCBeatTheTeamGame::SetEnabledState           @ 0x82472CA8 (caller: BrnGui::BootLegal::Update)
//   BrnResource::DLCFeatureAvailability::Construct             @ 0x82662C48
//   BrnResource::DLCFeatureAvailability::GetPackMask           @ 0x82661628
//   BrnResource::DLCFeatureAvailability::SetPackAvailabilityState @ 0x826616A0
//
// The full DLCManager / DLCDebugComponent bodies that share this translation unit
// land when those TUs are reconstructed -- add them here then.

namespace BrnResource
{
    // Never called -- pins the asm-attested member offsets of DLCFeatureAvailability.
    void DLCFeatureAvailability::_AssertLayout()
    {
        static_assert(offsetof(DLCFeatureAvailability, muAvailabilityMask) == 0x04, "mask @ +0x04");
        static_assert(offsetof(DLCFeatureAvailability, mauPackMask)        == 0x08, "pack masks @ +0x08");
        static_assert(offsetof(DLCFeatureAvailability, maePackForFeature)  == 0x1C, "feature pack @ +0x1C");
        static_assert(offsetof(DLCFeatureAvailability, mabFeatureEnabled)  == 0x80, "feature flags @ +0x80");
    }

    // X360 0x82662C48 (store-for-store). Clears the availability mask and the feature table,
    // then writes the default per-pack bit masks {1,2,4,8,16}, the default per-feature required-pack
    // indices, and marks every feature enabled. Every constant below is a baked immediate store.
    void DLCFeatureAvailability::Construct()
    {
        muAvailabilityMask = 0;                 // stw r30(0), 4(r31)

        // memset(this + 0x1C, 0, 100): zero the 25-dword feature-pack table.
        std::memset(maePackForFeature, 0, sizeof(maePackForFeature));

        // The +0x80 bool block is zeroed by a 6-dword + 1-byte loop, then set to 1 below.
        std::memset(mabFeatureEnabled, 0, sizeof(mabFeatureEnabled));

        mbField01    = false;                   // stb r30(0), 1(r31)
        mbConstructed = true;                   // stb r11(1), 0(r31)

        // Per-feature required-pack indices (stw immediates @ +0x1C..+0x7C).
        maePackForFeature[0]  = 0;
        maePackForFeature[1]  = 3;
        maePackForFeature[2]  = 1;
        maePackForFeature[3]  = 3;
        maePackForFeature[4]  = 3;
        maePackForFeature[5]  = 3;
        maePackForFeature[6]  = 4;
        maePackForFeature[7]  = 4;
        maePackForFeature[8]  = 3;
        maePackForFeature[9]  = 0;
        maePackForFeature[10] = 0;
        maePackForFeature[11] = 0;
        maePackForFeature[12] = 1;
        maePackForFeature[13] = 1;
        maePackForFeature[14] = 2;
        maePackForFeature[15] = 2;
        maePackForFeature[16] = 2;
        maePackForFeature[17] = 3;
        maePackForFeature[18] = 3;
        maePackForFeature[19] = 0;
        maePackForFeature[20] = 1;
        maePackForFeature[21] = 1;
        maePackForFeature[22] = 0;
        maePackForFeature[23] = 0;
        maePackForFeature[24] = 3;

        // Every feature defaults to enabled (stb r11(1), 0x80..0x98).
        for (int liFeature = 0; liFeature < KU_DLC_FEATURE_COUNT; ++liFeature)
        {
            mabFeatureEnabled[liFeature] = true;
        }

        // Per-pack contribution masks (stw immediates @ +0x08..+0x18).
        mauPackMask[0] = 1;
        mauPackMask[1] = 2;
        mauPackMask[2] = 4;
        mauPackMask[3] = 8;
        mauPackMask[4] = 16;
    }

    // X360 0x82661628. Returns the bit the given data pack contributes to the availability
    // mask: mauPackMask[liPack]. The asm indexes *(this + 4*(lPack+2)), i.e. mauPackMask at +0x08.
    u32 DLCFeatureAvailability::GetPackMask(s32 liPack) const
    {
        CGS_ASSERT(liPack >= E_DLC_DATA_PACK_START, "lPack >= E_DLC_DATA_PACK_START");
        CGS_ASSERT(liPack < E_DLC_DATA_PACK_COUNT,  "lPack < E_DLC_DATA_PACK_COUNT");

        return mauPackMask[liPack];
    }

    // X360 0x826616A0. When lbAvailable, OR the pack's bit into the running mask. Otherwise the asm
    // computes (mask == 0) via cntlzw/extrwi and ANDs the running mask with that 0-or-1 boolean -- so
    // clearing any real (non-zero-mask) pack zeroes the whole availability mask. Reproduced faithfully.
    void DLCFeatureAvailability::SetPackAvailabilityState(s32 liPack, bool lbAvailable)
    {
        CGS_ASSERT(liPack >= E_DLC_DATA_PACK_START, "lPack >= E_DLC_DATA_PACK_START");
        CGS_ASSERT(liPack < E_DLC_DATA_PACK_COUNT,  "lPack < E_DLC_DATA_PACK_COUNT");

        const u32 luMask = GetPackMask(liPack);
        if (lbAvailable)
        {
            muAvailabilityMask |= luMask;
        }
        else
        {
            muAvailabilityMask &= (luMask == 0u);
        }
    }
    // X360 0x82472CA8 (store-for-store). Writes mbIsEnabled (@+0x01) unconditionally, then --
    // only when enabling -- asserts that the content is actually available: the invariant is
    // "you may not enable a Beat The Team game that is not available". The X360 stores the flag
    // first (stb r4, 1(r3)) and the assert is non-fatal (it logs and returns the object).
    void DLCBeatTheTeamGame::SetEnabledState(bool lbEnabled)
    {
        mbIsEnabled = lbEnabled;

        if (lbEnabled)
        {
            CGS_ASSERT(mbIsAvailable, "!(mbIsEnabled && !mbIsAvailable)");
        }
    }
}
