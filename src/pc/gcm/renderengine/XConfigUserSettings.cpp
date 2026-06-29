#include "XConfigUserSettings.h"

// Reconstructed from BURNOUT_X360_ARTIST.XEX
//   XConfigUserSettings::GetVideoFlags @ 0x82953500
//
// Reads the Xbox 360 system "video flags" user setting through the kernel export ExGetXConfigSetting.
// The X360 asm passes, in order:
//   r3 = 3      Category   (XCONFIG_CATEGORY_USER)
//   r4 = 0x0A   Setting    (XCONFIG_USER_VIDEO_FLAGS)
//   r5 = this   the 4-byte output buffer (the object's muVideoFlags member)
//   r6 = 4      buffer size (a u32)
//   r7 = &      a u16 "required size written" scratch, pre-zeroed
// and returns ExGetXConfigSetting's result code in r3.

namespace
{
    // XConfig setting selectors grounded in the call's immediates (X360 0x82953524..0x8295352C).
    const u16 KU_XCONFIG_CATEGORY_USER       = 3u;     // r3
    const u16 KU_XCONFIG_USER_VIDEO_FLAGS    = 0x0Au;  // r4
    const u16 KU_XCONFIG_VIDEO_FLAGS_SIZE    = 4u;     // r6 (4-byte u32 value)
}

// The Xbox 360 kernel system-config import. [PC: not present off-console; declared so the reader
// links by name. On the X360 target this resolves to the kernel export at xboxkrnl.exe.]
extern "C" unsigned long ExGetXConfigSetting(
    unsigned short luCategory,
    unsigned short luSetting,
    void* lpSettingBuffer,
    unsigned short luSettingBufferSize,
    unsigned short* lpRequiredSizeWritten);

int XConfigUserSettings::GetVideoFlags()
{
    u16 luRequiredSize = 0;
    return static_cast<int>(ExGetXConfigSetting(
        KU_XCONFIG_CATEGORY_USER,
        KU_XCONFIG_USER_VIDEO_FLAGS,
        &muVideoFlags,
        KU_XCONFIG_VIDEO_FLAGS_SIZE,
        &luRequiredSize));
}
