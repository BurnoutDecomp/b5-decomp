#pragma once

#include "types.hpp"
#include <cstddef>

struct XOVERLAPPED;

// Native pointer-bearing profile API records. ARTIST8284CF94..CFBC attests
// result.pSettings and the two40-byte X360 records. The complete XUSER_DATA
// union includes native string/binary pointers; C++ owns its x64 layout.
// This is in-process host memory, not a serialized X360 byte stream.
enum XUSER_PROFILE_SOURCE : u32
{
    XSOURCE_NO_VALUE = 0,
    XSOURCE_DEFAULT = 1,
    XSOURCE_TITLE = 2,
    XSOURCE_PERMISSION_DENIED = 3,
};

struct XUSER_DATA
{
    u8 type;
    union
    {
        s32 nData;
        s64 i64Data;
        double dblData;
        struct { u32 cbData; u16* pwszData; } string;
        float fData;
        struct { u32 cbData; u8* pbData; } binary;
        struct { u32 dwLowDateTime; u32 dwHighDateTime; } ftData;
    };
};

struct XUSER_PROFILE_SETTING
{
    XUSER_PROFILE_SOURCE source;
    union { u32 dwUserIndex; u64 xuid; } user;
    u32 dwSettingId;
    XUSER_DATA data;
};

struct XUSER_READ_PROFILE_SETTING_RESULT
{
    u32 dwSettingsLen;
    XUSER_PROFILE_SETTING* pSettings;
};

constexpr u8 XUSER_DATA_TYPE_INT32 = 1;
constexpr u32 XPROFILE_OPTION_CONTROLLER_VIBRATION = 0x10040003u;
constexpr u32 XPROFILE_GAMER_RACE_CAMERA_LOCATION = 0x10040027u;

static_assert(offsetof(XUSER_DATA, nData) == 8, "native union alignment");
static_assert(sizeof(XUSER_DATA) == (sizeof(void*) == 8 ? 24 : 16), "complete data union");
static_assert(offsetof(XUSER_PROFILE_SETTING, user) == 8, "64-bit user union");
static_assert(offsetof(XUSER_PROFILE_SETTING, dwSettingId) == 16, "setting id follows user");
static_assert(offsetof(XUSER_PROFILE_SETTING, data) == 24, "typed data member");
static_assert(sizeof(XUSER_PROFILE_SETTING) == (sizeof(void*) == 8 ? 48 : 40), "native record stride");
static_assert(offsetof(XUSER_READ_PROFILE_SETTING_RESULT, pSettings) ==
    (sizeof(void*) == 8 ? 8 : 4), "native result pointer");
static_assert(sizeof(XUSER_READ_PROFILE_SETTING_RESULT) ==
    (sizeof(void*) == 8 ? 16 : 8), "native result extent");
static_assert(sizeof(XUSER_READ_PROFILE_SETTING_RESULT) +
    2 * sizeof(XUSER_PROFILE_SETTING) <= 128, "original128-byte consumer capacity suffices");

extern "C" u32 XUserReadProfileSettings(u32 luTitleId, u32 luUserIndex,
    u32 luNumSettingIds, const u32* lpaSettingIds, u32* lpcbResults,
    XUSER_READ_PROFILE_SETTING_RESULT* lpResults, XOVERLAPPED* lpOverlapped);
