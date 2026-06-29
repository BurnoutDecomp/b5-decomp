#pragma once

#include "types.hpp"

// renderengine / X360 system user-settings config reader.
//
// XConfigUserSettings wraps the Xbox 360 system configuration API (ExGetXConfigSetting) to read the
// per-user system settings the title needs at hardware-device bring-up. Reconstructed from
// BURNOUT_X360_ARTIST.XEX (XConfigUserSettings::GetVideoFlags @ 0x82953500); the only caller is
// D3D::InitializeHardwareDevice, which queries the system video flags to pick the display mode.
class XConfigUserSettings
{
public:
    // Read the system video-flags user setting (XConfig category 3, setting 0x0A, a 4-byte value)
    // into muVideoFlags. The X360 passes `this` as the 4-byte output buffer to ExGetXConfigSetting;
    // the return value is the system call's result code (0 on success). X360 @0x82953500.
    int GetVideoFlags();

    u32 muVideoFlags;  // +0x00 the 4-byte video-flags value the system call writes
};
