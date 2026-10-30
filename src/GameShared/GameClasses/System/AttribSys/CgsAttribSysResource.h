#pragma once

#include "types.hpp"

namespace CgsResource
{
    const s32 KI_ATTRIBSYS_VAULT_ALIGNMENT = 16;

    // Native resource payload view from DecFIGS CgsAttribSysResource.h:58-65.
    // ARTIST reads the two size fields at +4 and +0xC; the pointers occupy +0/+8.
    struct AttribSysVaultResource
    {
        u8* mpau8VltData;
        u32 muVltSizeInBytes;
        u8* mpau8BinData;
        u32 muBinSizeInBytes;
    };
}
