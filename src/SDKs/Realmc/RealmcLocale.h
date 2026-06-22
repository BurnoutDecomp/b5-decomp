#pragma once

// ===========================================================================
// RealmcCore::Locale -- the Realmc vendor library's localized-string formatter.
// Sibling to the committed RealmcCore core (SDKs/Realmc/RealmcCore.h). This is
// the OWNING home for the two Locale functions the X360 binary defines:
//
//     RealmcCore::Locale::SetLocaleGetStrCallback  @ 0x82C448D8
//     RealmcCore::Locale::GetString                @ 0x82C448E8
//
// No Feb-2007 leak source and no DWARF exist for this TU; the SHAPE below is
// reconstructed from the X360 pseudocode + asm. `Realmc` is a vendor boundary,
// so its identifiers (RealmcCore, Locale, GetString, SetLocaleGetStrCallback)
// are preserved verbatim per the naming convention.
//
// HOW IT WORKS (from asm):
//   Locale formats a printf-like wide string into one of five rotating output
//   buffers. A localized *template* is fetched at format time from a callback:
//     SetLocaleGetStrCallback() installs that callback (X360 dword_832BE200).
//   GetString(pszArgSpec, ...) packs its varargs according to a per-argument
//   conversion-character string `pszArgSpec` (each byte is one of 'c','d','p',
//   's'; anything else means "skip this arg slot"), calls the callback to get
//   the current u16 template, then expands the template's "%<n>" escapes -- the
//   ASCII digit after '%' indexes the packed arg (digit-'1') -- formatting 'd'
//   slots as signed decimal and 's' slots as wide strings, while a bare '"' in
//   the template is dropped. The filled buffer pointer is returned.
//
// PLATFORM/VENDOR EXTERNS / DATA (flagged):
//   * g_pfnLocaleGetStr (X360 dword_832BE200) -- the installed template-getter
//     callback: u16* (*)(void). Installed by SetLocaleGetStrCallback; consumed
//     by GetString. Null until installed (RealmcIface::MemcardInterface
//     ::CreateInstance is the lone caller of the setter).
//   * gauLocaleStringBuffers (X360 unk_832BB9F0) -- five rotating output
//     buffers of 1024 u16 each (2048 bytes; asm strides by idx<<11 bytes).
//   * guLocaleBufferIndex (X360 dword_832BE1FC) -- the next buffer index,
//     advanced (idx+1) % 5 after each successful format.
// ===========================================================================

#include "types.hpp"

namespace RealmcCore
{
namespace Locale
{

// Signature of the localized-template getter callback (X360 dword_832BE200).
// Returns a NUL-terminated u16 template, or null when none is available.
typedef u16* (*LocaleGetStrCallback)();

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
// when the callback is unset / yields no template).
u16* GetString(const char* pszArgSpec, ...);

} // namespace Locale
} // namespace RealmcCore
