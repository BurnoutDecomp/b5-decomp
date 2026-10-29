#pragma once
#include "types.hpp"
// Native localizer: ID in r3, conversion spec in r4, varargs in r5 onward.
// SaveLoad installs a callback returning ASCII key storage. The Realmc routine
// traverses the returned storage as u16 units and copies it to rotating buffers.
namespace RealmcCore
{
namespace Locale
{

// Signature of the localized-template getter callback (X360 dword_832BE200).
// Returns key/template storage; the native formatter consumes it in u16 units.
typedef const char* (*LocaleGetStrCallback)(u32 uStringId);

// Number of rotating output buffers and their capacity (in u16) -- the asm
// rotates the index modulo 5 and strides the base by (index << 11) bytes.
const int KI_LOCALE_BUFFER_COUNT       = 5;
const int KI_LOCALE_BUFFER_CAPACITY_U16 = 1024;   // 2048 bytes per buffer

// Maximum packed-argument slots the formatter tracks (asm stack array v41 is
// 16 dwords). FLAGGED: capacity inferred from the fixed 64-byte stack slot.
const int KI_LOCALE_MAX_ARGS = 16;

// @ 0x82C448D8 -- install the localized-template getter callback. Returns the
// installed pointer (the asm returns r3 unchanged).
LocaleGetStrCallback SetLocaleGetStrCallback(LocaleGetStrCallback pfnCallback);

// @ 0x82C448E8 -- format the current localized template using varargs described
// by pszArgSpec, into the next rotating buffer; returns that buffer (or null
// when the installed callback yields no template).
u16* GetString(u32 uStringId, const char* pszArgSpec = 0, ...);

} // namespace Locale
} // namespace RealmcCore
